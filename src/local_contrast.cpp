// See local_contrast.h. One separable dense-Gaussian blur program (reused for both layers,
// sigma set per dispatch) plus a downsample and a composite pass.
//
// Sequence, per layer: downsample(luma(src)) -> final -> H-blur -> temp -> V-blur -> final, the
// structure layer at half of each axis and the tone layer at a quarter; composite reads src +
// both blurred lumas, upsampled bilinearly. Both layers start from the
// ORIGINAL srcTexture, not chained off each other, so structureStrength and toneStrength are
// independent - raising one doesn't change what the other measures (see local_contrast.h).
//
// Cost: the composite only ever uses the blurs' LUMA, so they carry one R16F channel instead
// of RGBA16F (blur(luma) == luma(blur), luma being linear). And the tone layer's sigma of 13.6
// texels at full resolution was ~43 taps per direction over every pixel - 11 ms a frame on a
// 4000x3200 supersampled target. A blur that wide has nothing above a quarter of the sampling
// rate, so it is built at 1/4 resolution with sigma 3.4 (the same reach) and upsampled
// bilinearly by the composite: 1/16 of the pixels and a third of the taps. The structure
// layer (sigma 3.4) is built the same way at 1/2 resolution. Measured with debugStats on a
// 4000x3200 target: 11.8 ms before.
//
// No capture/blit/state-save of its own - see post_effects.cpp for the shared pipeline that
// owns srcTexture/dstTexture and the app's GL state around the whole chain of stages this is
// one link in. Only the blur intermediates (own, resized with width/height) belong here.
#include <cstdio>

#include "local_contrast.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_TEXTURE_MIN_FILTER        = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER        = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S            = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T            = 0x2803;
const unsigned int GL_LINEAR                    = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE             = 0x812F;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_R16F                      = 0x822D;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_NO_ERROR                  = 0;

// Fixed radius multipliers for the two blur passes - not config-exposed, same precedent as
// bloom.cpp's Gaussian sigma/weights (only the effect's STRENGTH is a tunable, not its
// underlying blur shape). 2 texels apart is tight enough to isolate fine texture; 8 is wide
// enough to separate broad midtone regions for a real "clarity" feel.
const float kStructureRadiusScale = 2.0f;
const float kToneRadiusScale = 8.0f;
// Each layer is built at a fraction of each axis - see the header comment. The structure
// layer's sigma of 3.4 texels leaves nothing above a quarter of the sampling rate, so half
// resolution loses nothing it keeps; the tone layer's 13.6 allows a quarter.
const int kStructureDownsample = 2;
const int kToneDownsample = 4;

// params.x is the blur direction (>0.5 = horizontal, else vertical), params.y the radius scale
// (sigma = 1.7 * scale texels), params.z > 0.5 when srcTex is the colour frame (take its luma)
// rather than an already-luma R16F texture.
const char* kBlurShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(r16f, binding = 1) uniform writeonly image2D dstImage;\n"
    "layout(std140, binding = 0) uniform LocalContrastBlurBlock {\n"
    "    vec4 params;\n"  // .x = direction, .y = radiusScale, .z = source is colour
    "};\n"
    "float Luma(vec2 uv) {\n"
    "    vec4 c = texture(srcTex, uv);\n"
    "    float l = params.z > 0.5 ? dot(c.rgb, vec3(0.2126, 0.7152, 0.0722)) : c.r;\n"
    "    return (isnan(l) || isinf(l)) ? 0.0 : l;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(dstImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 texelSize = 1.0 / vec2(outSize);\n"
    "    vec2 dir = params.x > 0.5 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) * texelSize;\n"
    // A DENSE Gaussian whose sigma scales with radiusScale - not the fixed 9-tap kernel with its
    // taps pushed radiusScale texels apart, which is what this used to be. At the tone layer's
    // scale of 8 that kernel sampled one texel in eight: not a blur but a comb, so every edge
    // came back as a row of faint copies 8, 16, 24 and 32 texels away, and subtracting that
    // from the image (the unsharp mask) printed those echoes onto the frame - the artifacts.
    // sigma 1.7 matches the old kernel's own spread at scale 1, so both layers keep their reach.
    // Taps are taken in pairs at the weighted midpoint of two texels, letting the bilinear filter
    // do half the work - the standard linear-sampling trick.
    "    float sigma = 1.7 * max(params.y, 0.5);\n"
    "    int radius = min(int(ceil(3.0 * sigma)), 48);\n"
    "    float twoSigmaSq = 2.0 * sigma * sigma;\n"
    "    float sum = Luma(uv);\n"
    "    float total = 1.0;\n"
    "    for (int i = 1; i <= radius; i += 2) {\n"
    "        float w1 = exp(-float(i * i) / twoSigmaSq);\n"
    "        float w2 = (i + 1 <= radius) ? exp(-float((i + 1) * (i + 1)) / twoSigmaSq) : 0.0;\n"
    "        float w = w1 + w2;\n"
    "        float o = (float(i) * w1 + float(i + 1) * w2) / w;\n"
    "        vec2 offset = dir * texelSize * o;\n"
    "        sum += (Luma(uv + offset) + Luma(uv - offset)) * w;\n"
    "        total += 2.0 * w;\n"
    "    }\n"
    "    imageStore(dstImage, outCoord, vec4(sum / total));\n"
    "}\n";

// Luma of each factor x factor block of the colour frame (factor = params.w, 2 or 4), as
// bilinear taps that each land on the shared corner of a 2x2 group and so average it - one tap
// for a 2x2 block, four for a 4x4 one: the whole block exactly.
const char* kDownsampleShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(r16f, binding = 1) uniform writeonly image2D dstImage;\n"
    "layout(std140, binding = 0) uniform LocalContrastBlurBlock {\n"
    "    vec4 params;\n"  // .w = downsample factor
    "};\n"
    "float Luma(vec2 p, vec2 srcSize) {\n"
    "    float l = dot(texture(srcTex, p / srcSize).rgb, vec3(0.2126, 0.7152, 0.0722));\n"
    "    return (isnan(l) || isinf(l)) ? 0.0 : l;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(dstImage);\n"
    "    ivec2 q = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (q.x >= outSize.x || q.y >= outSize.y) { return; }\n"
    "    vec2 srcSize = vec2(textureSize(srcTex, 0));\n"
    "    int factor = int(params.w);\n"
    "    vec2 base = vec2(q) * float(factor);\n"
    "    float l = 0.0;\n"
    "    int n = 0;\n"
    "    for (int y = 1; y < factor; y += 2) {\n"
    "        for (int x = 1; x < factor; x += 2) {\n"
    "            l += Luma(base + vec2(float(x), float(y)), srcSize);\n"
    "            n++;\n"
    "        }\n"
    "    }\n"
    "    imageStore(dstImage, q, vec4(l / float(max(n, 1))));\n"
    "}\n";

const char* kCompositeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D origTex;\n"
    "layout(binding = 1) uniform sampler2D structureBlurTex;\n"
    "layout(binding = 2) uniform sampler2D toneBlurTex;\n"
    "layout(rgba16f, binding = 3) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform LocalContrastCompositeBlock {\n"
    "    vec4 params;\n"  // .x = structureStrength, .y = toneStrength, .zw unused
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 orig = texture(origTex, uv);\n"
    "    vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);\n"
    "    float lumaOrig = dot(orig.rgb, lumaWeights);\n"
    "    if (isnan(lumaOrig) || isinf(lumaOrig)) { lumaOrig = 0.0; }\n"
    // The layers are 1/2 and 1/4 size, rounded up, so map by the factor rather than stretching
    // each over the frame: texel k of a layer covers full-res texels [k*f, (k+1)*f).
    "    vec2 p = vec2(outCoord) + vec2(0.5);\n"
    "    float lumaStructureBlur = texture(structureBlurTex, p / (2.0 * vec2(textureSize(structureBlurTex, 0)))).r;\n"
    "    float lumaToneBlur = texture(toneBlurTex, p / (4.0 * vec2(textureSize(toneBlurTex, 0)))).r;\n"
    "    float structureDetail = lumaOrig - lumaStructureBlur;\n"
    "    float toneDetail = lumaOrig - lumaToneBlur;\n"
    "    float delta = structureDetail * params.x + toneDetail * params.y;\n"
    "    vec3 result = clamp(orig.rgb + vec3(delta), 0.0, 1.0);\n"
    "    imageStore(outputImage, outCoord, vec4(result, orig.a));\n"
    "}\n";

struct BlurConfigData {
    float direction;
    float radiusScale;
    float sourceIsColour;
    float downsampleFactor;   // read by the downsample pass only
};

struct CompositeConfigData {
    float structureStrength;
    float toneStrength;
    float pad[2];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int blurProgram = 0;
    unsigned int downsampleProgram = 0;
    unsigned int compositeProgram = 0;
    unsigned int blurUbo = 0;
    unsigned int compositeUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    int structureWidth = 0;
    int structureHeight = 0;
    int smallWidth = 0;
    int smallHeight = 0;
    unsigned int blurTempTexture = 0;        // 1/kStructureDownsample res, R16F
    unsigned int structureFinalTexture = 0;  // 1/kStructureDownsample res, R16F
    unsigned int toneTempTexture = 0;        // 1/kToneDownsample res, R16F
    unsigned int toneFinalTexture = 0;       // 1/kToneDownsample res, R16F
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* source, const char* label, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &source, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] local_contrast: FAILED to compile '%s' compute shader: %s\n", label, log);
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
        printf("[opengl32_enh_cpp] local_contrast: FAILED to link '%s' compute program: %s\n", label, log);
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
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16F, width, height);
    return texture;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        unsigned int textures[4] = {g_state.blurTempTexture, g_state.structureFinalTexture,
                                    g_state.toneTempTexture, g_state.toneFinalTexture};
        gl.glDeleteTextures(4, textures);
        g_state.texturesValid = false;
    }

    g_state.structureWidth = (width + kStructureDownsample - 1) / kStructureDownsample;
    g_state.structureHeight = (height + kStructureDownsample - 1) / kStructureDownsample;
    g_state.smallWidth = (width + kToneDownsample - 1) / kToneDownsample;
    g_state.smallHeight = (height + kToneDownsample - 1) / kToneDownsample;
    g_state.blurTempTexture = CreateWorkTexture(gl, g_state.structureWidth, g_state.structureHeight);
    g_state.structureFinalTexture = CreateWorkTexture(gl, g_state.structureWidth, g_state.structureHeight);
    g_state.toneTempTexture = CreateWorkTexture(gl, g_state.smallWidth, g_state.smallHeight);
    g_state.toneFinalTexture = CreateWorkTexture(gl, g_state.smallWidth, g_state.smallHeight);

    g_state.width = width;
    g_state.height = height;
    g_state.texturesValid = true;
}

void EnsureUbos(const GlComputeApi& gl) {
    if (g_state.blurUbo == 0) {
        gl.glGenBuffers(1, &g_state.blurUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.blurUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BlurConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
    if (g_state.compositeUbo == 0) {
        gl.glGenBuffers(1, &g_state.compositeUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.compositeUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CompositeConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
}

unsigned int GroupCount(int extent) {
    return (unsigned int)((extent + 7) / 8);
}

void RunBlurPass(const GlComputeApi& gl, unsigned int srcTex, unsigned int dstTex,
                  float direction, float radiusScale, bool sourceIsColour, int width, int height) {
    BlurConfigData data{};
    data.direction = direction;
    data.radiusScale = radiusScale;
    data.sourceIsColour = sourceIsColour ? 1.0f : 0.0f;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.blurUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BlurConfigData), &data, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.blurUbo);

    gl.glUseProgram(g_state.blurProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTex);
    gl.glBindImageTexture(1, dstTex, 0, 0, 0, GL_WRITE_ONLY, GL_R16F);
    gl.glDispatchCompute(GroupCount(width), GroupCount(height), 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

void RunDownsamplePass(const GlComputeApi& gl, unsigned int srcTex, unsigned int dstTex,
                       int factor, int dstWidth, int dstHeight) {
    BlurConfigData data{};
    data.downsampleFactor = (float)factor;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.blurUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BlurConfigData), &data, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.blurUbo);

    gl.glUseProgram(g_state.downsampleProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTex);
    gl.glBindImageTexture(1, dstTex, 0, 0, 0, GL_WRITE_ONLY, GL_R16F);
    gl.glDispatchCompute(GroupCount(dstWidth), GroupCount(dstHeight), 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

}  // namespace

bool ApplyLocalContrast(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                         float structureStrength, float toneStrength) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] local_contrast: GL 4.3 compute support unavailable on "
                   "this context, effect disabled\n");
            warned = true;
        }
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
        bool ok = true;
        ok &= CompileAndLink(gl, kBlurShaderSource, "blur", g_state.blurProgram);
        ok &= CompileAndLink(gl, kDownsampleShaderSource, "downsample", g_state.downsampleProgram);
        ok &= CompileAndLink(gl, kCompositeShaderSource, "composite", g_state.compositeProgram);
        g_state.initOk = ok;
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] local_contrast: shader init failed, effect disabled "
                   "until the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureTextures(gl, width, height);
    EnsureUbos(gl);

    unsigned int groupsX = GroupCount(width);
    unsigned int groupsY = GroupCount(height);

    // Each layer: the ORIGINAL srcTexture's luma (the two are not chained off each other),
    // box-downsampled, then blurred at that size with the sigma scaled down by the same factor -
    // the same reach in full-resolution texels.
    RunDownsamplePass(gl, srcTexture, g_state.structureFinalTexture, kStructureDownsample,
                      g_state.structureWidth, g_state.structureHeight);
    const float structureSmallScale = kStructureRadiusScale / (float)kStructureDownsample;
    RunBlurPass(gl, g_state.structureFinalTexture, g_state.blurTempTexture, 1.0f, structureSmallScale,
                false, g_state.structureWidth, g_state.structureHeight);
    RunBlurPass(gl, g_state.blurTempTexture, g_state.structureFinalTexture, 0.0f, structureSmallScale,
                false, g_state.structureWidth, g_state.structureHeight);

    RunDownsamplePass(gl, srcTexture, g_state.toneFinalTexture, kToneDownsample,
                      g_state.smallWidth, g_state.smallHeight);
    const float toneSmallScale = kToneRadiusScale / (float)kToneDownsample;
    RunBlurPass(gl, g_state.toneFinalTexture, g_state.toneTempTexture, 1.0f, toneSmallScale, false,
                g_state.smallWidth, g_state.smallHeight);
    RunBlurPass(gl, g_state.toneTempTexture, g_state.toneFinalTexture, 0.0f, toneSmallScale, false,
                g_state.smallWidth, g_state.smallHeight);

    // Composite: srcTexture + both detail layers * their strengths -> dstTexture.
    CompositeConfigData compositeData{};
    compositeData.structureStrength = structureStrength;
    compositeData.toneStrength = toneStrength;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.compositeUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CompositeConfigData), &compositeData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.compositeUbo);

    gl.glUseProgram(g_state.compositeProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.structureFinalTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.toneFinalTexture);
    gl.glBindImageTexture(3, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] local_contrast: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
