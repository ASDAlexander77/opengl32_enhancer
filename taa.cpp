// See taa.h. Single compute dispatch writing to TWO images at once - dstTexture (this
// pipeline stage's caller-owned output) and pingPong[writeIndex] (this effect's own
// persistent history texture, read back as `historyTex` next call) - both get the same
// computed result. That avoids an extra copy pass: TAA's history buffer can't simply *be*
// dstTexture, because dstTexture is a slot in the shared pipeline's own ping-pong pair (see
// post_effects.cpp), and which physical texture object plays "the shared pipeline's dst" on
// any given frame depends on how many other stages ran before TAA that frame - not something
// TAA can treat as consistently "last frame's output" the way its own private history buffer
// is. No capture/blit/state-save of its own beyond that - the caller owns the app's GL state
// around the whole chain.
#include <cstdint>
#include <cstdio>

#include "taa.h"
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

// currentTex=binding 0 (texture unit 0), historyTex=binding 1 (texture unit 1), outputImage=
// binding 2 and historyImage=binding 3 (image units - a separate namespace from texture
// units, no collision with either sampler binding), TaaConfigBlock=binding 0 (uniform buffer
// binding point 0 - a third separate namespace, no collision with currentTex's texture-unit
// 0). See this plan's Global Constraints "Binding-number discipline" note; the C++ side below
// must bind to these exact same units/points.
const char* kTaaShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D currentTex;\n"
    "layout(binding = 1) uniform sampler2D historyTex;\n"
    "layout(rgba16f, binding = 2) uniform writeonly image2D outputImage;\n"
    "layout(rgba16f, binding = 3) uniform writeonly image2D historyImage;\n"
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
    "        imageStore(historyImage, outCoord, currentColor);\n"
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
    "    vec4 out4 = vec4(result, currentColor.a);\n"
    "    imageStore(outputImage, outCoord, out4);\n"
    "    imageStore(historyImage, outCoord, out4);\n"
    "}\n";

struct TaaConfigData {
    float blend;
    int32_t historyValid;
    int32_t pad[2];
};

struct TaaState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int pingPong[2] = {0, 0};
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
        gl.glDeleteTextures(2, state.pingPong);
        state.texturesValid = false;
    }

    unsigned int textures[2] = {0, 0};
    gl.glGenTextures(2, textures);
    state.pingPong[0] = textures[0];
    state.pingPong[1] = textures[1];

    for (int i = 0; i < 2; ++i) {
        gl.glBindTexture(GL_TEXTURE_2D, state.pingPong[i]);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    }

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

bool ApplyTaa(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float blend) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] taa: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
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
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureTextures(gl, g_state, width, height);
    EnsureUbo(gl, g_state);

    int readIndex = g_state.activeHistory;
    int writeIndex = 1 - g_state.activeHistory;

    TaaConfigData configData{};
    configData.blend = blend;
    configData.historyValid = g_state.historyValid ? 1 : 0;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.pingPong[readIndex]);
    gl.glBindImageTexture(2, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glBindImageTexture(3, g_state.pingPong[writeIndex], 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] taa: glGetError() = 0x%04X after dispatch\n", err);
        // Don't flip roles on a dispatch error - pingPong[writeIndex] may hold garbage, so
        // keep treating the same (valid) texture as history next call instead of promoting a
        // possibly-bad frame. dstTexture may also be garbage, so report failure to the caller.
        return false;
    }

    g_state.activeHistory = writeIndex;
    g_state.historyValid = true;
    return true;
}
