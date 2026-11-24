// See ssao.h. Two compute dispatches: an AO pass into an owned intermediate, then a depth-aware
// blur that composites the result into the color in the same pass. No capture/blit/state-save of
// its own - see post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture/
// depthTexture and the app's GL state around the whole chain of stages this is one link in.
#include <cstdio>

#include "ssao.h"
#include "gl_loader.h"
#include "perf_stats.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_TEXTURE1                  = 0x84C1;
const unsigned int GL_TEXTURE2                  = 0x84C2;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_TEXTURE_MIN_FILTER        = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER        = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S            = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T            = 0x2803;
const unsigned int GL_NEAREST                   = 0x2600;
const unsigned int GL_LINEAR                    = 0x2601;
const unsigned int GL_NEAREST_MIPMAP_NEAREST    = 0x2700;
const unsigned int GL_TEXTURE_MAX_LEVEL         = 0x813D;
const unsigned int GL_R16F                      = 0x822D;
const unsigned int GL_R32F                      = 0x822E;

// Mip levels of the linear-depth texture the AO taps can fall back to. Five is enough for a
// tap 2^(5+3) = 256 AO texels away to read a texel spacing comparable to its own distance.
const int kMaxDepthLevels = 5;

// AO is a soft, low-frequency term, and on a supersampled target (4000x3200 in Anachronox) the
// full-resolution AO + 5x5 bilateral blur cost 31 ms a frame - the single most expensive stage,
// measured with debugStats. Above this many pixels the AO and its blur run on a grid one half
// the size on each axis (a quarter of the work) and the result is upsampled into the colour.
// Below it nothing changes: at scale 1 every pass reads exactly the texels it always did.
const long long kHalfResAoAbovePixels = 2500000;
const unsigned int GL_CLAMP_TO_EDGE             = 0x812F;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_NO_ERROR                  = 0;

// Linear depth, once, on the AO grid. Every later pass used to fetch the 24-bit hardware depth
// at full resolution and unproject it per tap - 16 scattered taps plus 4 neighbours per AO
// texel, and 25 more per blur texel. Converting once into a small R32F texture, with a mip
// chain the AO taps can read coarser levels of, is what makes the AO pass cache-friendly. The
// sky (raw depth 1.0) linearizes to exactly zFar, which every reader treats as "no surface".
//
// View space puts the camera at the origin looking down -z, so a position's z is negative
// while `eyeDist` is the positive distance along the view axis. Keeping left/right/bottom/top
// separate rather than assuming a symmetric field of view means an off-center frustum
// unprojects correctly instead of skewing everything toward one side.
const char* kLinearizeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D depthTex;\n"
    "layout(r32f, binding = 0) uniform writeonly image2D linearImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"
    "    vec4 params;\n"
    "    vec4 params2;\n"
    "    vec4 params3;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 size = imageSize(linearImage);\n"
    "    ivec2 q = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (q.x >= size.x || q.y >= size.y) { return; }\n"
    "    ivec2 depthSize = textureSize(depthTex, 0);\n"
    "    int scale = int(params2.z);\n"
    "    ivec2 c = min(q * scale + ivec2(scale / 2), depthSize - ivec2(1));\n"
    "    float zNear = params.x;\n"
    "    float zFar = params.y;\n"
    "    float ndc = texelFetch(depthTex, c, 0).r * 2.0 - 1.0;\n"
    "    float eyeDist = (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "    imageStore(linearImage, q, vec4(eyeDist));\n"
    "}\n";

// The AO itself. Normals are reconstructed from neighbouring positions, picking whichever of
// the forward/backward difference spans the smaller depth step on each axis, so a silhouette's
// normal stays attached to its own surface. The kernel is the classic ShaderX/Crysis full
// sphere, folded onto the normal's side with abs(z), and rotated per texel so its 16 samples
// trade banding for noise the blur removes. The range check stops a wall metres behind the
// pixel from counting as a contact occluder just because it lines up in screen space.
const char* kSsaoShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D linearDepth;\n"
    "layout(r16f, binding = 0) uniform writeonly image2D aoImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"
    "    vec4 params;\n"
    "    vec4 params2;\n"
    "    vec4 params3;\n"
    "};\n"
    "const vec3 kKernel[16] = vec3[16](\n"
    "    vec3( 0.5381,  0.1856,  0.4319), vec3( 0.1379,  0.2486,  0.4430),\n"
    "    vec3( 0.3371,  0.5679,  0.0057), vec3(-0.6999, -0.0451,  0.0019),\n"
    "    vec3( 0.0689, -0.1598,  0.8547), vec3( 0.0560,  0.0069,  0.1843),\n"
    "    vec3(-0.0146,  0.1402,  0.0762), vec3( 0.0100, -0.1924, -0.0344),\n"
    "    vec3(-0.3577, -0.5301, -0.4358), vec3(-0.3169,  0.1063,  0.0158),\n"
    "    vec3( 0.0103, -0.5869,  0.0046), vec3(-0.0897, -0.4940,  0.3287),\n"
    "    vec3( 0.7119, -0.0154, -0.0918), vec3(-0.0533,  0.0596, -0.5411),\n"
    "    vec3( 0.0352, -0.0631,  0.5460), vec3(-0.4776,  0.2847, -0.0271)\n"
    ");\n"
    "float Hash(vec2 p) {\n"
    "    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
    "}\n"
    "vec3 ViewPosFor(vec2 uv, float eyeDist, float zNear) {\n"
    "    float x = (frustum.x + (frustum.y - frustum.x) * uv.x) * (eyeDist / zNear);\n"
    "    float y = (frustum.z + (frustum.w - frustum.z) * uv.y) * (eyeDist / zNear);\n"
    "    return vec3(x, y, -eyeDist);\n"
    "}\n"
    "vec2 UvFor(ivec2 q, ivec2 aoSize, ivec2 fullSize, int scale) {\n"
    "    ivec2 c = min(q * scale + ivec2(scale / 2), fullSize - ivec2(1));\n"
    "    return (vec2(c) + vec2(0.5)) / vec2(fullSize);\n"
    "}\n"
    "vec3 ViewPosAt(ivec2 q, ivec2 aoSize, ivec2 fullSize, int scale, float zNear) {\n"
    "    ivec2 c = clamp(q, ivec2(0), aoSize - ivec2(1));\n"
    "    float d = texelFetch(linearDepth, c, 0).r;\n"
    "    return ViewPosFor(UvFor(c, aoSize, fullSize, scale), d, zNear);\n"
    "}\n"
    "void main() {\n"
    "    ivec2 aoSize = imageSize(aoImage);\n"
    "    ivec2 aoCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (aoCoord.x >= aoSize.x || aoCoord.y >= aoSize.y) { return; }\n"
    "    int scale = int(params2.z);\n"
    "    ivec2 fullSize = ivec2(params3.xy);\n"
    "    float zNear = params.x;\n"
    "    float zFar = params.y;\n"
    "    float radius = params.z;\n"
    "    float bias = params.w;\n"
    "    float intensity = params2.x;\n"
    "    float maxLod = params2.w;\n"
    "    float eyeDist = texelFetch(linearDepth, aoCoord, 0).r;\n"
    "    if (eyeDist >= zFar * 0.999) {\n"
    "        imageStore(aoImage, aoCoord, vec4(1.0));\n"
    "        return;\n"
    "    }\n"
    "    vec2 uv = UvFor(aoCoord, aoSize, fullSize, scale);\n"
    "    vec3 P = ViewPosFor(uv, eyeDist, zNear);\n"
    "    vec3 Pr = ViewPosAt(aoCoord + ivec2(1, 0), aoSize, fullSize, scale, zNear);\n"
    "    vec3 Pl = ViewPosAt(aoCoord - ivec2(1, 0), aoSize, fullSize, scale, zNear);\n"
    "    vec3 Pu = ViewPosAt(aoCoord + ivec2(0, 1), aoSize, fullSize, scale, zNear);\n"
    "    vec3 Pd = ViewPosAt(aoCoord - ivec2(0, 1), aoSize, fullSize, scale, zNear);\n"
    "    vec3 dx = (abs(Pr.z - P.z) < abs(P.z - Pl.z)) ? (Pr - P) : (P - Pl);\n"
    "    vec3 dy = (abs(Pu.z - P.z) < abs(P.z - Pd.z)) ? (Pu - P) : (P - Pd);\n"
    "    vec3 N = cross(dx, dy);\n"
    "    float nLen = length(N);\n"
    "    if (nLen < 1e-8) {\n"
    "        imageStore(aoImage, aoCoord, vec4(1.0));\n"
    "        return;\n"
    "    }\n"
    "    N /= nLen;\n"
    "    if (dot(N, P) > 0.0) { N = -N; }\n"
    "    float angle = Hash(vec2(aoCoord)) * 6.2831853;\n"
    "    vec3 up = (abs(N.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);\n"
    "    vec3 T0 = normalize(cross(up, N));\n"
    "    vec3 B0 = cross(N, T0);\n"
    "    vec3 T = T0 * cos(angle) + B0 * sin(angle);\n"
    "    vec3 B = cross(N, T);\n"
    "    mat3 TBN = mat3(T, B, N);\n"
    "    float occlusion = 0.0;\n"
    // Interleaved sampling: each texel takes 8 of the 16 kernel taps - the first half or the
    // second, in a checkerboard - and the blur below averages neighbours from both halves back
    // into a 16-tap estimate. Halves the cost of the most expensive SSAO pass.
    "    int firstTap = ((aoCoord.x + aoCoord.y) & 1) * 8;\n"
    "    for (int t = 0; t < 8; ++t) {\n"
    "        vec3 k = kKernel[firstTap + t];\n"
    "        k.z = abs(k.z);\n"
    "        vec3 samplePos = P + (TBN * k) * radius;\n"
    "        if (samplePos.z > -zNear) { continue; }\n"
    "        float sampleDist = -samplePos.z;\n"
    "        float xn = samplePos.x * zNear / sampleDist;\n"
    "        float yn = samplePos.y * zNear / sampleDist;\n"
    "        vec2 sampleUv = vec2((xn - frustum.x) / (frustum.y - frustum.x),\n"
    "                             (yn - frustum.z) / (frustum.w - frustum.z));\n"
    "        if (any(lessThan(sampleUv, vec2(0.0))) || any(greaterThan(sampleUv, vec2(1.0)))) { continue; }\n"
    "        float offsetTexels = length((sampleUv - uv) * vec2(aoSize));\n"
    "        float lod = clamp(floor(log2(max(offsetTexels, 1.0))) - 3.0, 0.0, maxLod);\n"
    "        float sceneDist = textureLod(linearDepth, sampleUv, lod).r;\n"
    "        if (sceneDist >= zFar * 0.999) { continue; }\n"
    "        if (sceneDist < sampleDist - bias) {\n"
    "            float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(eyeDist - sceneDist), 1e-4));\n"
    "            occlusion += rangeCheck;\n"
    "        }\n"
    "    }\n"
    "    float ao = 1.0 - (occlusion / float(8)) * intensity;\n"
    "    imageStore(aoImage, aoCoord, vec4(ao));\n"
    "}\n";

// Depth-aware blur, on the AO grid. An 8-sample estimate is far too noisy to show directly, and
// a plain blur would smear the AO across depth discontinuities - weighting each tap by how
// close its linear depth is to the centre's keeps occlusion from bleeding between a foreground
// object and the background behind it. Separable: a 5-tap horizontal pass then a 5-tap
// vertical one (10 taps instead of the 25 of the 2D kernel it replaced). Either pass alone
// spans both checkerboard phases of the AO pass's interleaved taps, so the two halves of the
// 16-tap kernel are recombined here.
const char* kBlurHShaderSource =
    "#version 430\n"
    "const ivec2 kDir = ivec2(1, 0);\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D aoTex;\n"
    "layout(binding = 2) uniform sampler2D linearDepth;\n"
    "layout(r16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"
    "    vec4 params;\n"
    "    vec4 params2;\n"
    "    vec4 params3;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= size.x || outCoord.y >= size.y) { return; }\n"
    "    float depthSigma = params2.y;\n"
    "    float centerDist = texelFetch(linearDepth, outCoord, 0).r;\n"
    "    float sum = 0.0;\n"
    "    float weightSum = 0.0;\n"
    "    for (int i = -2; i <= 2; ++i) {\n"
    "        ivec2 c = clamp(outCoord + kDir * i, ivec2(0), size - ivec2(1));\n"
    "        float dist = texelFetch(linearDepth, c, 0).r;\n"
    "        float w = exp(-abs(dist - centerDist) / max(depthSigma, 1e-4));\n"
    "        sum += texelFetch(aoTex, c, 0).r * w;\n"
    "        weightSum += w;\n"
    "    }\n"
    "    float ao = (weightSum > 1e-4) ? (sum / weightSum) : 1.0;\n"
    "    imageStore(outputImage, outCoord, vec4(ao));\n"
    "}\n";

const char* kBlurVShaderSource =
    "#version 430\n"
    "const ivec2 kDir = ivec2(0, 1);\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D aoTex;\n"
    "layout(binding = 2) uniform sampler2D linearDepth;\n"
    "layout(r16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"
    "    vec4 params;\n"
    "    vec4 params2;\n"
    "    vec4 params3;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= size.x || outCoord.y >= size.y) { return; }\n"
    "    float depthSigma = params2.y;\n"
    "    float centerDist = texelFetch(linearDepth, outCoord, 0).r;\n"
    "    float sum = 0.0;\n"
    "    float weightSum = 0.0;\n"
    "    for (int i = -2; i <= 2; ++i) {\n"
    "        ivec2 c = clamp(outCoord + kDir * i, ivec2(0), size - ivec2(1));\n"
    "        float dist = texelFetch(linearDepth, c, 0).r;\n"
    "        float w = exp(-abs(dist - centerDist) / max(depthSigma, 1e-4));\n"
    "        sum += texelFetch(aoTex, c, 0).r * w;\n"
    "        weightSum += w;\n"
    "    }\n"
    "    float ao = (weightSum > 1e-4) ? (sum / weightSum) : 1.0;\n"
    "    imageStore(outputImage, outCoord, vec4(ao));\n"
    "}\n";


// Composite: multiply the (bilinearly upsampled) blurred AO into the colour. At scale 1 every
// sample lands on a texel centre, so this is exactly a per-pixel multiply.
const char* kCompositeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D aoTex;\n"
    "layout(binding = 1) uniform sampler2D colorTex;\n"
    "layout(binding = 2) uniform sampler2D hudMask;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"
    "    vec4 params;\n"
    "    vec4 params2;\n"
    "    vec4 params3;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= size.x || outCoord.y >= size.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(size);\n"
    "    float ao = texture(aoTex, uv).r;\n"
    "    vec4 color = texelFetch(colorTex, outCoord, 0);\n"
    "    if (params3.z > 0.5 && texelFetch(hudMask, outCoord, 0).r > 0.5) {\n"
    "        imageStore(outputImage, outCoord, color);\n"
    "        return;\n"
    "    }\n"
    "    imageStore(outputImage, outCoord, vec4(color.rgb * ao, color.a));\n"
    "}\n";

struct SsaoConfigData {
    float frustum[4];
    float params[4];
    float params2[4];
    float params3[4];   // full-resolution width, height, useHudMask (composite), unused
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int ssaoProgram = 0;
    unsigned int blurProgram = 0;
    unsigned int blurVProgram = 0;
    unsigned int compositeProgram = 0;
    unsigned int linearizeProgram = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int aoTexture = 0;
    unsigned int aoBlurTexture = 0;
    unsigned int aoTempTexture = 0;
    unsigned int linearDepthTexture = 0;
    int depthLevels = 1;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* source, const char* label,
                     unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &source, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] ssao: FAILED to compile %s compute shader: %s\n", label, log);
        gl.glDeleteShader(shader);
        return false;
    }

    unsigned int program = gl.glCreateProgram();
    gl.glAttachShader(program, shader);
    gl.glLinkProgram(program);
    gl.glDeleteShader(shader);

    int linked = 0;
    gl.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048];
        int logLen = 0;
        gl.glGetProgramInfoLog(program, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] ssao: FAILED to link %s compute program: %s\n", label, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

unsigned int CreateWorkTexture(const GlComputeApi& gl, int width, int height,
                               unsigned int filter = GL_NEAREST,
                               unsigned int format = GL_R16F, int levels = 1) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)filter);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                       (int)(filter == GL_NEAREST_MIPMAP_NEAREST ? GL_NEAREST : filter));
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
    gl.glTexStorage2D(GL_TEXTURE_2D, levels, format, width, height);
    return texture;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        gl.glDeleteTextures(1, &g_state.aoTexture);
        gl.glDeleteTextures(1, &g_state.aoBlurTexture);
        gl.glDeleteTextures(1, &g_state.aoTempTexture);
        gl.glDeleteTextures(1, &g_state.linearDepthTexture);
        g_state.texturesValid = false;
    }

    int levels = 1;
    while (levels < kMaxDepthLevels && ((width >> levels) > 0 || (height >> levels) > 0)) {
        ++levels;
    }
    g_state.depthLevels = levels;
    g_state.linearDepthTexture = CreateWorkTexture(gl, width, height, GL_NEAREST_MIPMAP_NEAREST,
                                                   GL_R32F, levels);

    g_state.aoTexture = CreateWorkTexture(gl, width, height);
    g_state.aoTempTexture = CreateWorkTexture(gl, width, height);
    // Sampled bilinearly by the composite, which is what upsamples it.
    g_state.aoBlurTexture = CreateWorkTexture(gl, width, height, GL_LINEAR);
    g_state.width = width;
    g_state.height = height;
    g_state.texturesValid = true;
}

void EnsureUbo(const GlComputeApi& gl) {
    if (g_state.configUbo != 0) {
        return;
    }
    gl.glGenBuffers(1, &g_state.configUbo);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SsaoConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplySsao(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
                int width, int height, const ProjectionParams& projection,
                float radius, float intensity, float bias, unsigned int hudMaskTexture) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] ssao: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (depthTexture == 0 || width <= 0 || height <= 0) {
        return false;
    }

    // Without a captured projection there is no way to turn raw depth into the view-space
    // positions this stage works in - see projection_capture.h. Refusing here (rather than
    // substituting a guessed near/far) keeps a wrong-looking AO from being mistaken for a
    // tuning problem.
    if (projection.zNear <= 0.0f || projection.zFar <= projection.zNear) {
        return false;
    }

    // Cached programs/textures belong to the GL context that built them. If that context is
    // gone, drop the handles rather than deleting them (the owning context freed them already,
    // and glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = PipelineState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, kSsaoShaderSource, "ssao", g_state.ssaoProgram) &&
                          CompileAndLink(gl, kBlurHShaderSource, "blur", g_state.blurProgram) &&
                          CompileAndLink(gl, kBlurVShaderSource, "blur v", g_state.blurVProgram) &&
                          CompileAndLink(gl, kCompositeShaderSource, "composite",
                                         g_state.compositeProgram) &&
                          CompileAndLink(gl, kLinearizeShaderSource, "linearize",
                                         g_state.linearizeProgram);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] ssao: shader init failed, effect disabled until the "
                   "GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    int scale = ((long long)width * height > kHalfResAoAbovePixels) ? 2 : 1;
    int aoWidth = (width + scale - 1) / scale;
    int aoHeight = (height + scale - 1) / scale;
    EnsureTextures(gl, aoWidth, aoHeight);
    EnsureUbo(gl);

    SsaoConfigData configData{};
    configData.frustum[0] = projection.left;
    configData.frustum[1] = projection.right;
    configData.frustum[2] = projection.bottom;
    configData.frustum[3] = projection.top;
    configData.params[0] = projection.zNear;
    configData.params[1] = projection.zFar;
    configData.params[2] = radius;
    configData.params[3] = bias;
    configData.params2[0] = intensity;
    // Blur taps more than a quarter of the occlusion radius away in depth are treated as a
    // different surface. Tying this to the radius rather than fixing it keeps the blur's notion
    // of "same surface" in step with the AO's own scale when radius is retuned per game.
    configData.params2[1] = radius * 0.25f;
    configData.params2[2] = (float)scale;
    configData.params2[3] = (float)(g_state.depthLevels - 1);
    configData.params3[0] = (float)width;
    configData.params3[1] = (float)height;
    configData.params3[2] = hudMaskTexture != 0 ? 1.0f : 0.0f;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SsaoConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    unsigned int aoGroupsX = (unsigned int)((aoWidth + 7) / 8);
    unsigned int aoGroupsY = (unsigned int)((aoHeight + 7) / 8);

    gl.glUseProgram(g_state.linearizeProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glBindImageTexture(0, g_state.linearDepthTexture, 0, 0, 0, GL_WRITE_ONLY, GL_R32F);
    gl.glDispatchCompute(aoGroupsX, aoGroupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.linearDepthTexture);
    gl.glGenerateMipmap(GL_TEXTURE_2D);
    PerfGpuMark("ssao depth");

    gl.glUseProgram(g_state.ssaoProgram);
    gl.glBindImageTexture(0, g_state.aoTexture, 0, 0, 0, GL_WRITE_ONLY, GL_R16F);
    gl.glDispatchCompute(aoGroupsX, aoGroupsY, 1);
    PerfGpuMark("ssao ao");
    // Each pass samples the previous one's image as a texture, so the writes have to land
    // before it runs - these barriers order the dispatches, they are not an optimization.
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    gl.glUseProgram(g_state.blurProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.aoTexture);
    gl.glActiveTexture(GL_TEXTURE2);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.linearDepthTexture);
    gl.glBindImageTexture(0, g_state.aoTempTexture, 0, 0, 0, GL_WRITE_ONLY, GL_R16F);
    gl.glDispatchCompute(aoGroupsX, aoGroupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    gl.glUseProgram(g_state.blurVProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.aoTempTexture);
    gl.glBindImageTexture(0, g_state.aoBlurTexture, 0, 0, 0, GL_WRITE_ONLY, GL_R16F);
    gl.glDispatchCompute(aoGroupsX, aoGroupsY, 1);
    PerfGpuMark("ssao blur");
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    gl.glUseProgram(g_state.compositeProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.aoBlurTexture);
    gl.glActiveTexture(GL_TEXTURE1);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    if (hudMaskTexture != 0) {
        gl.glActiveTexture(GL_TEXTURE2);
        gl.glBindTexture(GL_TEXTURE_2D, hudMaskTexture);
    }
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute((unsigned int)((width + 7) / 8), (unsigned int)((height + 7) / 8), 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    PerfGpuMark("ssao composite");

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] ssao: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
