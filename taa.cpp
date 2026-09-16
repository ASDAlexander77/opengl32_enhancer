// See taa.h. GL pipeline mirrors bilinear_upscale.cpp's ApplyBilinearUpscale(), extended
// with: a two-texture ping-pong history buffer (pingPong[0]/pingPong[1] alternate being
// "last frame's output, read as history" and "this frame's write target") and a small UBO
// (blend + historyValid) the same way hdr_look.cpp/nis_effect.cpp use UBOs for their
// scalar/struct parameters.
#include <cstdint>
#include <cstdio>

#include "taa.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_VIEWPORT                 = 0x0BA2;
const unsigned int GL_BACK                     = 0x0405;
const unsigned int GL_TEXTURE_2D               = 0x0DE1;
const unsigned int GL_TEXTURE0                 = 0x84C0;
const unsigned int GL_ACTIVE_TEXTURE           = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D       = 0x8069;
const unsigned int GL_TEXTURE_MIN_FILTER       = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER       = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S           = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T           = 0x2803;
const unsigned int GL_LINEAR                   = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE            = 0x812F;
const unsigned int GL_RGBA8                    = 0x8058;
const unsigned int GL_WRITE_ONLY               = 0x88B9;
const unsigned int GL_COMPUTE_SHADER           = 0x91B9;
const unsigned int GL_COMPILE_STATUS           = 0x8B81;
const unsigned int GL_LINK_STATUS              = 0x8B82;
const unsigned int GL_CURRENT_PROGRAM          = 0x8B8D;
const unsigned int GL_FRAMEBUFFER              = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER         = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER         = 0x8CA9;
const unsigned int GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const unsigned int GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const unsigned int GL_COLOR_ATTACHMENT0        = 0x8CE0;
const unsigned int GL_COLOR_BUFFER_BIT         = 0x00004000;
const unsigned int GL_NEAREST                  = 0x2600;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT  = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER           = 0x8A11;
const unsigned int GL_UNIFORM_BUFFER_BINDING   = 0x8A28;
const unsigned int GL_DYNAMIC_DRAW             = 0x88E8;
const unsigned int GL_NO_ERROR                 = 0;

// currentTex=binding 0 (texture unit 0), historyTex=binding 1 (texture unit 1),
// outputImage=binding 2 (image unit 2 - a separate namespace from texture units, no
// collision with either sampler binding), TaaConfigBlock=binding 0 (uniform buffer binding
// point 0 - a third separate namespace, no collision with currentTex's texture-unit 0). See
// this plan's Global Constraints "Binding-number discipline" note; the C++ side below must
// bind to these exact same units/points.
const char* kTaaShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D currentTex;\n"
    "layout(binding = 1) uniform sampler2D historyTex;\n"
    "layout(rgba8, binding = 2) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform TaaConfigBlock {\n"
    "    float blend;\n"
    "    int historyValid;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 texelSize = 1.0 / vec2(outSize);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) * texelSize;\n"
    "    vec4 currentColor = texture(currentTex, uv);\n"
    "    if (historyValid == 0) {\n"
    "        imageStore(outputImage, outCoord, currentColor);\n"
    "        return;\n"
    "    }\n"
    "    vec3 neighborMin = currentColor.rgb;\n"
    "    vec3 neighborMax = currentColor.rgb;\n"
    "    for (int dy = -1; dy <= 1; ++dy) {\n"
    "        for (int dx = -1; dx <= 1; ++dx) {\n"
    "            if (dx == 0 && dy == 0) { continue; }\n"
    "            vec3 s = texture(currentTex, uv + vec2(dx, dy) * texelSize).rgb;\n"
    "            neighborMin = min(neighborMin, s);\n"
    "            neighborMax = max(neighborMax, s);\n"
    "        }\n"
    "    }\n"
    "    vec4 historyColor = texture(historyTex, uv);\n"
    "    vec3 clampedHistory = clamp(historyColor.rgb, neighborMin, neighborMax);\n"
    "    vec3 result = mix(currentColor.rgb, clampedHistory, blend);\n"
    "    imageStore(outputImage, outCoord, vec4(result, currentColor.a));\n"
    "}\n";

struct TaaConfigData {
    float blend;
    int32_t historyValid;
};

struct TaaState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int currentTexture = 0;
    unsigned int pingPong[2] = {0, 0};
    unsigned int outputFbo = 0;
    int activeHistory = 0;     // pingPong[activeHistory] holds the last successful frame's output
    bool historyValid = false;
};

TaaState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kTaaShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] taa: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] taa: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

void EnsureTextures(const GlComputeApi& gl, TaaState& state, int width, int height) {
    if (state.texturesValid && state.width == width && state.height == height) {
        return;
    }

    if (state.texturesValid) {
        unsigned int textures[3] = {state.currentTexture, state.pingPong[0], state.pingPong[1]};
        gl.glDeleteTextures(3, textures);
        gl.glDeleteFramebuffers(1, &state.outputFbo);
        state.texturesValid = false;
    }

    unsigned int textures[3] = {0, 0, 0};
    gl.glGenTextures(3, textures);
    state.currentTexture = textures[0];
    state.pingPong[0] = textures[1];
    state.pingPong[1] = textures[2];

    unsigned int allTextures[3] = {state.currentTexture, state.pingPong[0], state.pingPong[1]};
    for (int i = 0; i < 3; ++i) {
        gl.glBindTexture(GL_TEXTURE_2D, allTextures[i]);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    }

    unsigned int fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    state.outputFbo = fbo;

    state.width = width;
    state.height = height;
    state.texturesValid = true;
    // A fresh/resized history buffer holds no meaningful data yet - reset both the ping-pong
    // role and the valid flag so the next call takes the historyValid=0 passthrough path
    // instead of blending against garbage.
    state.activeHistory = 0;
    state.historyValid = false;
}

void EnsureUbo(const GlComputeApi& gl, TaaState& state) {
    if (state.configUbo != 0) {
        return;
    }
    gl.glGenBuffers(1, &state.configUbo);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

void ApplyTaa(float blend) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] taa: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] taa: shader init failed, effect disabled for the rest "
                   "of this process\n");
        }
    }
    if (!g_state.initOk) {
        return;
    }

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    if (width <= 0 || height <= 0) {
        return;
    }

    int savedActiveTexture = 0;
    gl.glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedTextureBinding0 = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding0);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    int savedTextureBinding1 = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding1);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedProgram = 0;
    gl.glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    int savedDrawFbo = 0;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFbo);
    int savedUniformBuffer = 0;
    gl.glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &savedUniformBuffer);

    EnsureTextures(gl, g_state, width, height);
    EnsureUbo(gl, g_state);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, (unsigned int)savedUniformBuffer);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, (unsigned int)savedUniformBuffer);
        gl.glActiveTexture(GL_TEXTURE0 + 1);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding1);
        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding0);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
    };

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.currentTexture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    unsigned int captureErr = gl.glGetError();
    if (captureErr != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] taa: glGetError() = 0x%04X after capture, skipping this "
               "frame\n", captureErr);
        restoreState();
        return;
    }

    int readIndex = g_state.activeHistory;
    int writeIndex = 1 - g_state.activeHistory;

    TaaConfigData configData;
    configData.blend = blend;
    configData.historyValid = g_state.historyValid ? 1 : 0;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.currentTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.pingPong[readIndex]);
    gl.glBindImageTexture(2, g_state.pingPong[writeIndex], 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    gl.glBindFramebuffer(GL_FRAMEBUFFER, g_state.outputFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_state.pingPong[writeIndex], 0);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] taa: glGetError() = 0x%04X after dispatch\n", err);
    } else {
        // Only flip roles on success - if dispatch/blit errored, pingPong[writeIndex] may
        // hold garbage, so keep treating the same (valid) texture as history next call
        // instead of promoting a possibly-bad frame.
        g_state.activeHistory = writeIndex;
        g_state.historyValid = true;
    }

    restoreState();
}
