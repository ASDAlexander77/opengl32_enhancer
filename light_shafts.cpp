// See light_shafts.h. Two compute dispatches sharing one SSBO: a detect pass that locates the
// light, then a shaft pass that radiates from it. No capture/blit/state-save of its own - see
// post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture and the app's GL state
// around the whole chain of stages this is one link in.
#include <cstdio>

#include "gl_loader.h"
#include "light_shafts.h"
#include "perf_stats.h"

namespace {

const unsigned int GL_TEXTURE_2D                     = 0x0DE1;
const unsigned int GL_TEXTURE0                       = 0x84C0;
const unsigned int GL_RGBA16F                        = 0x881A;
const unsigned int GL_WRITE_ONLY                     = 0x88B9;
const unsigned int GL_COMPUTE_SHADER                 = 0x91B9;
const unsigned int GL_COMPILE_STATUS                 = 0x8B81;
const unsigned int GL_LINK_STATUS                    = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT      = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT        = 0x00000400;
const unsigned int GL_SHADER_STORAGE_BARRIER_BIT     = 0x00002000;
const unsigned int GL_UNIFORM_BUFFER                 = 0x8A11;
const unsigned int GL_SHADER_STORAGE_BUFFER          = 0x90D2;
const unsigned int GL_DYNAMIC_DRAW                   = 0x88E8;
const unsigned int GL_NO_ERROR                       = 0;
const unsigned int GL_TEXTURE1                       = 0x84C1;
const unsigned int GL_TEXTURE_MIN_FILTER             = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER             = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S                 = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T                 = 0x2803;
const unsigned int GL_LINEAR                         = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE                  = 0x812F;

// How coarsely the detect pass samples the frame. The centroid of a light source does not need
// per-pixel precision - a few thousand samples locate it to well within a pixel of where it
// matters - and this keeps the atomic traffic low enough not to matter next to the shaft pass.
const int kDetectStep = 4;

// The accumulators are uint because GLSL's atomicAdd is, but a weighted centroid needs
// fractions - so positions and weights are accumulated as integers scaled by 1024 and divided
// back out in the shaft pass. That scale is spelled literally in the shaders below rather than
// shared from here, since GLSL cannot see a C++ constant.

// Pass 1: find the brightness-weighted centroid of everything above the threshold.
//
// Weighted rather than "brightest pixel": a single brightest texel is noise-sensitive and jumps
// between frames as the camera moves, which would make the shafts visibly swim. A centroid over
// everything that qualifies moves smoothly.
const char* kDetectShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(std430, binding = 1) buffer LightAccum {\n"
    "    uint sumX;\n"
    "    uint sumY;\n"
    "    uint sumW;\n"
    "};\n"
    "layout(std140, binding = 0) uniform DetectConfigBlock {\n"
    "    float threshold;\n"
    "    int step;\n"
    "    int srcWidth;\n"
    "    int srcHeight;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy) * step;\n"
    "    if (coord.x >= srcWidth || coord.y >= srcHeight) { return; }\n"
    "    vec2 uv = (vec2(coord) + vec2(0.5)) / vec2(srcWidth, srcHeight);\n"
    "    vec3 c = texture(srcTex, uv).rgb;\n"
    "    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "    if (luma <= threshold) { return; }\n"
    // Weight by how far ABOVE the threshold the pixel is, so a genuine highlight pulls the
    // centroid harder than something that only just qualifies.
    "    float w = luma - threshold;\n"
    "    uint wi = uint(w * 1024.0);\n"
    "    if (wi == 0u) { return; }\n"
    "    atomicAdd(sumX, uint(uv.x * 1024.0) * wi / 1024u);\n"
    "    atomicAdd(sumY, uint(uv.y * 1024.0) * wi / 1024u);\n"
    "    atomicAdd(sumW, wi);\n"
    "}\n";

// The shafts are built at 1/kShaftDownsample resolution and upsampled. Marching 64 taps for
// every full-resolution pixel cost 28 ms a frame on a 4000x3200 supersampled target - over a
// third of the whole frame - for an effect that is a soft, low-frequency glow by nature. At a
// quarter of each axis the march does 1/16 of the work, and it reads a small pre-thresholded
// buffer instead of the full half-float frame, which is what made the scattered taps so
// cache-hostile. Measured with debugStats.
const int kShaftDownsample = 4;

// Pass 2: the soft-kneed "light that travels" term, per quarter-resolution texel. Only light
// above the threshold contributes; everything else is an occluder and contributes nothing,
// which is what carves the shafts out around silhouettes. The knee keeps a texel barely
// crossing the threshold from switching a whole ray on. Four bilinear taps, each a 2x2
// average, cover the texel's 4x4 source block; the knee is applied per tap rather than to the
// block average so one small bright texel still counts.
const char* kBrightShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outImage;\n"
    "layout(std140, binding = 0) uniform ShaftConfigBlock {\n"
    "    float intensity;\n"
    "    float density;\n"
    "    float decay;\n"
    "    float threshold;\n"
    "};\n"
    "vec3 Contrib(vec3 c) {\n"
    "    c = min(c, vec3(4.0));\n"
    "    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "    float over = max(luma - threshold, 0.0);\n"
    "    return c * (over / (over + 0.1)) * over;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 q = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (q.x >= outSize.x || q.y >= outSize.y) { return; }\n"
    "    vec2 srcSize = vec2(textureSize(srcTex, 0));\n"
    "    vec2 block = srcSize / vec2(outSize);\n"
    "    vec3 sum = vec3(0.0);\n"
    "    for (int j = 0; j < 2; ++j) {\n"
    "        for (int i = 0; i < 2; ++i) {\n"
    "            vec2 p = (vec2(q) + vec2(0.25 + 0.5 * float(i), 0.25 + 0.5 * float(j))) * block;\n"
    "            sum += Contrib(texture(srcTex, p / srcSize).rgb);\n"
    "        }\n"
    "    }\n"
    "    imageStore(outImage, q, vec4(sum * 0.25, 1.0));\n"
    "}\n";

// Pass 3: march each quarter-resolution texel toward the detected light, accumulating what it
// passes through. The start is jittered a random fraction of a step (interleaved gradient
// noise, Jimenez 2014): with every texel starting exactly at itself, neighbours along a ray
// sampled the same texels and the frame came out as discrete hard-edged streaks - the 'laser'
// look - instead of a continuous shaft.
const char* kMarchShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D brightTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outImage;\n"
    "layout(std430, binding = 1) buffer LightAccum {\n"
    "    uint sumX;\n"
    "    uint sumY;\n"
    "    uint sumW;\n"
    "};\n"
    "layout(std140, binding = 0) uniform ShaftConfigBlock {\n"
    "    float intensity;\n"
    "    float density;\n"
    "    float decay;\n"
    "    float threshold;\n"
    "};\n"
    "const int kSteps = 64;\n"
    "float Ign(vec2 p) {\n"
    "    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 q = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (q.x >= outSize.x || q.y >= outSize.y) { return; }\n"
    "    if (sumW == 0u) { imageStore(outImage, q, vec4(0.0)); return; }\n"
    "    vec2 uv = (vec2(q) + vec2(0.5)) / vec2(outSize);\n"
    "    vec2 light = vec2(float(sumX), float(sumY)) / float(sumW);\n"
    "    vec2 delta = (light - uv) * density / float(kSteps);\n"
    "    vec2 samplePos = uv + delta * Ign(vec2(q));\n"
    "    vec3 shaft = vec3(0.0);\n"
    "    for (int i = 0; i < kSteps; ++i) {\n"
    "        shaft += texture(brightTex, samplePos).rgb;\n"
    "        samplePos += delta;\n"
    "    }\n"
    "    imageStore(outImage, q, vec4(shaft / float(kSteps), 1.0));\n"
    "}\n";

// Pass 4: add the upsampled shafts to the full-resolution frame.
//
// DECAY IS A DISTANCE FALLOFF, applied here per full-resolution pixel. It used to be a
// per-step weight, and because every pixel takes the same number of steps whatever its distance
// from the light, it could never make a far pixel dimmer than a near one. It is applied once per
// 1% of the screen height between this pixel and the light (aspect-corrected): 0.99 keeps ~37%
// at a full screen away, 0.97 ~5%, 0.9 makes a tight glow around the source. No light found,
// or zero intensity, writes the source verbatim - an exact no-op rather than a dark wash.
const char* kCompositeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(binding = 1) uniform sampler2D shaftTex;\n"
    "layout(binding = 2) uniform sampler2D hudMask;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outImage;\n"
    "layout(std430, binding = 1) buffer LightAccum {\n"
    "    uint sumX;\n"
    "    uint sumY;\n"
    "    uint sumW;\n"
    "};\n"
    "layout(std140, binding = 0) uniform ShaftConfigBlock {\n"
    "    float intensity;\n"
    "    float density;\n"
    "    float decay;\n"
    "    float threshold;\n"
    "    float useHudMask;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec4 scene = texelFetch(srcTex, outCoord, 0);\n"
    "    if (useHudMask > 0.5 && texelFetch(hudMask, outCoord, 0).r > 0.5) {\n"
    "        imageStore(outImage, outCoord, scene);\n"
    "        return;\n"
    "    }\n"
    "    if (sumW == 0u || intensity <= 0.0) {\n"
    "        imageStore(outImage, outCoord, scene);\n"
    "        return;\n"
    "    }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec2 light = vec2(float(sumX), float(sumY)) / float(sumW);\n"
    "    vec2 toLight = light - uv;\n"
    "    float dist = length(toLight * vec2(float(outSize.x) / float(outSize.y), 1.0));\n"
    "    float falloff = pow(clamp(decay, 0.0, 1.0), dist * 100.0);\n"
    "    if (falloff < 1.0 / 1024.0) {\n"
    "        imageStore(outImage, outCoord, scene);\n"
    "        return;\n"
    "    }\n"
    "    vec3 shaft = texture(shaftTex, uv).rgb;\n"
    "    vec3 result = scene.rgb + shaft * intensity * falloff;\n"
    "    if (any(isnan(result)) || any(isinf(result))) { result = scene.rgb; }\n"
    "    imageStore(outImage, outCoord, vec4(result, scene.a));\n"
    "}\n";

struct DetectConfigData {
    float threshold;
    int step;
    int srcWidth;
    int srcHeight;
};

struct ShaftConfigData {
    float intensity;
    float density;
    float decay;
    float threshold;
    float useHudMask;   // read by the composite pass only
    float pad[3];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int detectProgram = 0;
    unsigned int brightProgram = 0;
    unsigned int marchProgram = 0;
    unsigned int compositeProgram = 0;
    // Quarter-resolution intermediates, rebuilt when the frame size changes.
    unsigned int brightTex = 0;
    unsigned int shaftTex = 0;
    int lowWidth = 0;
    int lowHeight = 0;
    unsigned int detectUbo = 0;
    unsigned int shaftUbo = 0;
    unsigned int accumSsbo = 0;
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
        printf("[opengl32_enh_cpp] lightshafts: FAILED to compile %s compute shader: %s\n",
               label, log);
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
        printf("[opengl32_enh_cpp] lightshafts: FAILED to link %s compute program: %s\n",
               label, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

unsigned int MakeLowResTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return texture;
}

}  // namespace

bool ApplyLightShafts(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                       float intensity, float density, float decay, float threshold,
                       unsigned int hudMaskTexture) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] lightshafts: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    // Cached programs/buffers belong to the GL context that built them. If that context is gone,
    // drop the handles rather than deleting them (the owning context freed them already, and
    // glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = PipelineState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk =
            CompileAndLink(gl, kDetectShaderSource, "detect", g_state.detectProgram) &&
            CompileAndLink(gl, kBrightShaderSource, "bright", g_state.brightProgram) &&
            CompileAndLink(gl, kMarchShaderSource, "march", g_state.marchProgram) &&
            CompileAndLink(gl, kCompositeShaderSource, "composite", g_state.compositeProgram);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] lightshafts: shader init failed, effect disabled until "
                   "the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (g_state.detectUbo == 0) {
        gl.glGenBuffers(1, &g_state.detectUbo);
        gl.glGenBuffers(1, &g_state.shaftUbo);
        gl.glGenBuffers(1, &g_state.accumSsbo);
    }

    // Zeroing the accumulators every frame is what makes the detected light position describe
    // THIS frame rather than every frame since the context was created.
    unsigned int zeros[3] = {0, 0, 0};
    gl.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_state.accumSsbo);
    gl.glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(zeros), zeros, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_state.accumSsbo);

    DetectConfigData detectData{};
    detectData.threshold = threshold;
    detectData.step = kDetectStep;
    detectData.srcWidth = width;
    detectData.srcHeight = height;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.detectUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(detectData), &detectData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.detectUbo);

    gl.glUseProgram(g_state.detectProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    int detectWidth = (width + kDetectStep - 1) / kDetectStep;
    int detectHeight = (height + kDetectStep - 1) / kDetectStep;
    gl.glDispatchCompute((unsigned int)((detectWidth + 7) / 8),
                          (unsigned int)((detectHeight + 7) / 8), 1);
    // The shaft pass reads what the detect pass just wrote, so the barrier between them is not
    // optional - without it the centroid it reads is whatever happened to have landed.
    gl.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    PerfGpuMark("lightshafts detect");

    ShaftConfigData shaftData{};
    shaftData.intensity = intensity;
    shaftData.density = density;
    shaftData.decay = decay;
    shaftData.threshold = threshold;
    shaftData.useHudMask = hudMaskTexture != 0 ? 1.0f : 0.0f;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.shaftUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(shaftData), &shaftData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.shaftUbo);

    int lowWidth = (width + kShaftDownsample - 1) / kShaftDownsample;
    int lowHeight = (height + kShaftDownsample - 1) / kShaftDownsample;
    if (g_state.lowWidth != lowWidth || g_state.lowHeight != lowHeight) {
        if (g_state.brightTex != 0) {
            unsigned int old[2] = {g_state.brightTex, g_state.shaftTex};
            gl.glDeleteTextures(2, old);
        }
        g_state.brightTex = MakeLowResTexture(gl, lowWidth, lowHeight);
        g_state.shaftTex = MakeLowResTexture(gl, lowWidth, lowHeight);
        g_state.lowWidth = lowWidth;
        g_state.lowHeight = lowHeight;
    }
    unsigned int lowGroupsX = (unsigned int)((lowWidth + 7) / 8);
    unsigned int lowGroupsY = (unsigned int)((lowHeight + 7) / 8);

    gl.glUseProgram(g_state.brightProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(0, g_state.brightTex, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(lowGroupsX, lowGroupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    PerfGpuMark("lightshafts bright");

    gl.glUseProgram(g_state.marchProgram);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.brightTex);
    gl.glBindImageTexture(0, g_state.shaftTex, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(lowGroupsX, lowGroupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    PerfGpuMark("lightshafts march");

    gl.glUseProgram(g_state.compositeProgram);
    gl.glActiveTexture(GL_TEXTURE1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.shaftTex);
    if (hudMaskTexture != 0) {
        gl.glActiveTexture(GL_TEXTURE1 + 1);
        gl.glBindTexture(GL_TEXTURE_2D, hudMaskTexture);
    }
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute((unsigned int)((width + 7) / 8), (unsigned int)((height + 7) / 8), 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] lightshafts: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
