// See smaa.h. Hand-ported from the SMAA reference implementation's SMAA.hlsl
// (https://github.com/iryoku/smaa, MIT licensed, Copyright (c) 2013 Jorge Jimenez, Jose I.
// Echevarria, Belen Masia, Fernando Navarro, Diego Gutierrez) - same vendoring treatment as
// nis_effect.cpp's NIS port. Deliberate deviations from the reference:
//   - Compute shaders (imageLoad/imageStore) instead of the reference's fragment-shader
//     vertex+pixel pass split - the per-pixel "vertex shader" offset math is just computed
//     inline at the top of each compute shader's main() instead.
//   - PRESET_HIGH's constants (threshold=0.1, max search steps=16/8 diag, corner rounding=25,
//     diagonal + corner detection ENABLED) are baked in directly - this project doesn't
//     expose an SMAA quality knob, so there is no reason to keep them configurable.
//   - Luma edge detection only (not color or depth) - the reference's recommended default,
//     and the only one this project needs.
//   - No predication, no color edge detection, no temporal reprojection (SMAA T2x) - this
//     project's swap-time pipeline has no velocity buffer or previous-frame color to feed
//     those, same reasoning nis_effect.cpp gives for stripping unused NIS branches.
//   - "Point" texture fetches collapse to the same textureLod(...,0.0) as "linear" fetches:
//     srcTexture arrives already GL_LINEAR-filtered (every stage's input is, per
//     post_effects.cpp's EnsurePipelineTextures), and sampling a linear filter exactly at a
//     texel center (which is what every "point" fetch here does) returns the same value a
//     true point sampler would - so no second point-filtered texture view is needed.
//   - subsampleIndices is always the zero vector: that parameter only matters for SMAA T2x's
//     multiple temporal subsamples, and this is SMAA 1x (single sample per pixel, like every
//     other stage in this pipeline).
//
// Two precomputed lookup textures (AreaTex/SearchTex, vendored verbatim in smaa_area_tex.h/
// smaa_search_tex.h) are required by the blending-weight-calculation pass - see those files'
// header comments.
#include <cstdint>
#include <cstdio>

#include "smaa.h"
#include "gl_loader.h"
#include "smaa_area_tex.h"
#include "smaa_search_tex.h"

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
const unsigned int GL_RED                       = 0x1903;
const unsigned int GL_R8                        = 0x8229;
const unsigned int GL_RG                        = 0x8227;
const unsigned int GL_RG8                       = 0x822B;
const unsigned int GL_UNSIGNED_BYTE             = 0x1401;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_NO_ERROR                  = 0;

// Shared preamble every pass includes: the SmaaConfigBlock UBO (rtMetrics = 1/width, 1/height,
// width, height - exactly SMAA_RT_METRICS from the reference) and the helper every pass uses
// for the "step()" conditional-move idiom the reference calls SMAAMovc.
const char* kSmaaCommonSource =
    "layout(std140, binding = 0) uniform SmaaConfigBlock {\n"
    "    vec4 rtMetrics;\n"
    "};\n"
    "const float SMAA_THRESHOLD = 0.1;\n"
    "const int SMAA_MAX_SEARCH_STEPS = 16;\n"
    "const int SMAA_MAX_SEARCH_STEPS_DIAG = 8;\n"
    "const float SMAA_CORNER_ROUNDING_NORM = 0.25;\n"
    "const float SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR = 2.0;\n";

// Pass 1/3: rec. 709 luma, used by edge detection to compare neighbor contrast.
const char* kSmaaLumaSource =
    "float smaaLuma(vec3 c) {\n"
    "    return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "}\n";

//-----------------------------------------------------------------------------
// Pass 1: Luma Edge Detection - see SMAALumaEdgeDetectionPS in the reference.
const char* kEdgeDetectionShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 1) uniform sampler2D colorTex;\n"
    "layout(rgba16f, binding = 2) uniform writeonly image2D edgesImage;\n";

const char* kEdgeDetectionMainSource =
    "void main() {\n"
    "    ivec2 size = imageSize(edgesImage);\n"
    "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (pos.x >= size.x || pos.y >= size.y) { return; }\n"
    "    vec2 texcoord = (vec2(pos) + 0.5) * rtMetrics.xy;\n"
    "    vec4 offset0 = rtMetrics.xyxy * vec4(-1.0, 0.0, 0.0, -1.0) + texcoord.xyxy;\n"
    "    vec4 offset1 = rtMetrics.xyxy * vec4(1.0, 0.0, 0.0, 1.0) + texcoord.xyxy;\n"
    "    vec4 offset2 = rtMetrics.xyxy * vec4(-2.0, 0.0, 0.0, -2.0) + texcoord.xyxy;\n"
    "\n"
    "    float L = smaaLuma(textureLod(colorTex, texcoord, 0.0).rgb);\n"
    "    float Lleft = smaaLuma(textureLod(colorTex, offset0.xy, 0.0).rgb);\n"
    "    float Ltop  = smaaLuma(textureLod(colorTex, offset0.zw, 0.0).rgb);\n"
    "\n"
    "    vec4 delta;\n"
    "    delta.xy = abs(L - vec2(Lleft, Ltop));\n"
    "    vec2 edges = step(vec2(SMAA_THRESHOLD), delta.xy);\n"
    "\n"
    "    if (edges.x + edges.y == 0.0) {\n"
    "        imageStore(edgesImage, pos, vec4(0.0));\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    float Lright  = smaaLuma(textureLod(colorTex, offset1.xy, 0.0).rgb);\n"
    "    float Lbottom = smaaLuma(textureLod(colorTex, offset1.zw, 0.0).rgb);\n"
    "    delta.zw = abs(L - vec2(Lright, Lbottom));\n"
    "    vec2 maxDelta = max(delta.xy, delta.zw);\n"
    "\n"
    "    float Lleftleft = smaaLuma(textureLod(colorTex, offset2.xy, 0.0).rgb);\n"
    "    float Ltoptop   = smaaLuma(textureLod(colorTex, offset2.zw, 0.0).rgb);\n"
    "    delta.zw = abs(vec2(Lleft, Ltop) - vec2(Lleftleft, Ltoptop));\n"
    "    maxDelta = max(maxDelta, delta.zw);\n"
    "\n"
    "    float finalDelta = max(maxDelta.x, maxDelta.y);\n"
    "    edges *= step(vec2(finalDelta), SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);\n"
    "\n"
    "    imageStore(edgesImage, pos, vec4(edges, 0.0, 0.0));\n"
    "}\n";

//-----------------------------------------------------------------------------
// Pass 2: Blending Weight Calculation - see SMAABlendingWeightCalculationPS and its many
// helper functions in the reference (SMAACalculateDiagWeights, SMAASearchXLeft/Right/YUp/
// YDown, SMAAArea/SMAAAreaDiag, SMAADetectHorizontal/VerticalCornerPattern).
const char* kBlendWeightShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 1) uniform sampler2D edgesTex;\n"
    "layout(binding = 2) uniform sampler2D areaTex;\n"
    "layout(binding = 3) uniform sampler2D searchTex;\n"
    "layout(rgba16f, binding = 4) uniform writeonly image2D blendImage;\n";

const char* kBlendWeightHelpersSource =
    "vec2 smaaDecodeDiagBilinearAccess2(vec2 e) {\n"
    "    e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);\n"
    "    return round(e);\n"
    "}\n"
    "vec4 smaaDecodeDiagBilinearAccess4(vec4 e) {\n"
    "    e.rb = e.rb * abs(5.0 * e.rb - vec2(5.0 * 0.75));\n"
    "    return round(e);\n"
    "}\n"
    "\n"
    "vec2 smaaSearchDiag1(vec2 texcoordIn, vec2 dir, out vec2 e) {\n"
    "    vec4 coord = vec4(texcoordIn, -1.0, 1.0);\n"
    "    vec3 t = vec3(rtMetrics.xy, 1.0);\n"
    "    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9) {\n"
    "        coord.xyz = t * vec3(dir, 1.0) + coord.xyz;\n"
    "        e = textureLod(edgesTex, coord.xy, 0.0).rg;\n"
    "        coord.w = dot(e, vec2(0.5));\n"
    "    }\n"
    "    return coord.zw;\n"
    "}\n"
    "\n"
    "vec2 smaaSearchDiag2(vec2 texcoordIn, vec2 dir, out vec2 e) {\n"
    "    vec4 coord = vec4(texcoordIn, -1.0, 1.0);\n"
    "    coord.x += 0.25 * rtMetrics.x;\n"
    "    vec3 t = vec3(rtMetrics.xy, 1.0);\n"
    "    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9) {\n"
    "        coord.xyz = t * vec3(dir, 1.0) + coord.xyz;\n"
    "        e = textureLod(edgesTex, coord.xy, 0.0).rg;\n"
    "        e = smaaDecodeDiagBilinearAccess2(e);\n"
    "        coord.w = dot(e, vec2(0.5));\n"
    "    }\n"
    "    return coord.zw;\n"
    "}\n"
    "\n"
    "vec2 smaaAreaDiag(vec2 dist, vec2 e, float offsetVal) {\n"
    "    vec2 texcoord = vec2(20.0, 20.0) * e + dist;\n"
    "    texcoord = (1.0 / vec2(160.0, 560.0)) * texcoord + 0.5 * (1.0 / vec2(160.0, 560.0));\n"
    "    texcoord.x += 0.5;\n"
    "    texcoord.y += (1.0 / 7.0) * offsetVal;\n"
    "    return textureLod(areaTex, texcoord, 0.0).rg;\n"
    "}\n"
    "\n"
    "vec2 smaaCalculateDiagWeights(vec2 texcoord, vec2 e) {\n"
    "    vec2 weights = vec2(0.0);\n"
    "    vec4 d;\n"
    "    vec2 end;\n"
    "    if (e.r > 0.0) {\n"
    "        d.xz = smaaSearchDiag1(texcoord, vec2(-1.0, 1.0), end);\n"
    "        d.x += float(end.y > 0.9);\n"
    "    } else {\n"
    "        d.xz = vec2(0.0);\n"
    "    }\n"
    "    d.yw = smaaSearchDiag1(texcoord, vec2(1.0, -1.0), end);\n"
    "\n"
    "    if (d.x + d.y > 2.0) {\n"
    "        vec4 coords = vec4(-d.x + 0.25, d.x, d.y, -d.y - 0.25) * rtMetrics.xyxy + texcoord.xyxy;\n"
    "        vec4 c;\n"
    "        c.xy = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(-1, 0)).rg;\n"
    "        c.zw = textureLodOffset(edgesTex, coords.zw, 0.0, ivec2(1, 0)).rg;\n"
    "        c.yxwz = smaaDecodeDiagBilinearAccess4(c);\n"
    "        vec2 cc = 2.0 * c.xz + c.yw;\n"
    "        if (step(0.9, d.z) > 0.0) { cc.x = 0.0; }\n"
    "        if (step(0.9, d.w) > 0.0) { cc.y = 0.0; }\n"
    "        weights += smaaAreaDiag(d.xy, cc, 0.0);\n"
    "    }\n"
    "\n"
    "    d.xz = smaaSearchDiag2(texcoord, vec2(-1.0, -1.0), end);\n"
    "    if (textureLodOffset(edgesTex, texcoord, 0.0, ivec2(1, 0)).r > 0.0) {\n"
    "        d.yw = smaaSearchDiag2(texcoord, vec2(1.0, 1.0), end);\n"
    "        d.y += float(end.y > 0.9);\n"
    "    } else {\n"
    "        d.yw = vec2(0.0);\n"
    "    }\n"
    "\n"
    "    if (d.x + d.y > 2.0) {\n"
    "        vec4 coords = vec4(-d.x, -d.x, d.y, d.y) * rtMetrics.xyxy + texcoord.xyxy;\n"
    "        vec4 c;\n"
    "        c.x = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(-1, 0)).g;\n"
    "        c.y = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(0, -1)).r;\n"
    "        c.zw = textureLodOffset(edgesTex, coords.zw, 0.0, ivec2(1, 0)).gr;\n"
    "        vec2 cc = 2.0 * c.xz + c.yw;\n"
    "        if (step(0.9, d.z) > 0.0) { cc.x = 0.0; }\n"
    "        if (step(0.9, d.w) > 0.0) { cc.y = 0.0; }\n"
    "        weights += smaaAreaDiag(d.xy, cc, 0.0).gr;\n"
    "    }\n"
    "\n"
    "    return weights;\n"
    "}\n"
    "\n"
    "float smaaSearchLength(vec2 e, float offsetVal) {\n"
    "    vec2 scale = vec2(66.0, 33.0) * vec2(0.5, -1.0);\n"
    "    vec2 bias = vec2(66.0, 33.0) * vec2(offsetVal, 1.0);\n"
    "    scale += vec2(-1.0, 1.0);\n"
    "    bias += vec2(0.5, -0.5);\n"
    "    scale *= 1.0 / vec2(64.0, 16.0);\n"
    "    bias *= 1.0 / vec2(64.0, 16.0);\n"
    "    return textureLod(searchTex, scale * e + bias, 0.0).r;\n"
    "}\n"
    "\n"
    "float smaaSearchXLeft(vec2 texcoordIn, float end) {\n"
    "    vec2 texcoord = texcoordIn;\n"
    "    vec2 e = vec2(0.0, 1.0);\n"
    "    while (texcoord.x > end && e.g > 0.8281 && e.r == 0.0) {\n"
    "        e = textureLod(edgesTex, texcoord, 0.0).rg;\n"
    "        texcoord -= vec2(2.0, 0.0) * rtMetrics.xy;\n"
    "    }\n"
    "    float offsetVal = -(255.0 / 127.0) * smaaSearchLength(e, 0.0) + 3.25;\n"
    "    return rtMetrics.x * offsetVal + texcoord.x;\n"
    "}\n"
    "\n"
    "float smaaSearchXRight(vec2 texcoordIn, float end) {\n"
    "    vec2 texcoord = texcoordIn;\n"
    "    vec2 e = vec2(0.0, 1.0);\n"
    "    while (texcoord.x < end && e.g > 0.8281 && e.r == 0.0) {\n"
    "        e = textureLod(edgesTex, texcoord, 0.0).rg;\n"
    "        texcoord += vec2(2.0, 0.0) * rtMetrics.xy;\n"
    "    }\n"
    "    float offsetVal = -(255.0 / 127.0) * smaaSearchLength(e, 0.5) + 3.25;\n"
    "    return -rtMetrics.x * offsetVal + texcoord.x;\n"
    "}\n"
    "\n"
    "float smaaSearchYUp(vec2 texcoordIn, float end) {\n"
    "    vec2 texcoord = texcoordIn;\n"
    "    vec2 e = vec2(1.0, 0.0);\n"
    "    while (texcoord.y > end && e.r > 0.8281 && e.g == 0.0) {\n"
    "        e = textureLod(edgesTex, texcoord, 0.0).rg;\n"
    "        texcoord -= vec2(0.0, 2.0) * rtMetrics.xy;\n"
    "    }\n"
    "    float offsetVal = -(255.0 / 127.0) * smaaSearchLength(e.gr, 0.0) + 3.25;\n"
    "    return rtMetrics.y * offsetVal + texcoord.y;\n"
    "}\n"
    "\n"
    "float smaaSearchYDown(vec2 texcoordIn, float end) {\n"
    "    vec2 texcoord = texcoordIn;\n"
    "    vec2 e = vec2(1.0, 0.0);\n"
    "    while (texcoord.y < end && e.r > 0.8281 && e.g == 0.0) {\n"
    "        e = textureLod(edgesTex, texcoord, 0.0).rg;\n"
    "        texcoord += vec2(0.0, 2.0) * rtMetrics.xy;\n"
    "    }\n"
    "    float offsetVal = -(255.0 / 127.0) * smaaSearchLength(e.gr, 0.5) + 3.25;\n"
    "    return -rtMetrics.y * offsetVal + texcoord.y;\n"
    "}\n"
    "\n"
    "vec2 smaaArea(vec2 dist, float e1, float e2, float offsetVal) {\n"
    "    vec2 texcoord = vec2(16.0, 16.0) * round(4.0 * vec2(e1, e2)) + dist;\n"
    "    texcoord = (1.0 / vec2(160.0, 560.0)) * texcoord + 0.5 * (1.0 / vec2(160.0, 560.0));\n"
    "    texcoord.y += (1.0 / 7.0) * offsetVal;\n"
    "    return textureLod(areaTex, texcoord, 0.0).rg;\n"
    "}\n"
    "\n"
    "void smaaDetectHorizontalCornerPattern(inout vec2 weights, vec4 texcoord, vec2 d) {\n"
    "    vec2 leftRight = step(d.xy, d.yx);\n"
    "    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;\n"
    "    rounding /= (leftRight.x + leftRight.y);\n"
    "    vec2 factor = vec2(1.0);\n"
    "    factor.x -= rounding.x * textureLodOffset(edgesTex, texcoord.xy, 0.0, ivec2(0, 1)).r;\n"
    "    factor.x -= rounding.y * textureLodOffset(edgesTex, texcoord.zw, 0.0, ivec2(1, 1)).r;\n"
    "    factor.y -= rounding.x * textureLodOffset(edgesTex, texcoord.xy, 0.0, ivec2(0, -2)).r;\n"
    "    factor.y -= rounding.y * textureLodOffset(edgesTex, texcoord.zw, 0.0, ivec2(1, -2)).r;\n"
    "    weights *= clamp(factor, 0.0, 1.0);\n"
    "}\n"
    "\n"
    "void smaaDetectVerticalCornerPattern(inout vec2 weights, vec4 texcoord, vec2 d) {\n"
    "    vec2 leftRight = step(d.xy, d.yx);\n"
    "    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;\n"
    "    rounding /= (leftRight.x + leftRight.y);\n"
    "    vec2 factor = vec2(1.0);\n"
    "    factor.x -= rounding.x * textureLodOffset(edgesTex, texcoord.xy, 0.0, ivec2(1, 0)).g;\n"
    "    factor.x -= rounding.y * textureLodOffset(edgesTex, texcoord.zw, 0.0, ivec2(1, 1)).g;\n"
    "    factor.y -= rounding.x * textureLodOffset(edgesTex, texcoord.xy, 0.0, ivec2(-2, 0)).g;\n"
    "    factor.y -= rounding.y * textureLodOffset(edgesTex, texcoord.zw, 0.0, ivec2(-2, 1)).g;\n"
    "    weights *= clamp(factor, 0.0, 1.0);\n"
    "}\n";

const char* kBlendWeightMainSource =
    "void main() {\n"
    "    ivec2 size = imageSize(blendImage);\n"
    "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (pos.x >= size.x || pos.y >= size.y) { return; }\n"
    "    vec2 texcoord = (vec2(pos) + 0.5) * rtMetrics.xy;\n"
    "    vec2 pixcoord = texcoord * rtMetrics.zw;\n"
    "\n"
    "    vec4 offset0 = rtMetrics.xyxy * vec4(-0.25, -0.125, 1.25, -0.125) + texcoord.xyxy;\n"
    "    vec4 offset1 = rtMetrics.xyxy * vec4(-0.125, -0.25, -0.125, 1.25) + texcoord.xyxy;\n"
    "    vec4 offset2 = rtMetrics.xxyy * (vec4(-2.0, 2.0, -2.0, 2.0) * float(SMAA_MAX_SEARCH_STEPS))\n"
    "        + vec4(offset0.xz, offset1.yw);\n"
    "\n"
    "    vec4 weights = vec4(0.0);\n"
    "    vec2 e = textureLod(edgesTex, texcoord, 0.0).rg;\n"
    "\n"
    "    if (e.g > 0.0) {\n"
    "        weights.rg = smaaCalculateDiagWeights(texcoord, e);\n"
    "        if (weights.r == -weights.g) {\n"
    "            vec2 d;\n"
    "            vec3 coords;\n"
    "            coords.x = smaaSearchXLeft(offset0.xy, offset2.x);\n"
    "            coords.y = offset1.y;\n"
    "            d.x = coords.x;\n"
    "            float e1 = textureLod(edgesTex, coords.xy, 0.0).r;\n"
    "            coords.z = smaaSearchXRight(offset0.zw, offset2.y);\n"
    "            d.y = coords.z;\n"
    "            d = abs(round(rtMetrics.zz * d - pixcoord.xx));\n"
    "            vec2 sqrt_d = sqrt(d);\n"
    "            float e2 = textureLodOffset(edgesTex, coords.zy, 0.0, ivec2(1, 0)).r;\n"
    "            weights.rg = smaaArea(sqrt_d, e1, e2, 0.0);\n"
    "            coords.y = texcoord.y;\n"
    "            smaaDetectHorizontalCornerPattern(weights.rg, vec4(coords.x, coords.y, coords.z, coords.y), d);\n"
    "        } else {\n"
    "            e.r = 0.0;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    if (e.r > 0.0) {\n"
    "        vec2 d;\n"
    "        vec3 coords;\n"
    "        coords.y = smaaSearchYUp(offset1.xy, offset2.z);\n"
    "        coords.x = offset0.x;\n"
    "        d.x = coords.y;\n"
    "        float e1 = textureLod(edgesTex, coords.xy, 0.0).g;\n"
    "        coords.z = smaaSearchYDown(offset1.zw, offset2.w);\n"
    "        d.y = coords.z;\n"
    "        d = abs(round(rtMetrics.ww * d - pixcoord.yy));\n"
    "        vec2 sqrt_d = sqrt(d);\n"
    "        float e2 = textureLodOffset(edgesTex, coords.xz, 0.0, ivec2(0, 1)).g;\n"
    "        weights.ba = smaaArea(sqrt_d, e1, e2, 0.0);\n"
    "        coords.x = texcoord.x;\n"
    "        smaaDetectVerticalCornerPattern(weights.ba, vec4(coords.x, coords.y, coords.x, coords.z), d);\n"
    "    }\n"
    "\n"
    "    imageStore(blendImage, pos, weights);\n"
    "}\n";

//-----------------------------------------------------------------------------
// Pass 3: Neighborhood Blending - see SMAANeighborhoodBlendingPS in the reference.
const char* kNeighborhoodShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 1) uniform sampler2D colorTex;\n"
    "layout(binding = 2) uniform sampler2D blendTex;\n"
    "layout(rgba16f, binding = 3) uniform writeonly image2D outImage;\n";

const char* kNeighborhoodMainSource =
    "void main() {\n"
    "    ivec2 size = imageSize(outImage);\n"
    "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (pos.x >= size.x || pos.y >= size.y) { return; }\n"
    "    vec2 texcoord = (vec2(pos) + 0.5) * rtMetrics.xy;\n"
    "    vec4 offset = rtMetrics.xyxy * vec4(1.0, 0.0, 0.0, 1.0) + texcoord.xyxy;\n"
    "\n"
    "    vec4 a;\n"
    "    a.x = textureLod(blendTex, offset.xy, 0.0).a;\n"
    "    a.y = textureLod(blendTex, offset.zw, 0.0).g;\n"
    "    a.wz = textureLod(blendTex, texcoord, 0.0).xz;\n"
    "\n"
    "    vec4 outColor;\n"
    "    if (dot(a, vec4(1.0)) < 1e-5) {\n"
    "        outColor = textureLod(colorTex, texcoord, 0.0);\n"
    "    } else {\n"
    "        bool h = max(a.x, a.z) > max(a.y, a.w);\n"
    "        vec4 blendingOffset = h ? vec4(a.x, 0.0, a.z, 0.0) : vec4(0.0, a.y, 0.0, a.w);\n"
    "        vec2 blendingWeight = h ? a.xz : a.yw;\n"
    "        blendingWeight /= dot(blendingWeight, vec2(1.0));\n"
    "\n"
    "        vec4 blendingCoord = blendingOffset * vec4(rtMetrics.xy, -rtMetrics.xy) + texcoord.xyxy;\n"
    "\n"
    "        outColor = blendingWeight.x * textureLod(colorTex, blendingCoord.xy, 0.0);\n"
    "        outColor += blendingWeight.y * textureLod(colorTex, blendingCoord.zw, 0.0);\n"
    "    }\n"
    "\n"
    "    imageStore(outImage, pos, outColor);\n"
    "}\n";

struct SmaaConfigData {
    float rtMetrics[4];
};

struct SmaaState {
    bool initTried = false;
    bool initOk = false;
    unsigned int edgeProgram = 0;
    unsigned int blendProgram = 0;
    unsigned int neighborProgram = 0;
    unsigned int configUbo = 0;

    unsigned int areaTex = 0;
    unsigned int searchTex = 0;

    bool scratchValid = false;
    int width = 0;
    int height = 0;
    unsigned int edgesTex = 0;
    unsigned int blendTex = 0;
};

SmaaState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* const* sources, int sourceCount,
                     const char* passName, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, sourceCount, sources, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[4096];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] smaa: FAILED to compile %s compute shader: %s\n", passName, log);
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
        printf("[opengl32_enh_cpp] smaa: FAILED to link %s compute program: %s\n", passName, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

bool CompileAllPrograms(const GlComputeApi& gl, SmaaState& state) {
    const char* edgeSources[] = {kEdgeDetectionShaderSource, kSmaaCommonSource, kSmaaLumaSource,
                                 kEdgeDetectionMainSource};
    if (!CompileAndLink(gl, edgeSources, 4, "edge detection", state.edgeProgram)) {
        return false;
    }

    const char* blendSources[] = {kBlendWeightShaderSource, kSmaaCommonSource,
                                   kBlendWeightHelpersSource, kBlendWeightMainSource};
    if (!CompileAndLink(gl, blendSources, 4, "blending weight calculation", state.blendProgram)) {
        return false;
    }

    const char* neighborSources[] = {kNeighborhoodShaderSource, kSmaaCommonSource,
                                      kNeighborhoodMainSource};
    if (!CompileAndLink(gl, neighborSources, 3, "neighborhood blending", state.neighborProgram)) {
        return false;
    }

    return true;
}

unsigned int MakeLookupTexture(const GlComputeApi& gl, unsigned int internalFormat,
                                unsigned int format, int width, int height, const void* bytes) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE,
                        const_cast<void*>(bytes));
    return texture;
}

void EnsureStaticResources(const GlComputeApi& gl, SmaaState& state) {
    if (state.areaTex == 0) {
        state.areaTex = MakeLookupTexture(gl, GL_RG8, GL_RG, AREATEX_WIDTH, AREATEX_HEIGHT,
                                           kSmaaAreaTexBytes);
    }
    if (state.searchTex == 0) {
        state.searchTex = MakeLookupTexture(gl, GL_R8, GL_RED, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT,
                                             kSmaaSearchTexBytes);
    }
    if (state.configUbo == 0) {
        gl.glGenBuffers(1, &state.configUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, state.configUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SmaaConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
}

unsigned int MakeScratchTexture(const GlComputeApi& gl, int width, int height) {
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

void EnsureScratchTextures(const GlComputeApi& gl, SmaaState& state, int width, int height) {
    if (state.scratchValid && state.width == width && state.height == height) {
        return;
    }
    if (state.scratchValid) {
        unsigned int textures[2] = {state.edgesTex, state.blendTex};
        gl.glDeleteTextures(2, textures);
    }
    state.edgesTex = MakeScratchTexture(gl, width, height);
    state.blendTex = MakeScratchTexture(gl, width, height);
    state.width = width;
    state.height = height;
    state.scratchValid = true;
}

}  // namespace

bool ApplySmaa(unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] smaa: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAllPrograms(gl, g_state);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] smaa: shader init failed, effect disabled for the rest "
                   "of this process\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureStaticResources(gl, g_state);
    EnsureScratchTextures(gl, g_state, width, height);

    SmaaConfigData configData{};
    configData.rtMetrics[0] = 1.0f / (float)width;
    configData.rtMetrics[1] = 1.0f / (float)height;
    configData.rtMetrics[2] = (float)width;
    configData.rtMetrics[3] = (float)height;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SmaaConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);

    // Pass 1: edge detection - colorTex (srcTexture) -> edgesTex.
    gl.glUseProgram(g_state.edgeProgram);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(2, g_state.edgesTex, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    // Pass 2: blending weight calculation - edgesTex (+ area/search lookups) -> blendTex.
    gl.glUseProgram(g_state.blendProgram);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.edgesTex);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.areaTex);
    gl.glActiveTexture(GL_TEXTURE0 + 3);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.searchTex);
    gl.glBindImageTexture(4, g_state.blendTex, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    // Pass 3: neighborhood blending - colorTex (srcTexture) + blendTex -> dstTexture.
    gl.glUseProgram(g_state.neighborProgram);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.blendTex);
    gl.glBindImageTexture(3, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] smaa: glGetError() = 0x%04X after dispatch\n", err);
        return false;
    }
    return true;
}
