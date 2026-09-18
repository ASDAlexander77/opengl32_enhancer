// See light_shafts.h. Two compute dispatches sharing one SSBO: a detect pass that locates the
// light, then a shaft pass that radiates from it. No capture/blit/state-save of its own - see
// post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture and the app's GL state
// around the whole chain of stages this is one link in.
#include <cstdio>

#include "gl_loader.h"
#include "light_shafts.h"

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

// Pass 2: march each pixel toward the detected light, accumulating what it passes through.
const char* kShaftShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
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
    "const int kSteps = 32;\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 scene = texture(srcTex, uv);\n"
    // Nothing above the threshold anywhere in the frame: no light, so no shafts. Writing the
    // source verbatim keeps this an exact no-op rather than a very dark wash.
    "    if (sumW == 0u || intensity <= 0.0) {\n"
    "        imageStore(outImage, outCoord, scene);\n"
    "        return;\n"
    "    }\n"
    "    vec2 light = vec2(float(sumX), float(sumY)) / float(sumW);\n"
    "    vec2 delta = (light - uv) * density / float(kSteps);\n"
    "    vec2 samplePos = uv;\n"
    "    float weight = 1.0;\n"
    "    vec3 shaft = vec3(0.0);\n"
    "    for (int i = 0; i < kSteps; ++i) {\n"
    "        samplePos += delta;\n"
    "        vec3 c = texture(srcTex, samplePos).rgb;\n"
    "        float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    // Only light above the threshold travels; everything else is an occluder and contributes
    // nothing, which is what carves the shafts out around silhouettes.
    "        shaft += max(luma - threshold, 0.0) * c * weight;\n"
    "        weight *= decay;\n"
    "    }\n"
    "    shaft /= float(kSteps);\n"
    "    imageStore(outImage, outCoord, vec4(scene.rgb + shaft * intensity, scene.a));\n"
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
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int detectProgram = 0;
    unsigned int shaftProgram = 0;
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

}  // namespace

bool ApplyLightShafts(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                       float intensity, float density, float decay, float threshold) {
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
            CompileAndLink(gl, kShaftShaderSource, "shaft", g_state.shaftProgram);
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

    ShaftConfigData shaftData{};
    shaftData.intensity = intensity;
    shaftData.density = density;
    shaftData.decay = decay;
    shaftData.threshold = threshold;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.shaftUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(shaftData), &shaftData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.shaftUbo);

    gl.glUseProgram(g_state.shaftProgram);
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
