// See nis_effect.h. GL pipeline mirrors bilinear_upscale.cpp's ApplyBilinearUpscale(): GPU-
// to-GPU capture via glCopyTexSubImage2D, compute dispatch, glBlitFramebuffer present, full
// GL state save/restore. Two additions bilinear didn't need: a UBO holding NISConfig (from
// vendored nis_config.h) and two small coefficient textures (coef_scale/coef_usm, built once
// from nis_config.h's constexpr tables).
//
// The two shader strings below are a hand-port of NVIDIA's
// E:\Gits\NVIDIAImageScaling\NIS\NIS_Main.glsl + NIS\NIS_Scaler.h (NIS Image Scaling SDK
// v1.0.3, MIT licensed, Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES) from Vulkan-
// flavored GLSL to desktop GLSL 430 core - see this plan's Global Constraints for the exact,
// deliberate deviations (combined sampler2D instead of separate texture2D+sampler, native
// GLSL types instead of the NVF/NVI/NVU HLSL/GLSL macro layer, and every branch this
// project's fixed configuration (no HDR, no viewport subsetting, no NV12, no texture gather,
// no output clamp) never takes stripped out rather than kept behind #if).
#include <cstdio>
#include <cstring>

#include "nis_effect.h"
#include "nis_config.h"
#include "gl_loader.h"

namespace {

const char* kNVScalerShaderSource =
    "#version 430\n"
    "layout(std140, binding = 0) uniform NISConfigBlock {\n"
    "    float kDetectRatio;\n"
    "    float kDetectThres;\n"
    "    float kMinContrastRatio;\n"
    "    float kRatioNorm;\n"
    "    float kContrastBoost;\n"
    "    float kEps;\n"
    "    float kSharpStartY;\n"
    "    float kSharpScaleY;\n"
    "    float kSharpStrengthMin;\n"
    "    float kSharpStrengthScale;\n"
    "    float kSharpLimitMin;\n"
    "    float kSharpLimitScale;\n"
    "    float kScaleX;\n"
    "    float kScaleY;\n"
    "    float kDstNormX;\n"
    "    float kDstNormY;\n"
    "    float kSrcNormX;\n"
    "    float kSrcNormY;\n"
    "    uint kInputViewportOriginX;\n"
    "    uint kInputViewportOriginY;\n"
    "    uint kInputViewportWidth;\n"
    "    uint kInputViewportHeight;\n"
    "    uint kOutputViewportOriginX;\n"
    "    uint kOutputViewportOriginY;\n"
    "    uint kOutputViewportWidth;\n"
    "    uint kOutputViewportHeight;\n"
    "    float reserved0;\n"
    "    float reserved1;\n"
    "};\n"
    "layout(binding = 1) uniform sampler2D in_texture;\n"
    "layout(rgba8, binding = 2) uniform writeonly image2D out_texture;\n"
    "layout(binding = 3) uniform sampler2D coef_scaler;\n"
    "layout(binding = 4) uniform sampler2D coef_usm;\n"
    "#define saturate(x) clamp(x, 0.0, 1.0)\n"
    "#define lerp(a, b, x) mix(a, b, x)\n"
    "float getY(vec3 rgba) {\n"
    "    return 0.2126 * rgba.x + 0.7152 * rgba.y + 0.0722 * rgba.z;\n"
    "}\n"
    "vec4 GetEdgeMap(float p[4][4], int i, int j) {\n"
    "    const float g_0 = abs(p[0+i][0+j] + p[0+i][1+j] + p[0+i][2+j] - p[2+i][0+j] - p[2+i][1+j] - p[2+i][2+j]);\n"
    "    const float g_45 = abs(p[1+i][0+j] + p[0+i][0+j] + p[0+i][1+j] - p[2+i][1+j] - p[2+i][2+j] - p[1+i][2+j]);\n"
    "    const float g_90 = abs(p[0+i][0+j] + p[1+i][0+j] + p[2+i][0+j] - p[0+i][2+j] - p[1+i][2+j] - p[2+i][2+j]);\n"
    "    const float g_135 = abs(p[1+i][0+j] + p[2+i][0+j] + p[2+i][1+j] - p[0+i][1+j] - p[0+i][2+j] - p[1+i][2+j]);\n"
    "    const float g_0_90_max = max(g_0, g_90);\n"
    "    const float g_0_90_min = min(g_0, g_90);\n"
    "    const float g_45_135_max = max(g_45, g_135);\n"
    "    const float g_45_135_min = min(g_45, g_135);\n"
    "    float e_0_90 = 0.0;\n"
    "    float e_45_135 = 0.0;\n"
    "    if (g_0_90_max + g_45_135_max == 0.0) { return vec4(0.0, 0.0, 0.0, 0.0); }\n"
    "    e_0_90 = min(g_0_90_max / (g_0_90_max + g_45_135_max), 1.0);\n"
    "    e_45_135 = 1.0 - e_0_90;\n"
    "    bool c_0_90 = (g_0_90_max > (g_0_90_min * kDetectRatio)) && (g_0_90_max > kDetectThres) && (g_0_90_max > g_45_135_min);\n"
    "    bool c_45_135 = (g_45_135_max > (g_45_135_min * kDetectRatio)) && (g_45_135_max > kDetectThres) && (g_45_135_max > g_0_90_min);\n"
    "    bool c_g_0_90 = g_0_90_max == g_0;\n"
    "    bool c_g_45_135 = g_45_135_max == g_45;\n"
    "    float f_e_0_90 = (c_0_90 && c_45_135) ? e_0_90 : 1.0;\n"
    "    float f_e_45_135 = (c_0_90 && c_45_135) ? e_45_135 : 1.0;\n"
    "    float weight_0 = (c_0_90 && c_g_0_90) ? f_e_0_90 : 0.0;\n"
    "    float weight_90 = (c_0_90 && !c_g_0_90) ? f_e_0_90 : 0.0;\n"
    "    float weight_45 = (c_45_135 && c_g_45_135) ? f_e_45_135 : 0.0;\n"
    "    float weight_135 = (c_45_135 && !c_g_45_135) ? f_e_45_135 : 0.0;\n"
    "    return vec4(weight_0, weight_90, weight_45, weight_135);\n"
    "}\n"
    "#define NIS_BLOCK_WIDTH 32\n"
    "#define NIS_BLOCK_HEIGHT 24\n"
    "#define NIS_THREAD_GROUP_SIZE 256\n"
    "#define kPhaseCount 64\n"
    "#define kSupportSize 6\n"
    "#define kPadSize kSupportSize\n"
    "#define kTilePitch (NIS_BLOCK_WIDTH + kPadSize)\n"
    "#define kTileSize (kTilePitch * (NIS_BLOCK_HEIGHT + kPadSize))\n"
    "#define kEdgeMapPitch (NIS_BLOCK_WIDTH + 2)\n"
    "#define kEdgeMapSize (kEdgeMapPitch * (NIS_BLOCK_HEIGHT + 2))\n"
    "shared float shPixelsY[kTileSize];\n"
    "shared float shCoefScaler[kPhaseCount][6];\n"
    "shared float shCoefUSM[kPhaseCount][6];\n"
    "shared vec4 shEdgeMap[kEdgeMapSize];\n"
    "void LoadFilterBanksSh(int i0) {\n"
    "    for (int i = i0; i < kPhaseCount * 2; i += NIS_THREAD_GROUP_SIZE) {\n"
    "        int phase = i >> 1;\n"
    "        int vIdx = i & 1;\n"
    "        vec4 v = texelFetch(coef_scaler, ivec2(vIdx, phase), 0);\n"
    "        int filterOffset = vIdx * 4;\n"
    "        shCoefScaler[phase][filterOffset + 0] = v.x;\n"
    "        shCoefScaler[phase][filterOffset + 1] = v.y;\n"
    "        if (vIdx == 0) { shCoefScaler[phase][2] = v.z; shCoefScaler[phase][3] = v.w; }\n"
    "        v = texelFetch(coef_usm, ivec2(vIdx, phase), 0);\n"
    "        shCoefUSM[phase][filterOffset + 0] = v.x;\n"
    "        shCoefUSM[phase][filterOffset + 1] = v.y;\n"
    "        if (vIdx == 0) { shCoefUSM[phase][2] = v.z; shCoefUSM[phase][3] = v.w; }\n"
    "    }\n"
    "}\n"
    "float CalcLTI(float p0, float p1, float p2, float p3, float p4, float p5, int phase_index) {\n"
    "    const bool selector = (phase_index <= kPhaseCount / 2);\n"
    "    float sel = selector ? p0 : p3;\n"
    "    const float a_min = min(min(p1, p2), sel);\n"
    "    const float a_max = max(max(p1, p2), sel);\n"
    "    sel = selector ? p2 : p5;\n"
    "    const float b_min = min(min(p3, p4), sel);\n"
    "    const float b_max = max(max(p3, p4), sel);\n"
    "    const float a_cont = a_max - a_min;\n"
    "    const float b_cont = b_max - b_min;\n"
    "    const float cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + kEps);\n"
    "    return (1.0 - saturate((cont_ratio - kMinContrastRatio) * kRatioNorm)) * kContrastBoost;\n"
    "}\n"
    "vec4 GetInterpEdgeMap(const vec4 edge[2][2], float phase_frac_x, float phase_frac_y) {\n"
    "    vec4 h0 = lerp(edge[0][0], edge[0][1], phase_frac_x);\n"
    "    vec4 h1 = lerp(edge[1][0], edge[1][1], phase_frac_x);\n"
    "    return lerp(h0, h1, phase_frac_y);\n"
    "}\n"
    "float EvalPoly6(const float pxl[6], int phase_int) {\n"
    "    float y = 0.0;\n"
    "    for (int i = 0; i < 6; ++i) { y += shCoefScaler[phase_int][i] * pxl[i]; }\n"
    "    float y_usm = 0.0;\n"
    "    for (int i = 0; i < 6; ++i) { y_usm += shCoefUSM[phase_int][i] * pxl[i]; }\n"
    "    const float y_scale = 1.0 - saturate((y - kSharpStartY) * kSharpScaleY);\n"
    "    const float y_sharpness = y_scale * kSharpStrengthScale + kSharpStrengthMin;\n"
    "    y_usm *= y_sharpness;\n"
    "    const float y_sharpness_limit = (y_scale * kSharpLimitScale + kSharpLimitMin) * y;\n"
    "    y_usm = min(y_sharpness_limit, max(-y_sharpness_limit, y_usm));\n"
    "    y_usm *= CalcLTI(pxl[0], pxl[1], pxl[2], pxl[3], pxl[4], pxl[5], phase_int);\n"
    "    return y + y_usm;\n"
    "}\n"
    "float FilterNormal(const float p[6][6], int phase_x_frac_int, int phase_y_frac_int) {\n"
    "    float h_acc = 0.0;\n"
    "    for (int j = 0; j < 6; ++j) {\n"
    "        float v_acc = 0.0;\n"
    "        for (int i = 0; i < 6; ++i) { v_acc += p[i][j] * shCoefScaler[phase_y_frac_int][i]; }\n"
    "        h_acc += v_acc * shCoefScaler[phase_x_frac_int][j];\n"
    "    }\n"
    "    return h_acc;\n"
    "}\n"
    "float AddDirFilters(float p[6][6], float phase_x_frac, float phase_y_frac, int phase_x_frac_int, int phase_y_frac_int, vec4 w) {\n"
    "    float f = 0.0;\n"
    "    if (w.x > 0.0) {\n"
    "        float interp0Deg[6];\n"
    "        for (int i = 0; i < 6; ++i) { interp0Deg[i] = lerp(p[i][2], p[i][3], phase_x_frac); }\n"
    "        f += EvalPoly6(interp0Deg, phase_y_frac_int) * w.x;\n"
    "    }\n"
    "    if (w.y > 0.0) {\n"
    "        float interp90Deg[6];\n"
    "        for (int i = 0; i < 6; ++i) { interp90Deg[i] = lerp(p[2][i], p[3][i], phase_y_frac); }\n"
    "        f += EvalPoly6(interp90Deg, phase_x_frac_int) * w.y;\n"
    "    }\n"
    "    if (w.z > 0.0) {\n"
    "        float pphase_b45 = 0.5 + 0.5 * (phase_x_frac - phase_y_frac);\n"
    "        float temp_interp45Deg[7];\n"
    "        temp_interp45Deg[1] = lerp(p[2][1], p[1][2], pphase_b45);\n"
    "        temp_interp45Deg[3] = lerp(p[3][2], p[2][3], pphase_b45);\n"
    "        temp_interp45Deg[5] = lerp(p[4][3], p[3][4], pphase_b45);\n"
    "        pphase_b45 = pphase_b45 - 0.5;\n"
    "        float a45 = (pphase_b45 >= 0.0) ? p[0][2] : p[2][0];\n"
    "        float b45 = (pphase_b45 >= 0.0) ? p[1][3] : p[3][1];\n"
    "        float c45 = (pphase_b45 >= 0.0) ? p[2][4] : p[4][2];\n"
    "        float d45 = (pphase_b45 >= 0.0) ? p[3][5] : p[5][3];\n"
    "        temp_interp45Deg[0] = lerp(p[1][1], a45, abs(pphase_b45));\n"
    "        temp_interp45Deg[2] = lerp(p[2][2], b45, abs(pphase_b45));\n"
    "        temp_interp45Deg[4] = lerp(p[3][3], c45, abs(pphase_b45));\n"
    "        temp_interp45Deg[6] = lerp(p[4][4], d45, abs(pphase_b45));\n"
    "        float interp45Deg[6];\n"
    "        float pphase_p45 = phase_x_frac + phase_y_frac;\n"
    "        if (pphase_p45 >= 1.0) {\n"
    "            for (int i = 0; i < 6; i++) { interp45Deg[i] = temp_interp45Deg[i + 1]; }\n"
    "            pphase_p45 = pphase_p45 - 1.0;\n"
    "        } else {\n"
    "            for (int i = 0; i < 6; i++) { interp45Deg[i] = temp_interp45Deg[i]; }\n"
    "        }\n"
    "        f += EvalPoly6(interp45Deg, int(pphase_p45 * 64.0)) * w.z;\n"
    "    }\n"
    "    if (w.w > 0.0) {\n"
    "        float pphase_b135 = 0.5 * (phase_x_frac + phase_y_frac);\n"
    "        float temp_interp135Deg[7];\n"
    "        temp_interp135Deg[1] = lerp(p[3][1], p[4][2], pphase_b135);\n"
    "        temp_interp135Deg[3] = lerp(p[2][2], p[3][3], pphase_b135);\n"
    "        temp_interp135Deg[5] = lerp(p[1][3], p[2][4], pphase_b135);\n"
    "        pphase_b135 = pphase_b135 - 0.5;\n"
    "        float a135 = (pphase_b135 >= 0.0) ? p[5][2] : p[3][0];\n"
    "        float b135 = (pphase_b135 >= 0.0) ? p[4][3] : p[2][1];\n"
    "        float c135 = (pphase_b135 >= 0.0) ? p[3][4] : p[1][2];\n"
    "        float d135 = (pphase_b135 >= 0.0) ? p[2][5] : p[0][3];\n"
    "        temp_interp135Deg[0] = lerp(p[4][1], a135, abs(pphase_b135));\n"
    "        temp_interp135Deg[2] = lerp(p[3][2], b135, abs(pphase_b135));\n"
    "        temp_interp135Deg[4] = lerp(p[2][3], c135, abs(pphase_b135));\n"
    "        temp_interp135Deg[6] = lerp(p[1][4], d135, abs(pphase_b135));\n"
    "        float interp135Deg[6];\n"
    "        float pphase_p135 = 1.0 + (phase_x_frac - phase_y_frac);\n"
    "        if (pphase_p135 >= 1.0) {\n"
    "            for (int i = 0; i < 6; ++i) { interp135Deg[i] = temp_interp135Deg[i + 1]; }\n"
    "            pphase_p135 = pphase_p135 - 1.0;\n"
    "        } else {\n"
    "            for (int i = 0; i < 6; ++i) { interp135Deg[i] = temp_interp135Deg[i]; }\n"
    "        }\n"
    "        f += EvalPoly6(interp135Deg, int(pphase_p135 * 64.0)) * w.w;\n"
    "    }\n"
    "    return f;\n"
    "}\n"
    "void NVScaler(uvec2 blockIdx, uint threadIdx) {\n"
    "    int dstBlockX = int(NIS_BLOCK_WIDTH * blockIdx.x);\n"
    "    int dstBlockY = int(NIS_BLOCK_HEIGHT * blockIdx.y);\n"
    "    const int srcBlockStartX = int(floor((dstBlockX + 0.5) * kScaleX - 0.5));\n"
    "    const int srcBlockStartY = int(floor((dstBlockY + 0.5) * kScaleY - 0.5));\n"
    "    const int srcBlockEndX = int(ceil((dstBlockX + NIS_BLOCK_WIDTH + 0.5) * kScaleX - 0.5));\n"
    "    const int srcBlockEndY = int(ceil((dstBlockY + NIS_BLOCK_HEIGHT + 0.5) * kScaleY - 0.5));\n"
    "    int numTilePixelsX = srcBlockEndX - srcBlockStartX + kSupportSize - 1;\n"
    "    int numTilePixelsY = srcBlockEndY - srcBlockStartY + kSupportSize - 1;\n"
    "    numTilePixelsX += numTilePixelsX & 0x1;\n"
    "    numTilePixelsY += numTilePixelsY & 0x1;\n"
    "    const int numTilePixels = numTilePixelsX * numTilePixelsY;\n"
    "    const int numEdgeMapPixelsX = numTilePixelsX - kSupportSize + 2;\n"
    "    const int numEdgeMapPixelsY = numTilePixelsY - kSupportSize + 2;\n"
    "    const int numEdgeMapPixels = numEdgeMapPixelsX * numEdgeMapPixelsY;\n"
    "    for (uint i = threadIdx * 2u; i < uint(numTilePixels) >> 1; i += NIS_THREAD_GROUP_SIZE * 2u) {\n"
    "        uint py = (i / uint(numTilePixelsX)) * 2u;\n"
    "        uint px = i % uint(numTilePixelsX);\n"
    "        float kShift = 0.5 - (kSupportSize - 1) / 2;\n"
    "        const float tx = (srcBlockStartX + px + kShift) * kSrcNormX;\n"
    "        const float ty = (srcBlockStartY + py + kShift) * kSrcNormY;\n"
    "        float p[2][2];\n"
    "        for (int j = 0; j < 2; j++) {\n"
    "            for (int k = 0; k < 2; k++) {\n"
    "                const vec4 px4 = textureLod(in_texture, vec2(tx + k * kSrcNormX, ty + j * kSrcNormY), 0.0);\n"
    "                p[j][k] = getY(px4.xyz);\n"
    "            }\n"
    "        }\n"
    "        const uint idx = py * kTilePitch + px;\n"
    "        shPixelsY[idx] = p[0][0];\n"
    "        shPixelsY[idx + 1] = p[0][1];\n"
    "        shPixelsY[idx + kTilePitch] = p[1][0];\n"
    "        shPixelsY[idx + kTilePitch + 1] = p[1][1];\n"
    "    }\n"
    "    groupMemoryBarrier(); barrier();\n"
    "    for (uint i = threadIdx * 2u; i < uint(numEdgeMapPixels) >> 1; i += NIS_THREAD_GROUP_SIZE * 2u) {\n"
    "        uint py = (i / uint(numEdgeMapPixelsX)) * 2u;\n"
    "        uint px = i % uint(numEdgeMapPixelsX);\n"
    "        const uint edgeMapIdx = py * kEdgeMapPitch + px;\n"
    "        uint tileCornerIdx = (py + 1u) * kTilePitch + px + 1u;\n"
    "        float p[4][4];\n"
    "        for (int j = 0; j < 4; j++) { for (int k = 0; k < 4; k++) { p[j][k] = shPixelsY[tileCornerIdx + j * kTilePitch + k]; } }\n"
    "        shEdgeMap[edgeMapIdx] = GetEdgeMap(p, 0, 0);\n"
    "        shEdgeMap[edgeMapIdx + 1u] = GetEdgeMap(p, 0, 1);\n"
    "        shEdgeMap[edgeMapIdx + kEdgeMapPitch] = GetEdgeMap(p, 1, 0);\n"
    "        shEdgeMap[edgeMapIdx + kEdgeMapPitch + 1u] = GetEdgeMap(p, 1, 1);\n"
    "    }\n"
    "    LoadFilterBanksSh(int(threadIdx));\n"
    "    groupMemoryBarrier(); barrier();\n"
    "    const ivec2 pos = ivec2(int(threadIdx) % NIS_BLOCK_WIDTH, int(threadIdx) / NIS_BLOCK_WIDTH);\n"
    "    const int dstX = dstBlockX + pos.x;\n"
    "    const float srcX = (0.5 + dstX) * kScaleX - 0.5;\n"
    "    const int px = int(floor(srcX) - srcBlockStartX);\n"
    "    const float fx = srcX - floor(srcX);\n"
    "    const int fx_int = int(fx * kPhaseCount);\n"
    "    for (int k = 0; k < NIS_BLOCK_WIDTH * NIS_BLOCK_HEIGHT / NIS_THREAD_GROUP_SIZE; ++k) {\n"
    "        const int dstY = dstBlockY + pos.y + k * (NIS_THREAD_GROUP_SIZE / NIS_BLOCK_WIDTH);\n"
    "        const float srcY = (0.5 + dstY) * kScaleY - 0.5;\n"
    "        const int py = int(floor(srcY) - srcBlockStartY);\n"
    "        const float fy = srcY - floor(srcY);\n"
    "        const int fy_int = int(fy * kPhaseCount);\n"
    "        const int startEdgeMapIdx = py * kEdgeMapPitch + px;\n"
    "        vec4 edge[2][2];\n"
    "        for (int i = 0; i < 2; i++) { for (int j = 0; j < 2; j++) { edge[i][j] = shEdgeMap[startEdgeMapIdx + (i * kEdgeMapPitch) + j]; } }\n"
    "        const vec4 w = GetInterpEdgeMap(edge, fx, fy);\n"
    "        const int startTileIdx = py * kTilePitch + px;\n"
    "        float p[6][6];\n"
    "        for (int i = 0; i < 6; ++i) { for (int j = 0; j < 6; ++j) { p[i][j] = shPixelsY[startTileIdx + i * kTilePitch + j]; } }\n"
    "        const float baseWeight = 1.0 - w.x - w.y - w.z - w.w;\n"
    "        float opY = 0.0;\n"
    "        opY += FilterNormal(p, fx_int, fy_int) * baseWeight;\n"
    "        opY += AddDirFilters(p, fx, fy, fx_int, fy_int, w);\n"
    "        vec2 coord = vec2((srcX + 0.5) * kSrcNormX, (srcY + 0.5) * kSrcNormY);\n"
    "        ivec2 dstCoord = ivec2(dstX, dstY);\n"
    "        vec4 op = textureLod(in_texture, coord, 0.0);\n"
    "        float y = getY(op.xyz);\n"
    "        const float corr = opY - y;\n"
    "        op.x += corr; op.y += corr; op.z += corr;\n"
    "        imageStore(out_texture, dstCoord, op);\n"
    "    }\n"
    "}\n"
    "layout(local_size_x = NIS_THREAD_GROUP_SIZE) in;\n"
    "void main() { NVScaler(gl_WorkGroupID.xy, gl_LocalInvocationID.x); }\n";

const char* kNVSharpenShaderSource =
    "#version 430\n"
    "layout(std140, binding = 0) uniform NISConfigBlock {\n"
    "    float kDetectRatio;\n"
    "    float kDetectThres;\n"
    "    float kMinContrastRatio;\n"
    "    float kRatioNorm;\n"
    "    float kContrastBoost;\n"
    "    float kEps;\n"
    "    float kSharpStartY;\n"
    "    float kSharpScaleY;\n"
    "    float kSharpStrengthMin;\n"
    "    float kSharpStrengthScale;\n"
    "    float kSharpLimitMin;\n"
    "    float kSharpLimitScale;\n"
    "    float kScaleX;\n"
    "    float kScaleY;\n"
    "    float kDstNormX;\n"
    "    float kDstNormY;\n"
    "    float kSrcNormX;\n"
    "    float kSrcNormY;\n"
    "    uint kInputViewportOriginX;\n"
    "    uint kInputViewportOriginY;\n"
    "    uint kInputViewportWidth;\n"
    "    uint kInputViewportHeight;\n"
    "    uint kOutputViewportOriginX;\n"
    "    uint kOutputViewportOriginY;\n"
    "    uint kOutputViewportWidth;\n"
    "    uint kOutputViewportHeight;\n"
    "    float reserved0;\n"
    "    float reserved1;\n"
    "};\n"
    "layout(binding = 1) uniform sampler2D in_texture;\n"
    "layout(rgba8, binding = 2) uniform writeonly image2D out_texture;\n"
    "#define saturate(x) clamp(x, 0.0, 1.0)\n"
    "#define lerp(a, b, x) mix(a, b, x)\n"
    "float getY(vec3 rgba) {\n"
    "    return 0.2126 * rgba.x + 0.7152 * rgba.y + 0.0722 * rgba.z;\n"
    "}\n"
    "vec4 GetEdgeMap(float p[5][5], int i, int j) {\n"
    "    const float g_0 = abs(p[0+i][0+j] + p[0+i][1+j] + p[0+i][2+j] - p[2+i][0+j] - p[2+i][1+j] - p[2+i][2+j]);\n"
    "    const float g_45 = abs(p[1+i][0+j] + p[0+i][0+j] + p[0+i][1+j] - p[2+i][1+j] - p[2+i][2+j] - p[1+i][2+j]);\n"
    "    const float g_90 = abs(p[0+i][0+j] + p[1+i][0+j] + p[2+i][0+j] - p[0+i][2+j] - p[1+i][2+j] - p[2+i][2+j]);\n"
    "    const float g_135 = abs(p[1+i][0+j] + p[2+i][0+j] + p[2+i][1+j] - p[0+i][1+j] - p[0+i][2+j] - p[1+i][2+j]);\n"
    "    const float g_0_90_max = max(g_0, g_90);\n"
    "    const float g_0_90_min = min(g_0, g_90);\n"
    "    const float g_45_135_max = max(g_45, g_135);\n"
    "    const float g_45_135_min = min(g_45, g_135);\n"
    "    float e_0_90 = 0.0;\n"
    "    float e_45_135 = 0.0;\n"
    "    if (g_0_90_max + g_45_135_max == 0.0) { return vec4(0.0, 0.0, 0.0, 0.0); }\n"
    "    e_0_90 = min(g_0_90_max / (g_0_90_max + g_45_135_max), 1.0);\n"
    "    e_45_135 = 1.0 - e_0_90;\n"
    "    bool c_0_90 = (g_0_90_max > (g_0_90_min * kDetectRatio)) && (g_0_90_max > kDetectThres) && (g_0_90_max > g_45_135_min);\n"
    "    bool c_45_135 = (g_45_135_max > (g_45_135_min * kDetectRatio)) && (g_45_135_max > kDetectThres) && (g_45_135_max > g_0_90_min);\n"
    "    bool c_g_0_90 = g_0_90_max == g_0;\n"
    "    bool c_g_45_135 = g_45_135_max == g_45;\n"
    "    float f_e_0_90 = (c_0_90 && c_45_135) ? e_0_90 : 1.0;\n"
    "    float f_e_45_135 = (c_0_90 && c_45_135) ? e_45_135 : 1.0;\n"
    "    float weight_0 = (c_0_90 && c_g_0_90) ? f_e_0_90 : 0.0;\n"
    "    float weight_90 = (c_0_90 && !c_g_0_90) ? f_e_0_90 : 0.0;\n"
    "    float weight_45 = (c_45_135 && c_g_45_135) ? f_e_45_135 : 0.0;\n"
    "    float weight_135 = (c_45_135 && !c_g_45_135) ? f_e_45_135 : 0.0;\n"
    "    return vec4(weight_0, weight_90, weight_45, weight_135);\n"
    "}\n"
    "#define NIS_BLOCK_WIDTH 32\n"
    "#define NIS_BLOCK_HEIGHT 32\n"
    "#define NIS_THREAD_GROUP_SIZE 256\n"
    "#define kSupportSize 5\n"
    "#define kNumPixelsX (NIS_BLOCK_WIDTH + kSupportSize + 1)\n"
    "#define kNumPixelsY (NIS_BLOCK_HEIGHT + kSupportSize + 1)\n"
    "shared float shPixelsY[kNumPixelsY][kNumPixelsX];\n"
    "float CalcLTIFast(const float y[5]) {\n"
    "    const float a_min = min(min(y[0], y[1]), y[2]);\n"
    "    const float a_max = max(max(y[0], y[1]), y[2]);\n"
    "    const float b_min = min(min(y[2], y[3]), y[4]);\n"
    "    const float b_max = max(max(y[2], y[3]), y[4]);\n"
    "    const float a_cont = a_max - a_min;\n"
    "    const float b_cont = b_max - b_min;\n"
    "    const float cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + kEps);\n"
    "    return (1.0 - saturate((cont_ratio - kMinContrastRatio) * kRatioNorm)) * kContrastBoost;\n"
    "}\n"
    "float EvalUSM(const float pxl[5], const float sharpnessStrength, const float sharpnessLimit) {\n"
    "    float y_usm = -0.6001 * pxl[1] + 1.2002 * pxl[2] - 0.6001 * pxl[3];\n"
    "    y_usm *= sharpnessStrength;\n"
    "    y_usm = min(sharpnessLimit, max(-sharpnessLimit, y_usm));\n"
    "    y_usm *= CalcLTIFast(pxl);\n"
    "    return y_usm;\n"
    "}\n"
    "vec4 GetDirUSM(const float p[5][5]) {\n"
    "    const float scaleY = 1.0 - saturate((p[2][2] - kSharpStartY) * kSharpScaleY);\n"
    "    const float sharpnessStrength = scaleY * kSharpStrengthScale + kSharpStrengthMin;\n"
    "    const float sharpnessLimit = (scaleY * kSharpLimitScale + kSharpLimitMin) * p[2][2];\n"
    "    vec4 rval;\n"
    "    float interp0Deg[5];\n"
    "    for (int i = 0; i < 5; ++i) { interp0Deg[i] = p[i][2]; }\n"
    "    rval.x = EvalUSM(interp0Deg, sharpnessStrength, sharpnessLimit);\n"
    "    float interp90Deg[5];\n"
    "    for (int i = 0; i < 5; ++i) { interp90Deg[i] = p[2][i]; }\n"
    "    rval.y = EvalUSM(interp90Deg, sharpnessStrength, sharpnessLimit);\n"
    "    float interp45Deg[5];\n"
    "    interp45Deg[0] = p[1][1];\n"
    "    interp45Deg[1] = lerp(p[2][1], p[1][2], 0.5);\n"
    "    interp45Deg[2] = p[2][2];\n"
    "    interp45Deg[3] = lerp(p[3][2], p[2][3], 0.5);\n"
    "    interp45Deg[4] = p[3][3];\n"
    "    rval.z = EvalUSM(interp45Deg, sharpnessStrength, sharpnessLimit);\n"
    "    float interp135Deg[5];\n"
    "    interp135Deg[0] = p[3][1];\n"
    "    interp135Deg[1] = lerp(p[3][2], p[2][1], 0.5);\n"
    "    interp135Deg[2] = p[2][2];\n"
    "    interp135Deg[3] = lerp(p[2][3], p[1][2], 0.5);\n"
    "    interp135Deg[4] = p[1][3];\n"
    "    rval.w = EvalUSM(interp135Deg, sharpnessStrength, sharpnessLimit);\n"
    "    return rval;\n"
    "}\n"
    "void NVSharpen(uvec2 blockIdx, uint threadIdx) {\n"
    "    const int dstBlockX = int(NIS_BLOCK_WIDTH * blockIdx.x);\n"
    "    const int dstBlockY = int(NIS_BLOCK_HEIGHT * blockIdx.y);\n"
    "    const float kShift = 0.5 - kSupportSize / 2;\n"
    "    for (int i = int(threadIdx) * 2; i < kNumPixelsX * kNumPixelsY / 2; i += NIS_THREAD_GROUP_SIZE * 2) {\n"
    "        uvec2 pos = uvec2(uint(i) % uint(kNumPixelsX), uint(i) / uint(kNumPixelsX) * 2u);\n"
    "        for (int dy = 0; dy < 2; dy++) {\n"
    "            for (int dx = 0; dx < 2; dx++) {\n"
    "                const float tx = (dstBlockX + pos.x + dx + kShift) * kSrcNormX;\n"
    "                const float ty = (dstBlockY + pos.y + dy + kShift) * kSrcNormY;\n"
    "                const vec4 px4 = textureLod(in_texture, vec2(tx, ty), 0.0);\n"
    "                shPixelsY[pos.y + dy][pos.x + dx] = getY(px4.xyz);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    groupMemoryBarrier(); barrier();\n"
    "    for (int k = int(threadIdx); k < NIS_BLOCK_WIDTH * NIS_BLOCK_HEIGHT; k += NIS_THREAD_GROUP_SIZE) {\n"
    "        const ivec2 pos = ivec2(k % NIS_BLOCK_WIDTH, k / NIS_BLOCK_WIDTH);\n"
    "        float p[5][5];\n"
    "        for (int i = 0; i < 5; ++i) { for (int j = 0; j < 5; ++j) { p[i][j] = shPixelsY[pos.y + i][pos.x + j]; } }\n"
    "        vec4 dirUSM = GetDirUSM(p);\n"
    "        vec4 w = GetEdgeMap(p, kSupportSize / 2 - 1, kSupportSize / 2 - 1);\n"
    "        const float usmY = (dirUSM.x * w.x + dirUSM.y * w.y + dirUSM.z * w.z + dirUSM.w * w.w);\n"
    "        const int dstX = dstBlockX + pos.x;\n"
    "        const int dstY = dstBlockY + pos.y;\n"
    "        vec2 coord = vec2((dstX + 0.5) * kSrcNormX, (dstY + 0.5) * kSrcNormY);\n"
    "        ivec2 dstCoord = ivec2(dstX, dstY);\n"
    "        vec4 op = textureLod(in_texture, coord, 0.0);\n"
    "        op.x += usmY; op.y += usmY; op.z += usmY;\n"
    "        imageStore(out_texture, dstCoord, op);\n"
    "    }\n"
    "}\n"
    "layout(local_size_x = NIS_THREAD_GROUP_SIZE) in;\n"
    "void main() { NVSharpen(gl_WorkGroupID.xy, gl_LocalInvocationID.x); }\n";

// GL constants used here, defined by hand rather than pulling in <gl/gl.h> - see Global
// Constraints in the plan / wrapper.cpp's header comment for why.
const unsigned int GL_VIEWPORT                 = 0x0BA2;
const unsigned int GL_BACK                     = 0x0405;
const unsigned int GL_TEXTURE_2D               = 0x0DE1;
const unsigned int GL_TEXTURE0                 = 0x84C0;
const unsigned int GL_TEXTURE1                 = 0x84C1;
const unsigned int GL_ACTIVE_TEXTURE           = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D       = 0x8069;
const unsigned int GL_TEXTURE_MIN_FILTER       = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER       = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S           = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T           = 0x2803;
const unsigned int GL_LINEAR                   = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE            = 0x812F;
const unsigned int GL_RGBA8                    = 0x8058;
const unsigned int GL_RGBA32F                  = 0x8814;
const unsigned int GL_RGBA                     = 0x1908;
const unsigned int GL_FLOAT                    = 0x1406;
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
const unsigned int GL_SHADER_STORAGE_BARRIER_BIT = 0x00002000;
const unsigned int GL_UNIFORM_BARRIER_BIT      = 0x00000004;
const unsigned int GL_UNIFORM_BUFFER           = 0x8A11;
const unsigned int GL_UNIFORM_BUFFER_BINDING   = 0x8A28;
const unsigned int GL_STATIC_DRAW              = 0x88E4;
const unsigned int GL_DYNAMIC_DRAW             = 0x88E8;
const unsigned int GL_NO_ERROR                 = 0;

enum class NisVariant { Scaler, Sharpen };

struct NisPipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;

    bool resourcesValid = false;
    int width = 0;
    int height = 0;
    unsigned int inputTexture = 0;
    unsigned int outputTexture = 0;
    unsigned int outputFbo = 0;
    unsigned int coefScaleTexture = 0;
    unsigned int coefUsmTexture = 0;
    unsigned int configUbo = 0;
};

NisPipelineState g_scalerState;
NisPipelineState g_sharpenState;

// CompileAndLink, EnsureCoefTexture, EnsureTextures, and RunNisPipeline below are
// deliberately generic over NisVariant so ApplyNVScaler/ApplyNVSharpen share one
// implementation - mirrors bilinear_upscale.cpp's structure, extended with the coefficient
// textures and UBO that pipeline didn't need.

bool CompileAndLink(const GlComputeApi& gl, const char* source, const char* effectName, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &source, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[4096];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] nis_effect: FAILED to compile %s compute shader: %s\n", effectName, log);
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
        printf("[opengl32_enh_cpp] nis_effect: FAILED to link %s compute program: %s\n", effectName, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

// Uploads a [64][8]-float coefficient table (kPhaseCount x kFilterSize, from nis_config.h)
// into a 2x64 RGBA32F texture: each row of 8 floats becomes 2 vec4 texels, matching
// LoadFilterBanksSh's texelFetch(coef, ivec2(vIdx, phase), 0) indexing in the shader.
unsigned int CreateCoefTexture(const GlComputeApi& gl, const float table[64][8]) {
    unsigned int tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 64);
    // table[phase] is 8 floats = 2 vec4 texels (x=[0..3], y=[4..7]); upload directly, the
    // float layout already matches RGBA32F row-major with width=2.
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 64, GL_RGBA, GL_FLOAT, table);
    return tex;
}

void EnsureResources(const GlComputeApi& gl, NisPipelineState& state, int width, int height) {
    if (state.resourcesValid && state.width == width && state.height == height) {
        return;
    }

    if (state.resourcesValid) {
        unsigned int textures[2] = {state.inputTexture, state.outputTexture};
        gl.glDeleteTextures(2, textures);
        gl.glDeleteFramebuffers(1, &state.outputFbo);
        state.resourcesValid = false;
    }

    unsigned int textures[2] = {0, 0};
    gl.glGenTextures(2, textures);
    unsigned int inputTexture = textures[0];
    unsigned int outputTexture = textures[1];

    gl.glBindTexture(GL_TEXTURE_2D, inputTexture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    gl.glBindTexture(GL_TEXTURE_2D, outputTexture);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    unsigned int fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    state.inputTexture = inputTexture;
    state.outputTexture = outputTexture;
    state.outputFbo = fbo;
    state.width = width;
    state.height = height;
    state.resourcesValid = true;
}

// One-time (never resized) resources: coefficient textures (NVScaler only) and the config
// UBO. Created lazily alongside the shader program on first successful compile.
void EnsureStaticResources(const GlComputeApi& gl, NisPipelineState& state, NisVariant variant) {
    if (variant == NisVariant::Scaler && state.coefScaleTexture == 0) {
        state.coefScaleTexture = CreateCoefTexture(gl, coef_scale);
        state.coefUsmTexture = CreateCoefTexture(gl, coef_usm);
    }
    if (state.configUbo == 0) {
        gl.glGenBuffers(1, &state.configUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, state.configUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(NISConfig), nullptr, GL_DYNAMIC_DRAW);
    }
}

void RunNisPipeline(NisPipelineState& state, NisVariant variant, const char* effectName, const char* shaderSource, float sharpness) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warnedScaler = false;
        static bool warnedSharpen = false;
        bool& warned = (variant == NisVariant::Scaler) ? warnedScaler : warnedSharpen;
        if (!warned) {
            printf("[opengl32_enh_cpp] nis_effect: GL 4.3 compute support unavailable on this "
                   "context, %s disabled\n", effectName);
            warned = true;
        }
        return;
    }

    if (!state.initTried) {
        state.initTried = true;
        state.initOk = CompileAndLink(gl, shaderSource, effectName, state.program);
        if (!state.initOk) {
            printf("[opengl32_enh_cpp] nis_effect: %s shader init failed, effect disabled for "
                   "the rest of this process\n", effectName);
        }
    }
    if (!state.initOk) {
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
    int savedTextureBinding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding);
    int savedProgram = 0;
    gl.glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    int savedDrawFbo = 0;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFbo);
    int savedUniformBuffer = 0;
    gl.glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &savedUniformBuffer);

    EnsureResources(gl, state, width, height);
    EnsureStaticResources(gl, state, variant);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, (unsigned int)savedUniformBuffer);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, (unsigned int)savedUniformBuffer);
    };

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, state.inputTexture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    unsigned int captureErr = gl.glGetError();
    if (captureErr != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] nis_effect: glGetError() = 0x%04X after capture (%s), "
               "skipping this frame\n", captureErr, effectName);
        restoreState();
        return;
    }

    // Build the NISConfig for this frame's dimensions/sharpness and upload it to the UBO.
    // Same viewport used as both input and output (no real reduced-resolution pipeline
    // exists yet - see the spec's "Non-goals"), so every NVScalerUpdateConfig call here
    // passes width/height for both the input and output viewport/texture arguments.
    NISConfig config{};
    bool configOk;
    if (variant == NisVariant::Scaler) {
        configOk = NVScalerUpdateConfig(config, sharpness,
            0, 0, (uint32_t)width, (uint32_t)height, (uint32_t)width, (uint32_t)height,
            0, 0, (uint32_t)width, (uint32_t)height, (uint32_t)width, (uint32_t)height,
            NISHDRMode::None);
    } else {
        configOk = NVSharpenUpdateConfig(config, sharpness,
            0, 0, (uint32_t)width, (uint32_t)height, (uint32_t)width, (uint32_t)height,
            0, 0, NISHDRMode::None);
    }
    if (!configOk) {
        printf("[opengl32_enh_cpp] nis_effect: %s config rejected (scale out of [0.5,1] "
               "range), skipping this frame\n", effectName);
        restoreState();
        return;
    }

    gl.glBindBuffer(GL_UNIFORM_BUFFER, state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(NISConfig), &config, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.configUbo);

    gl.glUseProgram(state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, state.inputTexture);
    gl.glBindImageTexture(2, state.outputTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);
    if (variant == NisVariant::Scaler) {
        gl.glActiveTexture(GL_TEXTURE0 + 2);
        gl.glBindTexture(GL_TEXTURE_2D, state.coefScaleTexture);
        gl.glActiveTexture(GL_TEXTURE0 + 3);
        gl.glBindTexture(GL_TEXTURE_2D, state.coefUsmTexture);
        gl.glActiveTexture(GL_TEXTURE0);
    }

    // Dispatch grid: one thread group per NIS_BLOCK_WIDTH x NIS_BLOCK_HEIGHT output block -
    // 32x24 for NVScaler, 32x32 for NVSharpen (see each shader's #define block above).
    unsigned int blockWidth = 32;
    unsigned int blockHeight = (variant == NisVariant::Scaler) ? 24u : 32u;
    unsigned int groupsX = ((unsigned int)width + blockWidth - 1) / blockWidth;
    unsigned int groupsY = ((unsigned int)height + blockHeight - 1) / blockHeight;
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, state.outputFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] nis_effect: glGetError() = 0x%04X after dispatch (%s)\n", err, effectName);
    }

    restoreState();
}

}  // namespace

void ApplyNVScaler(float sharpness) {
    RunNisPipeline(g_scalerState, NisVariant::Scaler, "nvscaler", kNVScalerShaderSource, sharpness);
}

void ApplyNVSharpen(float sharpness) {
    RunNisPipeline(g_sharpenState, NisVariant::Sharpen, "nvsharpen", kNVSharpenShaderSource, sharpness);
}
