// See hud_mask.h.
#include <cstdio>

#include "gl_loader.h"
#include "hud_mask.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_READ_WRITE                = 0x88BA;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;

// The mask test is a 3x3 dilation of the same per-pixel test taa.cpp uses, not the bare test:
// anti-aliased HUD text and icon edges blend toward the world at their borders, and those
// border pixels can differ from the world-only frame by less than the threshold while still
// being visibly part of the overlay. Growing the mask by one pixel catches that fringe.
const char* kHudRestoreShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D stageInput;\n"
    "layout(binding = 1) uniform sampler2D worldTex;\n"
    "layout(binding = 2) uniform sampler2D captureTex;\n"
    "layout(rgba16f, binding = 0) uniform image2D stageOutput;\n"
    "bool IsHud(ivec2 c) {\n"
    "    vec3 finished = texelFetch(captureTex, c, 0).rgb;\n"
    "    vec3 world = texelFetch(worldTex, c, 0).rgb;\n"
    "    return any(greaterThan(abs(finished - world), vec3(1.0 / 128.0)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(stageOutput);\n"
    "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (coord.x >= size.x || coord.y >= size.y) { return; }\n"
    "    bool hud = false;\n"
    "    for (int dy = -1; dy <= 1 && !hud; ++dy) {\n"
    "        for (int dx = -1; dx <= 1 && !hud; ++dx) {\n"
    "            hud = IsHud(clamp(coord + ivec2(dx, dy), ivec2(0), size - ivec2(1)));\n"
    "        }\n"
    "    }\n"
    "    if (hud) {\n"
    "        imageStore(stageOutput, coord, texelFetch(stageInput, coord, 0));\n"
    "    }\n"
    "}\n";

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int generation = 0;
    unsigned int program = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kHudRestoreShaderSource, nullptr);
    gl.glCompileShader(shader);
    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] hud_mask: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] hud_mask: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }
    outProgram = program;
    return true;
}

}  // namespace

bool ApplyHudRestore(unsigned int stageInput, unsigned int stageOutput, unsigned int worldTex,
                     unsigned int captureTex, int width, int height) {
    if (stageInput == 0 || stageOutput == 0 || worldTex == 0 || captureTex == 0 ||
        width <= 0 || height <= 0) {
        return false;
    }
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        return false;
    }
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = PipelineState{};
        g_state.generation = GetGlContextGeneration();
    }
    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
    }
    if (!g_state.initOk) {
        return false;
    }

    // The stage that just ran wrote stageOutput through an image store; reading it back here
    // through another image load needs the image-access barrier, not just the fetch barrier.
    gl.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, stageInput);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, worldTex);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, captureTex);
    gl.glBindImageTexture(0, stageOutput, 0, 0, 0, GL_READ_WRITE, GL_RGBA16F);
    gl.glDispatchCompute((unsigned int)((width + 7) / 8), (unsigned int)((height + 7) / 8), 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT |
                       GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);
    return true;
}
