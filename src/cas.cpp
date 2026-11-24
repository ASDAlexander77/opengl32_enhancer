// See cas.h. Hand-ported from AMD's FidelityFX CAS reference implementation
// (https://github.com/GPUOpen-Effects/FidelityFX-CAS, ffx_cas.h / ffx_a.h, MIT licensed,
// Copyright (c) 2021 Advanced Micro Devices, Inc.) from its A*-macro HLSL/GLSL dialect to plain
// desktop GLSL 430 core. Deliberate deviations from the reference, mirroring fsr.cpp's:
//
//   - The A* type macro layer is gone; native float/vec3 throughout, 32-bit path only.
//   - Exact 1.0/x and sqrt() replace the bit-trick approximations (APrxLoRcpF1, APrxMedRcpF1,
//     APrxLoSqrtF1). That is what the reference's own CAS_GO_SLOWER define selects, and on
//     anything running GL 4.3 compute the exact forms cost nothing measurable.
//   - CAS_BETTER_DIAGONALS is NOT taken, matching the reference's default. It folds the four
//     corner taps into the min/max (doubling their scale, hence its `2.0 - mx` term instead of
//     `1.0 - mx`); the default 5-tap soft min/max is what AMD ships.
//   - CAS_SLOW is not taken either: the reference derives the filter weight from the green
//     channel's amount alone and reuses it for all three channels, which is what the shipped
//     fast path does.
//   - Only the noScaling branch is ported - see cas.h for why this stage never resizes.
#include <cstdio>

#include "cas.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_NO_ERROR                  = 0;

const char* kCasShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(std140, binding = 0) uniform CasConfigBlock {\n"
    "    vec4 params;\n"   // .x = peak (negative), .yzw unused (std140 padding)
    "};\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outImage;\n"
    "vec3 casLoad(ivec2 p, ivec2 maxCoord) {\n"
    "    return texelFetch(srcTex, clamp(p, ivec2(0), maxCoord), 0).rgb;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 sp = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (sp.x >= outSize.x || sp.y >= outSize.y) { return; }\n"
    "    ivec2 maxCoord = outSize - ivec2(1);\n"
    // The 3x3 neighborhood:  a b c
    //                        d e f
    //                        g h i
    "    vec3 a = casLoad(sp + ivec2(-1, -1), maxCoord);\n"
    "    vec3 b = casLoad(sp + ivec2( 0, -1), maxCoord);\n"
    "    vec3 c = casLoad(sp + ivec2( 1, -1), maxCoord);\n"
    "    vec3 d = casLoad(sp + ivec2(-1,  0), maxCoord);\n"
    "    vec3 e = casLoad(sp,                 maxCoord);\n"
    "    vec3 f = casLoad(sp + ivec2( 1,  0), maxCoord);\n"
    "    vec3 g = casLoad(sp + ivec2(-1,  1), maxCoord);\n"
    "    vec3 h = casLoad(sp + ivec2( 0,  1), maxCoord);\n"
    "    vec3 i = casLoad(sp + ivec2( 1,  1), maxCoord);\n"
    // Soft min/max over the cross (d e f / b h). The corner taps a,c,g,i are loaded because the
    // reference's CAS_BETTER_DIAGONALS variant uses them; the default path does not, and the
    // compiler drops those fetches.
    "    vec3 mn = min(min(min(d, e), f), min(b, h));\n"
    "    vec3 mx = max(max(max(d, e), f), max(b, h));\n"
    // "Smooth minimum distance to signal limit divided by smooth max" - how much headroom this
    // pixel has before sharpening would clip, per channel.
    "    vec3 rcpM = 1.0 / max(mx, vec3(1.0 / 32768.0));\n"
    "    vec3 amp = clamp(min(mn, vec3(1.0) - mx) * rcpM, vec3(0.0), vec3(1.0));\n"
    "    amp = sqrt(amp);\n"
    // Filter shape:  0 w 0
    //                w 1 w
    //                0 w 0
    // params.x (peak) is negative, so a larger amount means a stronger negative lobe.
    "    float w = amp.g * params.x;\n"
    "    float rcpWeight = 1.0 / (1.0 + 4.0 * w);\n"
    "    vec3 pix = clamp((b * w + d * w + f * w + h * w + e) * rcpWeight, vec3(0.0), vec3(1.0));\n"
    // Alpha is passed through, not filtered. CAS is an RGB sharpener, and the reference runs on
    // an opaque back buffer where alpha is meaningless - but textureEffect=cas runs this over the
    // game's own textures, where alpha is the cutout/blend mask. Writing a constant 1.0 there
    // makes every masked sprite, glyph and decal opaque, which reads on screen as the texture
    // having gained a solid-color background. Sharpening alpha instead would be just as wrong: it
    // would harden cutout edges the game blends deliberately.
    "    float srcA = texelFetch(srcTex, sp, 0).a;\n"
    "    imageStore(outImage, sp, vec4(pix, srcA));\n"
    "}\n";

// Matches the std140 CasConfigBlock above.
struct CasConfigData {
    float params[4];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int program = 0;
    unsigned int configUbo = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kCasShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] cas: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] cas: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

}  // namespace

bool ApplyCas(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
               float sharpness) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] cas: GL 4.3 compute support unavailable on this context, "
                   "effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (width <= 0 || height <= 0) {
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
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (g_state.initOk && g_state.configUbo == 0) {
            gl.glGenBuffers(1, &g_state.configUbo);
            gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
            gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CasConfigData), nullptr, GL_DYNAMIC_DRAW);
        }
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] cas: shader init failed, effect disabled "
                   "until the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    // CasSetup's sharpness term: peak = -1 / lerp(8, 5, saturate(sharpness)). Note it is
    // negative by construction - the filter's side taps carry a negative weight.
    float clamped = sharpness;
    if (clamped < 0.0f) { clamped = 0.0f; }
    if (clamped > 1.0f) { clamped = 1.0f; }
    float peak = -1.0f / (8.0f + (5.0f - 8.0f) * clamped);

    CasConfigData configData{};
    configData.params[0] = peak;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CasConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(1, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    // dstTexture may next be read by another stage's sampler (GL_TEXTURE_FETCH_BARRIER_BIT) or
    // blitted from as the pipeline's final output (GL_FRAMEBUFFER_BARRIER_BIT).
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] cas: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
