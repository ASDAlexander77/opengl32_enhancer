// See bloom.h. Four sequential compute dispatches sharing one UBO (re-filled before each
// dispatch, since only one pass is ever "in flight" at a time): extract (bright-pass
// threshold) -> blur horizontal -> blur vertical -> composite (additive blend back onto
// srcTexture, written to dstTexture). No capture/blit/state-save of its own - see
// post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture and the app's GL
// state around the whole chain of stages this is one link in. Only the bright-pass/blur
// intermediates (own, resized with width/height) belong to this file.
#include <cstdio>

#include "bloom.h"
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
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_NO_ERROR                  = 0;

// Every shader below: inputTex/srcTex=binding 0 (texture unit 0), a second sampler (only the
// composite shader has one, bloomTex)=binding 1 (texture unit 1), the write image=binding 1
// or 2 (image unit - separate namespace from texture units), BloomConfigBlock=binding 0
// (uniform buffer binding point 0). See taa.cpp/lut_grading.cpp's equivalent comments.

const char* kExtractShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D brightImage;\n"
    "layout(std140, binding = 0) uniform BloomConfigBlock {\n"
    "    float value;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(brightImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec3 c = texture(inputTex, uv).rgb;\n"
    "    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "    float knee = 0.2;\n"
    "    float contribution = smoothstep(value - knee, value + knee, luma);\n"
    "    imageStore(brightImage, outCoord, vec4(c * contribution, 1.0));\n"
    "}\n";

// Standard normalized 9-tap Gaussian weights (w0 + 2*(w1+w2+w3+w4) == 1.0). `value` doubles
// as the blur direction flag here (>0.5 = horizontal, else vertical) - reused across both the
// horizontal and vertical dispatches of the same program.
const char* kBlurShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D dstImage;\n"
    "layout(std140, binding = 0) uniform BloomConfigBlock {\n"
    "    float value;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(dstImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 texelSize = 1.0 / vec2(outSize);\n"
    "    vec2 dir = value > 0.5 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) * texelSize;\n"
    "    float w[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);\n"
    "    vec3 sum = texture(srcTex, uv).rgb * w[0];\n"
    "    for (int i = 1; i < 5; ++i) {\n"
    "        vec2 offset = dir * texelSize * float(i);\n"
    "        sum += texture(srcTex, uv + offset).rgb * w[i];\n"
    "        sum += texture(srcTex, uv - offset).rgb * w[i];\n"
    "    }\n"
    "    imageStore(dstImage, outCoord, vec4(sum, 1.0));\n"
    "}\n";

const char* kCompositeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(binding = 1) uniform sampler2D bloomTex;\n"
    "layout(rgba16f, binding = 2) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform BloomConfigBlock {\n"
    "    float value;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 orig = texture(inputTex, uv);\n"
    "    vec3 bloom = texture(bloomTex, uv).rgb;\n"
    "    vec3 result = clamp(orig.rgb + bloom * value, 0.0, 1.0);\n"
    "    imageStore(outputImage, outCoord, vec4(result, orig.a));\n"
    "}\n";

struct BloomConfigData {
    float value;
    float pad[3];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int extractProgram = 0;
    unsigned int blurProgram = 0;
    unsigned int compositeProgram = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int brightTexture = 0;
    unsigned int blurTempTexture = 0;
    unsigned int blurFinalTexture = 0;
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
        printf("[opengl32_enh_cpp] bloom: FAILED to compile '%s' compute shader: %s\n", label, log);
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
        printf("[opengl32_enh_cpp] bloom: FAILED to link '%s' compute program: %s\n", label, log);
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
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return texture;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        unsigned int textures[3] = {g_state.brightTexture, g_state.blurTempTexture, g_state.blurFinalTexture};
        gl.glDeleteTextures(3, textures);
        g_state.texturesValid = false;
    }

    g_state.brightTexture = CreateWorkTexture(gl, width, height);
    g_state.blurTempTexture = CreateWorkTexture(gl, width, height);
    g_state.blurFinalTexture = CreateWorkTexture(gl, width, height);

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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BloomConfigData), nullptr, GL_DYNAMIC_DRAW);
}

void UploadConfig(const GlComputeApi& gl, float value) {
    BloomConfigData configData{};
    configData.value = value;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BloomConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);
}

unsigned int GroupCount(int extent) {
    return (unsigned int)((extent + 7) / 8);
}

}  // namespace

bool ApplyBloom(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float threshold, float intensity) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] bloom: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        bool ok = true;
        ok &= CompileAndLink(gl, kExtractShaderSource, "extract", g_state.extractProgram);
        ok &= CompileAndLink(gl, kBlurShaderSource, "blur", g_state.blurProgram);
        ok &= CompileAndLink(gl, kCompositeShaderSource, "composite", g_state.compositeProgram);
        g_state.initOk = ok;
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] bloom: shader init failed, effect disabled for the "
                   "rest of this process\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureTextures(gl, width, height);
    EnsureUbo(gl);

    unsigned int groupsX = GroupCount(width);
    unsigned int groupsY = GroupCount(height);

    // Pass 1: extract bright pixels above `threshold` (soft-kneed) from srcTexture into
    // brightTexture.
    UploadConfig(gl, threshold);
    gl.glUseProgram(g_state.extractProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(1, g_state.brightTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    // Pass 2: horizontal blur, brightTexture -> blurTempTexture.
    UploadConfig(gl, 1.0f);
    gl.glUseProgram(g_state.blurProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.brightTexture);
    gl.glBindImageTexture(1, g_state.blurTempTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    // Pass 3: vertical blur, blurTempTexture -> blurFinalTexture.
    UploadConfig(gl, 0.0f);
    gl.glUseProgram(g_state.blurProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.blurTempTexture);
    gl.glBindImageTexture(1, g_state.blurFinalTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    // Pass 4: composite - srcTexture + blurFinalTexture * intensity -> dstTexture.
    UploadConfig(gl, intensity);
    gl.glUseProgram(g_state.compositeProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.blurFinalTexture);
    gl.glBindImageTexture(2, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] bloom: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
