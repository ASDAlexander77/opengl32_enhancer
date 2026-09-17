// See fsr.h. Hand-ported from AMD's FidelityFX FSR 1 reference implementation
// (https://github.com/GPUOpen-Effects/FidelityFX-FSR, ffx_fsr1.h / ffx_a.h, MIT licensed,
// Copyright (c) 2021 Advanced Micro Devices, Inc.) from its A*-macro HLSL/GLSL dialect to plain
// desktop GLSL 430 core. Deliberate deviations from the reference, all of them local:
//
//   - The A* type macro layer (AF1/AF2/AF3/AU1/...) is gone; this uses native float/vec2/vec3.
//     Only the 32-bit path is ported - the packed FP16 (A_HALF) variants exist purely as a
//     perf option on hardware with fast half math and would not change the image.
//   - The reference's bit-trick reciprocal/rsqrt approximations (APrxLoRcpF1, APrxMedRcpF1,
//     APrxLoRsqF1) are replaced with exact 1.0/x and inversesqrt(). They exist to dodge slow
//     transcendentals on pre-Navi hardware; on anything that can run a GL 4.3 compute shader
//     the exact forms are no slower and strictly more accurate. RCAS's own comment notes its
//     limiters "need to be high precision RCPs" anyway.
//   - EASU's four gather4 fetches are replaced with twelve direct textureLod taps at the same
//     texel centers. Identical results, and it avoids depending on textureGather's component
//     ordering, which is what the reference's elaborate a/b/r/g swizzle comments are working
//     around.
//   - No input transform / denoise hook (FsrRcasInputF, FSR_RCAS_DENOISE): the pipeline hands
//     stages linear-ish RGBA16F and there is no noise source to suppress here.
//
// The EASU pass writes into a scratch texture which the RCAS pass then reads, so the caller's
// srcTexture is never written and dstTexture is written exactly once.
#include <cmath>
#include <cstdio>

#include "fsr.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_TEXTURE1                  = 0x84C1;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_MIN_FILTER        = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER        = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S            = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T            = 0x2803;
const unsigned int GL_LINEAR                    = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE             = 0x812F;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_NO_ERROR                  = 0;

// Shared by both passes: con0..con3 are exactly FsrEasuCon's outputs (kept as float vec4s
// rather than the reference's bit-cast uint4s, since nothing here needs the packed-half path),
// and rcasAttenuation is FsrRcasCon's exp2(-sharpnessStops).
const char* kFsrCommonSource =
    "layout(std140, binding = 0) uniform FsrConfigBlock {\n"
    "    vec4 con0;\n"
    "    vec4 con1;\n"
    "    vec4 con2;\n"
    "    vec4 con3;\n"
    "    vec4 rcasParams;\n"   // .x = attenuation, .yzw unused (std140 padding)
    "};\n"
    // Luma times 2, the reference's "simplest multi-channel approximate luma possible".
    "float fsrLuma(vec3 c) {\n"
    "    return c.b * 0.5 + (c.r * 0.5 + c.g);\n"
    "}\n";

// Pass 1: EASU - see FsrEasuF / FsrEasuSetF / FsrEasuTapF in the reference.
const char* kEasuShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outImage;\n";

const char* kEasuBodySource =
    // One tap of the anisotropic, windowed, Lanczos2-approximating kernel (FsrEasuTapF).
    "void fsrEasuTap(inout vec3 aC, inout float aW, vec2 off, vec2 dir, vec2 len,\n"
    "                float lob, float clp, vec3 c) {\n"
    "    vec2 v;\n"
    "    v.x = (off.x * ( dir.x)) + (off.y * dir.y);\n"
    "    v.y = (off.x * (-dir.y)) + (off.y * dir.x);\n"
    "    v *= len;\n"
    "    float d2 = v.x * v.x + v.y * v.y;\n"
    "    d2 = min(d2, clp);\n"
    // (25/16 * (2/5 * x^2 - 1)^2 - (25/16 - 1)) * (lob * x^2 - 1)^2 - base times window.
    "    float wB = (2.0 / 5.0) * d2 + -1.0;\n"
    "    float wA = lob * d2 + -1.0;\n"
    "    wB *= wB;\n"
    "    wA *= wA;\n"
    "    wB = (25.0 / 16.0) * wB + -(25.0 / 16.0 - 1.0);\n"
    "    float w = wB * wA;\n"
    "    aC += c * w;\n"
    "    aW += w;\n"
    "}\n"
    // Accumulate edge direction and length from a '+' of lumas (FsrEasuSetF). `w` is the
    // bilinear weight of this of the four sub-positions; the reference passes it as four
    // compile-time predicates, which collapses to the same four weights computed here.
    "void fsrEasuSet(inout vec2 dir, inout float len, float w,\n"
    "                float lA, float lB, float lC, float lD, float lE) {\n"
    "    float dc = lD - lC;\n"
    "    float cb = lC - lB;\n"
    "    float lenX = max(abs(dc), abs(cb));\n"
    "    lenX = 1.0 / max(lenX, 1.0 / 32768.0);\n"
    "    float dirX = lD - lB;\n"
    "    dir.x += dirX * w;\n"
    "    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);\n"
    "    lenX *= lenX;\n"
    "    len += lenX * w;\n"
    "    float ec = lE - lC;\n"
    "    float ca = lC - lA;\n"
    "    float lenY = max(abs(ec), abs(ca));\n"
    "    lenY = 1.0 / max(lenY, 1.0 / 32768.0);\n"
    "    float dirY = lE - lA;\n"
    "    dir.y += dirY * w;\n"
    "    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);\n"
    "    lenY *= lenY;\n"
    "    len += lenY * w;\n"
    "}\n"
    // Fetch one input texel by its integer position in the (virtual) input grid. con1.xy is
    // 1/inputSize, so this lands on the texel center the reference's gather4 would have hit.
    "vec3 fsrEasuFetch(vec2 fp, vec2 off) {\n"
    "    vec2 uv = (fp + off + vec2(0.5)) * con1.xy;\n"
    "    return textureLod(srcTex, uv, 0.0).rgb;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 ip = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (ip.x >= outSize.x || ip.y >= outSize.y) { return; }\n"
    // Position of 'f' in input space.
    "    vec2 pp = vec2(ip) * con0.xy + con0.zw;\n"
    "    vec2 fp = floor(pp);\n"
    "    pp -= fp;\n"
    // The 12-tap kernel:    b c
    //                     e f g h
    //                     i j k l
    //                       n o
    "    vec3 b = fsrEasuFetch(fp, vec2( 0.0, -1.0));\n"
    "    vec3 c = fsrEasuFetch(fp, vec2( 1.0, -1.0));\n"
    "    vec3 e = fsrEasuFetch(fp, vec2(-1.0,  0.0));\n"
    "    vec3 f = fsrEasuFetch(fp, vec2( 0.0,  0.0));\n"
    "    vec3 g = fsrEasuFetch(fp, vec2( 1.0,  0.0));\n"
    "    vec3 h = fsrEasuFetch(fp, vec2( 2.0,  0.0));\n"
    "    vec3 i = fsrEasuFetch(fp, vec2(-1.0,  1.0));\n"
    "    vec3 j = fsrEasuFetch(fp, vec2( 0.0,  1.0));\n"
    "    vec3 k = fsrEasuFetch(fp, vec2( 1.0,  1.0));\n"
    "    vec3 l = fsrEasuFetch(fp, vec2( 2.0,  1.0));\n"
    "    vec3 n = fsrEasuFetch(fp, vec2( 0.0,  2.0));\n"
    "    vec3 o = fsrEasuFetch(fp, vec2( 1.0,  2.0));\n"
    "    float bL = fsrLuma(b); float cL = fsrLuma(c);\n"
    "    float eL = fsrLuma(e); float fL = fsrLuma(f);\n"
    "    float gL = fsrLuma(g); float hL = fsrLuma(h);\n"
    "    float iL = fsrLuma(i); float jL = fsrLuma(j);\n"
    "    float kL = fsrLuma(k); float lL = fsrLuma(l);\n"
    "    float nL = fsrLuma(n); float oL = fsrLuma(o);\n"
    // Bilinear weights of the four sub-positions, matching the reference's biS/biT/biU/biV.
    "    vec2 dir = vec2(0.0);\n"
    "    float len = 0.0;\n"
    "    fsrEasuSet(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);\n"
    "    fsrEasuSet(dir, len,        pp.x  * (1.0 - pp.y), cL, fL, gL, hL, kL);\n"
    "    fsrEasuSet(dir, len, (1.0 - pp.x) *        pp.y , fL, iL, jL, kL, nL);\n"
    "    fsrEasuSet(dir, len,        pp.x  *        pp.y , gL, jL, kL, lL, oL);\n"
    // Normalize direction, cleaning up the degenerate (flat) case.
    "    vec2 dir2 = dir * dir;\n"
    "    float dirR = dir2.x + dir2.y;\n"
    "    bool zro = dirR < (1.0 / 32768.0);\n"
    "    dirR = inversesqrt(max(dirR, 1.0 / 32768.0));\n"
    "    dirR = zro ? 1.0 : dirR;\n"
    "    dir.x = zro ? 1.0 : dir.x;\n"
    "    dir *= vec2(dirR);\n"
    // {0..2} -> {0..1}, shaped by squaring.
    "    len = len * 0.5;\n"
    "    len *= len;\n"
    // Stretch the kernel from 1.0 on axis to sqrt(2) on the diagonal.
    "    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));\n"
    "    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 + -0.5 * len);\n"
    "    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;\n"
    "    float clp = 1.0 / lob;\n"
    // Deringing bounds from the four nearest taps (f, g, j, k).
    "    vec3 min4 = min(min(min(f, g), j), k);\n"
    "    vec3 max4 = max(max(max(f, g), j), k);\n"
    "    vec3 aC = vec3(0.0);\n"
    "    float aW = 0.0;\n"
    "    fsrEasuTap(aC, aW, vec2( 0.0, -1.0) - pp, dir, len2, lob, clp, b);\n"
    "    fsrEasuTap(aC, aW, vec2( 1.0, -1.0) - pp, dir, len2, lob, clp, c);\n"
    "    fsrEasuTap(aC, aW, vec2(-1.0,  1.0) - pp, dir, len2, lob, clp, i);\n"
    "    fsrEasuTap(aC, aW, vec2( 0.0,  1.0) - pp, dir, len2, lob, clp, j);\n"
    "    fsrEasuTap(aC, aW, vec2( 0.0,  0.0) - pp, dir, len2, lob, clp, f);\n"
    "    fsrEasuTap(aC, aW, vec2(-1.0,  0.0) - pp, dir, len2, lob, clp, e);\n"
    "    fsrEasuTap(aC, aW, vec2( 1.0,  1.0) - pp, dir, len2, lob, clp, k);\n"
    "    fsrEasuTap(aC, aW, vec2( 2.0,  1.0) - pp, dir, len2, lob, clp, l);\n"
    "    fsrEasuTap(aC, aW, vec2( 2.0,  0.0) - pp, dir, len2, lob, clp, h);\n"
    "    fsrEasuTap(aC, aW, vec2( 1.0,  0.0) - pp, dir, len2, lob, clp, g);\n"
    "    fsrEasuTap(aC, aW, vec2( 1.0,  2.0) - pp, dir, len2, lob, clp, o);\n"
    "    fsrEasuTap(aC, aW, vec2( 0.0,  2.0) - pp, dir, len2, lob, clp, n);\n"
    "    vec3 pix = min(max4, max(min4, aC / aW));\n"
    "    imageStore(outImage, ip, vec4(pix, 1.0));\n"
    "}\n";

// Pass 2: RCAS - see FsrRcasF in the reference. 3x3 cross, no denoise, no input transform.
const char* kRcasShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D easuTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outImage;\n";

const char* kRcasBodySource =
    "#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))\n"
    "vec3 fsrRcasLoad(ivec2 p, ivec2 maxCoord) {\n"
    "    return texelFetch(easuTex, clamp(p, ivec2(0), maxCoord), 0).rgb;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outImage);\n"
    "    ivec2 sp = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (sp.x >= outSize.x || sp.y >= outSize.y) { return; }\n"
    "    ivec2 maxCoord = outSize - ivec2(1);\n"
    // Neighborhood:   b
    //               d e f
    //                 h
    "    vec3 b = fsrRcasLoad(sp + ivec2( 0, -1), maxCoord);\n"
    "    vec3 d = fsrRcasLoad(sp + ivec2(-1,  0), maxCoord);\n"
    "    vec3 e = fsrRcasLoad(sp,                 maxCoord);\n"
    "    vec3 f = fsrRcasLoad(sp + ivec2( 1,  0), maxCoord);\n"
    "    vec3 h = fsrRcasLoad(sp + ivec2( 0,  1), maxCoord);\n"
    "    float bL = fsrLuma(b);\n"
    "    float dL = fsrLuma(d);\n"
    "    float eL = fsrLuma(e);\n"
    "    float fL = fsrLuma(f);\n"
    "    float hL = fsrLuma(h);\n"
    // Min/max of the ring, per channel.
    "    vec3 mn4 = min(min(min(b, d), f), h);\n"
    "    vec3 mx4 = max(max(max(b, d), f), h);\n"
    // Limiters. The reference stresses these need high-precision reciprocals, hence exact 1/x.
    "    vec2 peakC = vec2(1.0, -4.0);\n"
    "    vec3 hitMin = min(mn4, e) / (4.0 * mx4);\n"
    "    vec3 hitMax = (peakC.x - max(mx4, e)) / (4.0 * mn4 + peakC.y);\n"
    "    vec3 lobeRGB = max(-hitMin, hitMax);\n"
    "    float lobe = max(-FSR_RCAS_LIMIT,\n"
    "                     min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0)) * rcasParams.x;\n"
    "    vec3 pix = (lobe * b + lobe * d + lobe * h + lobe * f + e) / (4.0 * lobe + 1.0);\n"
    "    imageStore(outImage, sp, vec4(pix, 1.0));\n"
    "}\n";

// Matches the std140 FsrConfigBlock above.
struct FsrConfigData {
    float con0[4];
    float con1[4];
    float con2[4];
    float con3[4];
    float rcasParams[4];
};

struct FsrState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int easuProgram = 0;
    unsigned int rcasProgram = 0;
    unsigned int configUbo = 0;

    bool scratchValid = false;
    int width = 0;
    int height = 0;
    unsigned int easuTex = 0;   // EASU output, RCAS input
};

FsrState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* header, const char* body,
                     const char* passName, unsigned int& outProgram) {
    const char* sources[] = {header, kFsrCommonSource, body};
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 3, sources, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[4096];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] fsr: FAILED to compile %s compute shader: %s\n", passName, log);
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
        char log[4096];
        int logLen = 0;
        gl.glGetProgramInfoLog(program, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] fsr: FAILED to link %s compute program: %s\n", passName, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

unsigned int CreateScratchTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return tex;
}

void EnsureScratchTexture(const GlComputeApi& gl, int width, int height) {
    if (g_state.scratchValid && g_state.width == width && g_state.height == height) {
        return;
    }
    if (g_state.scratchValid) {
        gl.glDeleteTextures(1, &g_state.easuTex);
        g_state.easuTex = 0;
        g_state.scratchValid = false;
    }
    g_state.easuTex = CreateScratchTexture(gl, width, height);
    g_state.width = width;
    g_state.height = height;
    g_state.scratchValid = true;
}

// FsrEasuCon from the reference, minus the bit-casting - inputViewport and inputSize are the
// same here because the "virtual" low-res image we resample the source into IS the whole input
// grid; there is no dynamic-resolution sub-rect to account for.
void FillEasuConstants(FsrConfigData& data, float inputW, float inputH, float outputW, float outputH) {
    data.con0[0] = inputW / outputW;
    data.con0[1] = inputH / outputH;
    data.con0[2] = 0.5f * inputW / outputW - 0.5f;
    data.con0[3] = 0.5f * inputH / outputH - 0.5f;

    data.con1[0] = 1.0f / inputW;
    data.con1[1] = 1.0f / inputH;
    data.con1[2] = 1.0f / inputW;
    data.con1[3] = -1.0f / inputH;

    data.con2[0] = -1.0f / inputW;
    data.con2[1] = 2.0f / inputH;
    data.con2[2] = 1.0f / inputW;
    data.con2[3] = 2.0f / inputH;

    data.con3[0] = 0.0f;
    data.con3[1] = 4.0f / inputH;
    data.con3[2] = 0.0f;
    data.con3[3] = 0.0f;
}

}  // namespace

bool ApplyFsr(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
               float scale, float sharpness) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] fsr: GL 4.3 compute support unavailable on this context, "
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
        g_state = FsrState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        bool ok = true;
        ok &= CompileAndLink(gl, kEasuShaderSource, kEasuBodySource, "easu", g_state.easuProgram);
        ok &= CompileAndLink(gl, kRcasShaderSource, kRcasBodySource, "rcas", g_state.rcasProgram);
        if (ok && g_state.configUbo == 0) {
            gl.glGenBuffers(1, &g_state.configUbo);
            gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
            gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(FsrConfigData), nullptr, GL_DYNAMIC_DRAW);
        }
        g_state.initOk = ok;
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] fsr: shader init failed, effect disabled "
                   "until the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    EnsureScratchTexture(gl, width, height);

    // The source is a full-resolution capture, so `scale` is applied by treating it as a
    // lower-resolution grid: EASU samples it at that reduced rate and reconstructs full size,
    // which is the workload FSR 1 is designed for. Clamped to at least one texel so the
    // reciprocals in FillEasuConstants can never divide by zero.
    float clampedScale = scale;
    if (clampedScale < 0.05f) { clampedScale = 0.05f; }
    if (clampedScale > 1.0f)  { clampedScale = 1.0f; }
    float inputW = (float)width * clampedScale;
    float inputH = (float)height * clampedScale;
    if (inputW < 1.0f) { inputW = 1.0f; }
    if (inputH < 1.0f) { inputH = 1.0f; }

    FsrConfigData configData{};
    FillEasuConstants(configData, inputW, inputH, (float)width, (float)height);
    // FsrRcasCon: sharpness arrives 0..1 (1 = sharpest) but RCAS wants "stops" of attenuation,
    // where 0 stops is maximum sharpening. Map 1 -> 0 stops and 0 -> 2 stops, then take
    // exp2(-stops) exactly as the reference does.
    float stops = (1.0f - sharpness) * 2.0f;
    configData.rcasParams[0] = std::exp2(-stops);

    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(FsrConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);

    // Pass 1: EASU, srcTexture -> scratch.
    gl.glUseProgram(g_state.easuProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(1, g_state.easuTex, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    // The next pass samples this scratch texture, so the image writes must land first.
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Pass 2: RCAS, scratch -> dstTexture.
    gl.glUseProgram(g_state.rcasProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.easuTex);
    gl.glBindImageTexture(1, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    // dstTexture may next be read by another stage's sampler (GL_TEXTURE_FETCH_BARRIER_BIT) or
    // blitted from as the pipeline's final output (GL_FRAMEBUFFER_BARRIER_BIT).
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] fsr: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
