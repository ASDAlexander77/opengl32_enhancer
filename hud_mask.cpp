// See hud_mask.h.
#include <cstdio>

#include "gl_loader.h"
#include "hud_mask.h"
#include "perf_stats.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_R8                        = 0x8229;
const unsigned int GL_TEXTURE_MIN_FILTER        = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER        = 0x2800;
const unsigned int GL_NEAREST                   = 0x2600;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;

// The mask is built ONCE per frame and reused by every restore. It used to be recomputed inside
// each restore - a 3x3 neighbourhood of the capture/world comparison, 18 half-float fetches per
// pixel - after every one of the five or six scene stages, which on a 4000x3200 supersampled
// target cost ~1.5 ms each, ~8 ms a frame, all to recompute a mask that cannot change within the
// frame (both of its inputs are fixed once the frame is captured). Measured with debugStats.
//
// The test is the same per-pixel one taa.cpp uses, dilated 3x3: anti-aliased HUD text and icon
// edges blend toward the world at their borders, and those border pixels can differ from the
// world-only frame by less than the threshold while still being visibly part of the overlay.
// Growing the mask by one pixel catches that fringe. Built in two passes (test, then dilate the
// one-byte result) so the dilation reads bytes rather than two RGBA16F frames per neighbour.
const char* kRawMaskShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 1) uniform sampler2D worldTex;\n"
    "layout(binding = 2) uniform sampler2D captureTex;\n"
    "layout(r8, binding = 0) uniform writeonly image2D rawMask;\n"
    "void main() {\n"
    "    ivec2 size = imageSize(rawMask);\n"
    "    ivec2 c = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (c.x >= size.x || c.y >= size.y) { return; }\n"
    "    vec3 finished = texelFetch(captureTex, c, 0).rgb;\n"
    "    vec3 world = texelFetch(worldTex, c, 0).rgb;\n"
    "    bool hud = any(greaterThan(abs(finished - world), vec3(1.0 / 128.0)));\n"
    "    imageStore(rawMask, c, vec4(hud ? 1.0 : 0.0));\n"
    "}\n";

const char* kDilateShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D rawMask;\n"
    "layout(r8, binding = 0) uniform writeonly image2D mask;\n"
    "void main() {\n"
    "    ivec2 size = imageSize(mask);\n"
    "    ivec2 c = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (c.x >= size.x || c.y >= size.y) { return; }\n"
    "    float m = 0.0;\n"
    "    for (int dy = -1; dy <= 1; ++dy) {\n"
    "        for (int dx = -1; dx <= 1; ++dx) {\n"
    "            m = max(m, texelFetch(rawMask, clamp(c + ivec2(dx, dy), ivec2(0), size - ivec2(1)), 0).r);\n"
    "        }\n"
    "    }\n"
    "    imageStore(mask, c, vec4(m));\n"
    "}\n";

// The restore itself: one byte per pixel, and a copy only where the mask is set.
const char* kHudRestoreShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D stageInput;\n"
    "layout(binding = 3) uniform sampler2D mask;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D stageOutput;\n"
    "void main() {\n"
    "    ivec2 size = imageSize(stageOutput);\n"
    "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (coord.x >= size.x || coord.y >= size.y) { return; }\n"
    "    if (texelFetch(mask, coord, 0).r > 0.5) {\n"
    "        imageStore(stageOutput, coord, texelFetch(stageInput, coord, 0));\n"
    "    }\n"
    "}\n";

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int generation = 0;
    unsigned int program = 0;
    unsigned int rawProgram = 0;
    unsigned int dilateProgram = 0;

    unsigned int rawMaskTex = 0;
    unsigned int maskTex = 0;
    int maskWidth = 0;
    int maskHeight = 0;

    // What the current mask was built from. Rebuilt when any of these changes - the frame
    // serial is what InvalidateHudMask() advances.
    unsigned int builtSerial = 0;
    unsigned int builtWorldTex = 0;
    unsigned int builtCaptureTex = 0;
    bool built = false;
};

unsigned int g_frameSerial = 1;

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* source, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &source, nullptr);
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

unsigned int MakeMaskTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, width, height);
    return texture;
}

void BuildMask(const GlComputeApi& gl, unsigned int worldTex, unsigned int captureTex,
               int width, int height) {
    if (g_state.maskWidth != width || g_state.maskHeight != height) {
        if (g_state.maskTex != 0) {
            unsigned int old[2] = {g_state.rawMaskTex, g_state.maskTex};
            gl.glDeleteTextures(2, old);
        }
        g_state.rawMaskTex = MakeMaskTexture(gl, width, height);
        g_state.maskTex = MakeMaskTexture(gl, width, height);
        g_state.maskWidth = width;
        g_state.maskHeight = height;
    }
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);

    gl.glUseProgram(g_state.rawProgram);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, worldTex);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, captureTex);
    gl.glBindImageTexture(0, g_state.rawMaskTex, 0, 0, 0, GL_WRITE_ONLY, GL_R8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    gl.glUseProgram(g_state.dilateProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.rawMaskTex);
    gl.glBindImageTexture(0, g_state.maskTex, 0, 0, 0, GL_WRITE_ONLY, GL_R8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    g_state.builtSerial = g_frameSerial;
    g_state.builtWorldTex = worldTex;
    g_state.builtCaptureTex = captureTex;
    g_state.built = true;
}

}  // namespace

void InvalidateHudMask() {
    ++g_frameSerial;
}

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
        g_state.initOk = CompileAndLink(gl, kHudRestoreShaderSource, g_state.program) &&
                         CompileAndLink(gl, kRawMaskShaderSource, g_state.rawProgram) &&
                         CompileAndLink(gl, kDilateShaderSource, g_state.dilateProgram);
    }
    if (!g_state.initOk) {
        return false;
    }

    if (!g_state.built || g_state.builtSerial != g_frameSerial ||
        g_state.builtWorldTex != worldTex || g_state.builtCaptureTex != captureTex ||
        g_state.maskWidth != width || g_state.maskHeight != height) {
        BuildMask(gl, worldTex, captureTex, width, height);
        PerfGpuMark("hud mask");
    }

    // The stage that just ran wrote stageOutput through an image store; this pass writes it
    // again, so the stage's writes must land first.
    gl.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, stageInput);
    gl.glActiveTexture(GL_TEXTURE0 + 3);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.maskTex);
    gl.glBindImageTexture(0, stageOutput, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute((unsigned int)((width + 7) / 8), (unsigned int)((height + 7) / 8), 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT |
                       GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);
    return true;
}
