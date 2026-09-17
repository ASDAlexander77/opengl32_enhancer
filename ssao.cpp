// See ssao.h. Two compute dispatches: an AO pass into an owned intermediate, then a depth-aware
// blur that composites the result into the color in the same pass. No capture/blit/state-save of
// its own - see post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture/
// depthTexture and the app's GL state around the whole chain of stages this is one link in.
#include <cstdio>

#include "ssao.h"
#include "gl_loader.h"

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

// Both passes unproject raw (non-linear, hardware) depth back to a positive eye-space distance,
// and the AO pass goes further to a full view-space position. Keeping left/right/bottom/top
// separate rather than assuming a symmetric field of view means an off-center frustum (which
// glFrustum permits, and some engines use for split/offset views) unprojects correctly instead
// of skewing everything toward one side. View space here puts the camera at the origin looking
// down -z, so a position's z is negative while `eyeDist` is the positive distance along the
// view axis.
const char* kSsaoShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D depthTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D aoImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"   // left, right, bottom, top
    "    vec4 params;\n"    // zNear, zFar, radius, bias
    "    vec4 params2;\n"   // intensity, depthSigma, unused, unused
    "};\n"
    "float LinearEyeDistance(float rawDepth, float zNear, float zFar) {\n"
    "    float ndc = rawDepth * 2.0 - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    "vec3 ViewPosFor(vec2 uv, float eyeDist, vec4 frustum, float zNear) {\n"
    "    float x = (frustum.x + (frustum.y - frustum.x) * uv.x) * (eyeDist / zNear);\n"
    "    float y = (frustum.z + (frustum.w - frustum.z) * uv.y) * (eyeDist / zNear);\n"
    "    return vec3(x, y, -eyeDist);\n"
    "}\n"
    // The classic ShaderX/Crysis sample set. It is a full sphere, not a hemisphere, so z is
    // taken as abs() below to fold every sample onto the normal's side - which is also why the
    // set does not need to be regenerated to stay correctly oriented.
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
    "vec3 ViewPosAt(ivec2 coord, ivec2 size, vec4 frustum, float zNear, float zFar) {\n"
    "    ivec2 c = clamp(coord, ivec2(0), size - ivec2(1));\n"
    "    float raw = texelFetch(depthTex, c, 0).r;\n"
    "    vec2 uv = (vec2(c) + vec2(0.5)) / vec2(size);\n"
    "    return ViewPosFor(uv, LinearEyeDistance(raw, zNear, zFar), frustum, zNear);\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(aoImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= size.x || outCoord.y >= size.y) { return; }\n"
    "    float zNear = params.x;\n"
    "    float zFar = params.y;\n"
    "    float radius = params.z;\n"
    "    float bias = params.w;\n"
    "    float intensity = params2.x;\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(size);\n"
    "    float rawDepth = texelFetch(depthTex, outCoord, 0).r;\n"
    // Nothing was drawn here (cleared depth / sky): there is no surface to occlude, and
    // unprojecting the far plane would put the sample kernel kilometres away.
    "    if (rawDepth >= 1.0) {\n"
    "        imageStore(aoImage, outCoord, vec4(1.0));\n"
    "        return;\n"
    "    }\n"
    "    float eyeDist = LinearEyeDistance(rawDepth, zNear, zFar);\n"
    "    vec3 P = ViewPosFor(uv, eyeDist, frustum, zNear);\n"
    // Normals are reconstructed from neighbouring positions, picking whichever of the forward/
    // backward difference spans the smaller depth step on each axis. Taking the nearer neighbour
    // keeps a silhouette's normal attached to the surface it belongs to instead of tilting it
    // toward whatever distant geometry happens to sit on the other side of the edge.
    "    vec3 Pr = ViewPosAt(outCoord + ivec2(1, 0), size, frustum, zNear, zFar);\n"
    "    vec3 Pl = ViewPosAt(outCoord - ivec2(1, 0), size, frustum, zNear, zFar);\n"
    "    vec3 Pu = ViewPosAt(outCoord + ivec2(0, 1), size, frustum, zNear, zFar);\n"
    "    vec3 Pd = ViewPosAt(outCoord - ivec2(0, 1), size, frustum, zNear, zFar);\n"
    "    vec3 dx = (abs(Pr.z - P.z) < abs(P.z - Pl.z)) ? (Pr - P) : (P - Pl);\n"
    "    vec3 dy = (abs(Pu.z - P.z) < abs(P.z - Pd.z)) ? (Pu - P) : (P - Pd);\n"
    "    vec3 N = cross(dx, dy);\n"
    "    float nLen = length(N);\n"
    // A degenerate cross product means the four neighbours were collinear (or the surface is
    // exactly edge-on), leaving no usable normal - report "unoccluded" rather than normalizing
    // a zero vector into NaN and poisoning the blur with it.
    "    if (nLen < 1e-8) {\n"
    "        imageStore(aoImage, outCoord, vec4(1.0));\n"
    "        return;\n"
    "    }\n"
    "    N /= nLen;\n"
    "    if (dot(N, P) > 0.0) { N = -N; }\n"
    // Rotate the kernel per pixel so the 16 samples trade visible banding for high-frequency
    // noise, which the blur pass then removes. Building the basis from an axis chosen against N
    // (rather than rotating a fixed in-plane vector) keeps it well-conditioned for surfaces
    // seen edge-on, where the naive construction collapses.
    "    float angle = Hash(vec2(outCoord)) * 6.2831853;\n"
    "    vec3 up = (abs(N.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);\n"
    "    vec3 T0 = normalize(cross(up, N));\n"
    "    vec3 B0 = cross(N, T0);\n"
    "    vec3 T = T0 * cos(angle) + B0 * sin(angle);\n"
    "    vec3 B = cross(N, T);\n"
    "    mat3 TBN = mat3(T, B, N);\n"
    "    float occlusion = 0.0;\n"
    "    for (int i = 0; i < 16; ++i) {\n"
    "        vec3 k = kKernel[i];\n"
    "        k.z = abs(k.z);\n"
    "        vec3 samplePos = P + (TBN * k) * radius;\n"
    "        if (samplePos.z > -zNear) { continue; }\n"
    "        float sampleDist = -samplePos.z;\n"
    "        float xn = samplePos.x * zNear / sampleDist;\n"
    "        float yn = samplePos.y * zNear / sampleDist;\n"
    "        vec2 sampleUv = vec2((xn - frustum.x) / (frustum.y - frustum.x),\n"
    "                             (yn - frustum.z) / (frustum.w - frustum.z));\n"
    "        if (any(lessThan(sampleUv, vec2(0.0))) || any(greaterThan(sampleUv, vec2(1.0)))) { continue; }\n"
    "        float sampleRaw = texture(depthTex, sampleUv).r;\n"
    "        if (sampleRaw >= 1.0) { continue; }\n"
    "        float sceneDist = LinearEyeDistance(sampleRaw, zNear, zFar);\n"
    // Occluded when real geometry sits in front of where this sample landed. The range check
    // stops a wall metres behind the pixel from counting as a contact occluder just because it
    // happens to line up in screen space.
    "        if (sceneDist < sampleDist - bias) {\n"
    "            float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(eyeDist - sceneDist), 1e-4));\n"
    "            occlusion += rangeCheck;\n"
    "        }\n"
    "    }\n"
    "    float ao = 1.0 - (occlusion / float(16)) * intensity;\n"
    "    imageStore(aoImage, outCoord, vec4(ao, ao, ao, 1.0));\n"
    "}\n";

// Depth-aware blur plus composite in one pass. A 16-sample kernel is far too noisy to show
// directly, and a plain blur would smear the AO across depth discontinuities - weighting each
// tap by how close its linear depth is to the centre's keeps occlusion from bleeding between
// a foreground object and the background behind it.
const char* kBlurShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D aoTex;\n"
    "layout(binding = 1) uniform sampler2D colorTex;\n"
    "layout(binding = 2) uniform sampler2D depthTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform SsaoConfigBlock {\n"
    "    vec4 frustum;\n"
    "    vec4 params;\n"
    "    vec4 params2;\n"
    "};\n"
    "float LinearEyeDistance(float rawDepth, float zNear, float zFar) {\n"
    "    float ndc = rawDepth * 2.0 - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= size.x || outCoord.y >= size.y) { return; }\n"
    "    float zNear = params.x;\n"
    "    float zFar = params.y;\n"
    "    float depthSigma = params2.y;\n"
    "    float centerDist = LinearEyeDistance(texelFetch(depthTex, outCoord, 0).r, zNear, zFar);\n"
    "    float sum = 0.0;\n"
    "    float weightSum = 0.0;\n"
    "    for (int y = -2; y <= 2; ++y) {\n"
    "        for (int x = -2; x <= 2; ++x) {\n"
    "            ivec2 c = clamp(outCoord + ivec2(x, y), ivec2(0), size - ivec2(1));\n"
    "            float dist = LinearEyeDistance(texelFetch(depthTex, c, 0).r, zNear, zFar);\n"
    "            float w = exp(-abs(dist - centerDist) / max(depthSigma, 1e-4));\n"
    "            sum += texelFetch(aoTex, c, 0).r * w;\n"
    "            weightSum += w;\n"
    "        }\n"
    "    }\n"
    "    float ao = (weightSum > 1e-4) ? (sum / weightSum) : 1.0;\n"
    "    vec4 color = texelFetch(colorTex, outCoord, 0);\n"
    "    imageStore(outputImage, outCoord, vec4(color.rgb * ao, color.a));\n"
    "}\n";

struct SsaoConfigData {
    float frustum[4];
    float params[4];
    float params2[4];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int ssaoProgram = 0;
    unsigned int blurProgram = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int aoTexture = 0;
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

unsigned int CreateWorkTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return texture;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        gl.glDeleteTextures(1, &g_state.aoTexture);
        g_state.texturesValid = false;
    }

    g_state.aoTexture = CreateWorkTexture(gl, width, height);
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
                float radius, float intensity, float bias) {
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
                          CompileAndLink(gl, kBlurShaderSource, "blur", g_state.blurProgram);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] ssao: shader init failed, effect disabled until the "
                   "GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    EnsureTextures(gl, width, height);
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
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SsaoConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);

    gl.glUseProgram(g_state.ssaoProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glBindImageTexture(0, g_state.aoTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    // The blur pass samples aoTexture as a texture, so the image writes above have to land
    // before it runs - this barrier is what orders the two dispatches, not an optimization.
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    gl.glUseProgram(g_state.blurProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.aoTexture);
    gl.glActiveTexture(GL_TEXTURE1);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE2);
    gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] ssao: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
