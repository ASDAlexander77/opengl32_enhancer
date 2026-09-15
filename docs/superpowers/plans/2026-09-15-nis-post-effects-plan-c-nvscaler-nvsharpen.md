# NVScaler/NVSharpen (NIS shader port) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `effect=nvscaler` and `effect=nvsharpen` in `anax_enhancer.ini` actually run NVIDIA's NIS edge-adaptive scale+sharpen and sharpen-only compute shaders, replacing the "not implemented yet" log-and-fallback in `post_effects.cpp`.

**Architecture:** Reuse the GPU capture/compute/blit pipeline pattern proven in Plan B's `bilinear_upscale.cpp` (GPU-to-GPU capture via `glCopyTexSubImage2D`, no CPU readback, careful FBO/state save-restore). Vendor NVIDIA's portable `NISConfig` header unmodified, hand-port the two NIS GLSL compute shaders (`NIS_SCALER=1`/`=0`) from Vulkan-flavored GLSL to desktop GLSL 430 core as embedded string literals, and add one new coefficient-texture + UBO setup step the bilinear pipeline didn't need.

**Tech Stack:** C++17, GLSL 430 core compute shaders, existing `gl_loader.h`'s `GlComputeApi` (all needed entry points already resolved by Plan A/B — no loader changes needed this time).

**Spec:** `docs/superpowers/specs/2026-09-15-nis-post-effects-design.md`

**Reference sources (read-only, outside this repo — do not modify, do not commit copies beyond what each task explicitly vendors):**
- `E:\Gits\NVIDIAImageScaling\NIS\NIS_Config.h` — the struct/functions Task 1 vendors
- `E:\Gits\NVIDIAImageScaling\NIS\NIS_Main.glsl` and `NIS\NIS_Scaler.h` — the Vulkan GLSL Task 2's shader text is hand-ported from (for cross-checking only; Task 2's brief already contains the complete ported result, so reading these is optional, not required, to complete the task)

## Global Constraints

- Never `#include <windows.h>` or `<gl/gl.h>`/`<GL/gl.h>` in any `.cpp`/`.h` that compiles into the `opengl32_enh32`/`opengl32_enh64` proxy DLL (collides with the proxy's own `dllexport`ed `wgl*`/`gl*` names). Test executables (not linked into the DLL) are exempt. GL enum values needed and not already in `gl_loader.h` are defined by hand as `const unsigned int`, matching `bilinear_upscale.cpp`'s existing pattern.
- Near-term priority is the `x86`/`opengl32_enh32` build (per project priority — 64-bit TSLANG port is future work). Every new `.cpp` file must be added to `CMakeLists.txt`'s `add_library(${WRAPPER_NAME_CXX} ...)` sources line, which is shared by both the `x86` and `default` (x64) presets — do not touch the `CMAKE_SIZEOF_VOID_P EQUAL 8`-gated TSLANG block.
- All new GL entry points this plan needs (`glGenBuffers`, `glBindBuffer`, `glBindBufferBase`, `glBufferData`, `glTexImage2D`... — check against the list below) must already exist in `gl_loader.h`'s `GlComputeApi`; if a task discovers one genuinely missing, that is a plan defect — rule on it via the ledger (add the entry point to `gl_loader.h`/`.cpp` following the existing pattern) rather than working around it.
- Every failure mode (shader compile/link error, `glGetError()` after dispatch) logs once via `printf("[opengl32_enh_cpp] ...")` and disables the effect for the rest of the process — never crashes or hangs the host app. Mirror `bilinear_upscale.cpp`'s `PipelineState`/`initTried`/`initOk` pattern exactly.
- NVIDIA's MIT license header must be preserved verbatim at the top of every vendored/ported file (`nis_config.h`, and the two embedded GLSL shader strings via a comment block above the string literal).
- **Deviation from the spec's literal shader text (ruled by the plan author, not re-litigated per-task):** the upstream Vulkan GLSL declares `in_texture`/`coef_scaler`/`coef_usm` as separate `texture2D` opaque uniforms plus one shared `sampler` uniform (`samplerLinearClamp`), combined at use-site via the `sampler2D(texture, sampler)` constructor — a Vulkan-style separate-texture-and-sampler-object pattern. This plan uses plain combined `sampler2D` uniforms instead (texture filtering set once via `glTexParameteri` at texture-creation time, exactly like `bilinear_upscale.cpp` already does), because: (a) it avoids adding `glGenSamplers`/`glBindSampler`/`glSamplerParameteri` to `gl_loader.h` for a single call site, (b) every texture this pipeline creates only ever needs one fixed sampling mode (`GL_LINEAR`/`GL_CLAMP_TO_EDGE`) for its whole lifetime, so a separate sampler object buys nothing here, and (c) it matches this codebase's existing bilinear pipeline. This is a pure implementation-detail substitution — the sampling math (`textureLod`/`texelFetch` call sites) is otherwise a direct line-for-line port.
- **Deviation:** the upstream shader source is written against an `NVF`/`NVI`/`NVU`/`NVH`-style macro layer that abstracts over HLSL and GLSL simultaneously (see `NIS_Scaler.h`'s type-alias `#define`s). Since this port only ever targets desktop GLSL, Task 2's ported shader text drops that macro layer and uses native GLSL types (`float`, `int`, `uint`, `vec2`/`vec3`/`vec4`, `ivec2`, `uvec2`, `bool`) directly. The two thin helper macros with no native GLSL equivalent (`saturate`, `lerp`) are kept.
- **Deviation:** every dead branch implied by this project's fixed configuration is stripped from the ported shader text rather than kept behind `#if`: `NIS_HDR_MODE=NIS_HDR_MODE_NONE` (no HDR), `NIS_VIEWPORT_SUPPORT=0` (always the whole texture), `NIS_NV12_SUPPORT=0` (RGBA8 only, matching this pipeline's capture format), `NIS_TEXTURE_GATHER=0` (the non-gather sampling path), `NIS_CLAMP_OUTPUT=0` (no output clamp — matches `NVCLAMP(x)` reducing to `(x)` in this configuration), `NIS_USE_HALF_PRECISION=0`, and the always-constant `NIS_SCALE_INT=1`/`NIS_SCALE_FLOAT=1.0` (upstream never redefines these). This is what the design spec's "hand-ported ... targeting `#version 430`" already implies; this section just makes the resolved values explicit so Task 2 doesn't have to re-derive them.

## File/module layout

- `nis_config.h` (new, Task 1) — vendored `NISConfig`/`NVScalerUpdateConfig`/`NVSharpenUpdateConfig`/`coef_scale`/`coef_usm` from `NIS_Config.h`, effectively unmodified.
- `nis_effect.h/.cpp` (new, Task 2) — the two embedded GLSL shader string literals, GL resource management (textures, coefficient textures, UBO, FBO, two compiled programs), and `ApplyNVScaler()`/`ApplyNVSharpen()` entry points.
- `post_effects.cpp` (modify, Task 3) — dispatch `EffectKind::NVScaler`/`NVSharpen` to the new entry points instead of the current "not implemented" log-and-fallback.
- `nis_effect_test.cpp` (new, Task 4) — real-GL-context test exercising both entry points, mirroring `bilinear_upscale_test.cpp`.
- `CMakeLists.txt` (modify, Task 1 and Task 4) — add `nis_effect.cpp` to `${WRAPPER_NAME_CXX}`'s sources; add the `nis_effect_test` executable target.

---

### Task 1: Vendor `nis_config.h` and wire it into the build

**Files:**
- Create: `I:\anax_enhancer\nis_config.h`
- Modify: `I:\anax_enhancer\CMakeLists.txt` (only the `add_library(${WRAPPER_NAME_CXX} ...)` sources line — this task doesn't add a new `.cpp`, `nis_config.h` is header-only, but Task 2 will need it added to that line's dependents; nothing to change here yet if the header alone doesn't require a new source file entry — verify by grepping the line before deciding, see Step 1)

**Interfaces:**
- Produces: `NISConfig` struct, `NISHDRMode`/`NISGPUArchitecture` enums, `NVScalerUpdateConfig(...)`, `NVSharpenUpdateConfig(...)`, and the file-scoped `coef_scale`/`coef_usm`/`coef_scale_fp16`/`coef_usm_fp16` tables (all inside an anonymous namespace, exactly as upstream) — Task 2 consumes `NISConfig`, `NISHDRMode::None`, `NVScalerUpdateConfig`, `NVSharpenUpdateConfig`, `coef_scale`, `coef_usm`. The `_fp16` tables are vendored for fidelity but never referenced (this pipeline doesn't use half precision) — that is expected, not a defect.

- [ ] **Step 1: Read the CMakeLists.txt sources line**

Run: read `I:\anax_enhancer\CMakeLists.txt` around the line `add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp)`.

`nis_config.h` is header-only (no `.cpp`), so it needs no entry on this line by itself — confirm this and make no CMake edit in this task. (Task 2 adds `nis_effect.cpp`, which `#include`s this header, to that line.)

- [ ] **Step 2: Create `nis_config.h`**

Copy `E:\Gits\NVIDIAImageScaling\NIS\NIS_Config.h` verbatim into `I:\anax_enhancer\nis_config.h`, with exactly these two changes:
1. Replace the `#pragma once` block's following comment
   ```
   //---------------------------------------------------------------------------------
   // NVIDIA Image Scaling SDK  - v1.0.3
   //---------------------------------------------------------------------------------
   // Configuration
   //---------------------------------------------------------------------------------
   ```
   with
   ```
   //---------------------------------------------------------------------------------
   // NVIDIA Image Scaling SDK  - v1.0.3
   // Vendored unmodified (except this comment) from
   // E:\Gits\NVIDIAImageScaling\NIS\NIS_Config.h for nis_effect.cpp's NVScaler/NVSharpen
   // pipeline - see docs/superpowers/specs/2026-09-15-nis-post-effects-design.md.
   //---------------------------------------------------------------------------------
   // Configuration
   //---------------------------------------------------------------------------------
   ```
2. Nothing else changes — same struct layout, same functions, same coefficient tables, byte-for-byte identical otherwise (needed so `NISConfig`'s memory layout matches what `glBufferData` uploads in Task 2 with zero reinterpretation risk).

- [ ] **Step 3: Verify it's a valid, self-contained header**

Run (PowerShell, from `I:\anax_enhancer`):
```
type nul > scratch_nis_config_check.cpp
echo #include "nis_config.h" >> scratch_nis_config_check.cpp
echo int main() { NISConfig c{}; return static_cast<int>(sizeof(c)); } >> scratch_nis_config_check.cpp
cl /std:c++17 /c scratch_nis_config_check.cpp /Fo:scratch_nis_config_check.obj
del scratch_nis_config_check.cpp scratch_nis_config_check.obj
```
If `cl` isn't on PATH in the shell, use the CMake x86 build instead: temporarily nothing needs building yet since no `.cpp` includes it — skip straight to Step 4 if a standalone compile isn't convenient; Task 2's build is the real gate for this header. Do not spend more than one attempt on a standalone compile check — it's a nice-to-have, not the acceptance gate.

Expected: no errors (a scan for obvious typos from the copy is the real goal here).

- [ ] **Step 4: Commit**

```
git add nis_config.h
git commit -m "Vendor NIS_Config.h as nis_config.h for the NVScaler/NVSharpen pipeline"
```

---

### Task 2: Port the NIS GLSL shaders and build the NVScaler/NVSharpen GL pipeline

**Files:**
- Create: `I:\anax_enhancer\nis_effect.h`
- Create: `I:\anax_enhancer\nis_effect.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt` (add `nis_effect.cpp` to `${WRAPPER_NAME_CXX}`'s sources)

**Interfaces:**
- Consumes: `GlComputeApi`/`GetGlComputeApi()` from `gl_loader.h` (Plan A/B — every entry point needed here, listed in Step 3 below, already exists); `NISConfig`/`NVScalerUpdateConfig`/`NVSharpenUpdateConfig`/`coef_scale`/`coef_usm`/`NISHDRMode::None` from `nis_config.h` (Task 1).
- Produces: `void ApplyNVScaler(float sharpness);` and `void ApplyNVSharpen(float sharpness);` in `nis_effect.h` — Task 3 calls these, passing `GetAnaxConfig().sharpness`.

- [ ] **Step 1: Write `nis_effect.h`**

```cpp
#pragma once

// NVScaler (edge-adaptive resample + sharpen) and NVSharpen (sharpen-only) post-process
// effects, ported from NVIDIA's Image Scaling SDK (NIS) compute shaders - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's "GPU pipeline" and
// "Shader source and NIS reuse" sections, and this plan's Global Constraints for the exact
// deviations from the upstream Vulkan GLSL. Same capture/compute/blit structure as
// bilinear_upscale.cpp's ApplyBilinearUpscale().

// Runs NVScaler (NIS_SCALER=1: directional edge-adaptive resample + adaptive sharpen) on
// the current back buffer, called from wglSwapBuffers via post_effects.cpp. sharpness is
// GetAnaxConfig().sharpness, 0..1 (see NVScalerUpdateConfig in nis_config.h for how it maps
// to the algorithm's internal strength/limit parameters). Safe to call every frame; a no-op
// (falls back silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyNVScaler(float sharpness);

// Runs NVSharpen (NIS_SCALER=0: adaptive directional sharpen only, no resample) on the
// current back buffer. Same calling convention and fallback behavior as ApplyNVScaler.
void ApplyNVSharpen(float sharpness);
```

- [ ] **Step 2: Write the two ported GLSL compute shader string literals in `nis_effect.cpp`**

These are complete, ready-to-use ports of `E:\Gits\NVIDIAImageScaling\NIS\NIS_Main.glsl` + `NIS\NIS_Scaler.h`, with the Global Constraints' deviations already applied (combined `sampler2D`, native GLSL types, dead branches for this project's fixed configuration stripped). Start `nis_effect.cpp` with:

```cpp
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

}  // namespace
```

- [ ] **Step 3: Write the GL resource/dispatch code in `nis_effect.cpp` (below the shader strings, still inside the anonymous namespace unless noted)**

GL entry points used, all already present in `gl_loader.h`'s `GlComputeApi` (confirm each exists before writing code that calls it — this is the Global Constraints check): `glCreateShader`, `glShaderSource`, `glCompileShader`, `glGetShaderiv`, `glGetShaderInfoLog`, `glDeleteShader`, `glCreateProgram`, `glAttachShader`, `glLinkProgram`, `glGetProgramiv`, `glGetProgramInfoLog`, `glDeleteProgram`, `glUseProgram`, `glGenFramebuffers`, `glDeleteFramebuffers`, `glBindFramebuffer`, `glFramebufferTexture2D`, `glBlitFramebuffer`, `glGenBuffers`, `glDeleteBuffers`, `glBindBuffer`, `glBindBufferBase`, `glBufferData`, `glActiveTexture`, `glTexStorage2D`, `glGenTextures`, `glDeleteTextures`, `glBindTexture`, `glTexParameteri`, `glCopyTexSubImage2D`, `glReadBuffer`, `glGetIntegerv`, `glGetError`, `glBindImageTexture`, `glDispatchCompute`, `glMemoryBarrier`. One entry point this task needs that is **not** yet in `GlComputeApi`: `glTexImage2D` (needed to upload the `coef_scale`/`coef_usm` float data — `glTexStorage2D` alone doesn't upload data, and these coefficient textures need `GL_RGBA32F` texel data uploaded once at creation, which `glTexSubImage2D` + a prior `glTexStorage2D(..., GL_RGBA32F, 2, 64)` handles without needing plain `glTexImage2D`). Use `glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 64)` then `glTexSubImage2D(...)` to upload — so `glTexSubImage2D` is the one genuinely new entry point needed.

**This is a plan defect the implementer must fix, not work around:** `glTexSubImage2D` (and its type `PFNGLTEXSUBIMAGE2DPROC`) is missing from `gl_loader.h`/`gl_loader.cpp`. Add it following the exact pattern of the adjacent `glCopyTexSubImage2D` entry (same resolution category — GL 1.1, resolved via plain `GetProcAddress` on the real `opengl32.dll`, not `wglGetProcAddress` — see `gl_loader.h`'s header comment on the two resolution categories):
- In `gl_loader.h`: add `typedef void (__stdcall *PFNGLTEXSUBIMAGE2DPROC)(unsigned int target, int level, int xoffset, int yoffset, int width, int height, unsigned int format, unsigned int type, const void* pixels);` near `PFNGLCOPYTEXSUBIMAGE2DPROC`, and `PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = nullptr;` in the `GlComputeApi` struct near `glCopyTexSubImage2D`.
- In `gl_loader.cpp`: add it to whichever `ResolveLegacy()`-driven list already resolves `glCopyTexSubImage2D`, using the same `ok &= ...` accumulation pattern as its neighbors (find `glCopyTexSubImage2D`'s resolution line and add an identical one for `glTexSubImage2D` right next to it).

Now write, in `nis_effect.cpp`'s anonymous namespace:

```cpp
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

    EnsureResources(gl, state, width, height);
    EnsureStaticResources(gl, state, variant);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
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
```

Notes for the implementer:
- `coef_scale`/`coef_usm` are `constexpr float [64][8]` inside an anonymous namespace in `nis_config.h` (matching upstream) — `CreateCoefTexture(gl, coef_scale)` works because both this file and `nis_config.h` see the same anonymous-namespace symbols once `nis_config.h` is `#include`d (anonymous-namespace symbols are visible within the including translation unit).
- Texture image unit 2 is used for `out_texture`'s image binding (matches the shaders' `layout(rgba8, binding = 2)`); texture units 0/2/3 are used for `in_texture`/`coef_scaler`/`coef_usm` samplers (matches `layout(binding = 1/3/4)` — sampler binding N in GLSL means "texture unit N", independent of the image-unit numbering, so unit 2 being used by both an image binding and about to be reused... **verify this does not alias**: `glBindImageTexture(2, ...)` binds *image unit* 2, and `glActiveTexture(GL_TEXTURE0+3); glBindTexture(...)` binds *texture unit* 3 — these are separate binding namespaces in GL, so no aliasing occurs even though the numbers 2/3 are reused across the two namespaces. Do not "fix" this by renumbering; it is correct as written.

- [ ] **Step 4: Write `ApplyNVScaler`/`ApplyNVSharpen` (outside the anonymous namespace, at file scope, matching the header's declarations)**

```cpp
void ApplyNVScaler(float sharpness) {
    RunNisPipeline(g_scalerState, NisVariant::Scaler, "nvscaler", kNVScalerShaderSource, sharpness);
}

void ApplyNVSharpen(float sharpness) {
    RunNisPipeline(g_sharpenState, NisVariant::Sharpen, "nvsharpen", kNVSharpenShaderSource, sharpness);
}
```

- [ ] **Step 5: Add `nis_effect.cpp` to CMakeLists.txt**

Change:
```
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp)
```
to:
```
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp)
```

- [ ] **Step 6: Build the x86 preset and fix every compile/link error the shader strings or C++ produce**

Run: `cmake --build --preset x86`

Expected: `opengl32_enh32.dll` builds clean. This is where GLSL syntax mistakes from the hand-port surface only as this repo's own C++ compile succeeding — the *shader* text itself only gets checked at runtime by `glCompileShader` (Task 4's test is the real gate for shader correctness; a clean C++ build here only proves the string literals are syntactically valid C++ string content, not valid GLSL).

- [ ] **Step 7: Commit**

```
git add nis_effect.h nis_effect.cpp gl_loader.h gl_loader.cpp CMakeLists.txt
git commit -m "Add NVScaler/NVSharpen GPU pipeline: ported NIS compute shaders + GL resource management"
```

---

### Task 3: Wire NVScaler/NVSharpen into the effect dispatcher

**Files:**
- Modify: `I:\anax_enhancer\post_effects.cpp`

**Interfaces:**
- Consumes: `ApplyNVScaler(float sharpness)`/`ApplyNVSharpen(float sharpness)` from `nis_effect.h` (Task 2); `GetAnaxConfig().sharpness` (existing, `config.h`).

- [ ] **Step 1: Update `post_effects.cpp`**

Add `#include "nis_effect.h"` near the other includes, then replace the `NVScaler`/`NVSharpen` case in `ApplySelectedEffect()`:

```cpp
        case EffectKind::NVScaler:
            ApplyNVScaler(config.sharpness);
            break;
        case EffectKind::NVSharpen:
            ApplyNVSharpen(config.sharpness);
            break;
```

removing the old shared `case EffectKind::NVScaler: case EffectKind::NVSharpen: { ... "not implemented yet" ... }` block entirely (both the block and the now-unused `EffectName()` helper's `NVScaler`/`NVSharpen` cases stay — `EffectName()` is still used for logging elsewhere, e.g. `config.cpp`'s own diagnostics if any; check whether `EffectName()` has any remaining caller after this change, and if it becomes fully dead code, remove the whole function rather than leaving it unused).

- [ ] **Step 2: Build and smoke-check**

Run: `cmake --build --preset x86`

Expected: clean build. Then run the existing `config_test` and `bilinear_upscale_test` executables to confirm this change didn't disturb anything already working:
```
build-x86\config_test.exe
build-x86\bilinear_upscale_test.exe
```
(adjust the path if the x86 preset's binary output directory differs — check `CMakePresets.json`'s `x86` build preset if `build-x86\` isn't where the `.exe`s land)

Expected: both exit 0.

- [ ] **Step 3: Commit**

```
git add post_effects.cpp
git commit -m "Wire ApplyNVScaler/ApplyNVSharpen into the effect dispatcher"
```

---

### Task 4: Real-GPU test for NVScaler and NVSharpen

**Files:**
- Create: `I:\anax_enhancer\nis_effect_test.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt`

**Interfaces:**
- Consumes: `ApplyNVScaler`/`ApplyNVSharpen` (`nis_effect.h`), `GetGlComputeApi()` (`gl_loader.h`) — same window/context-creation boilerplate as `bilinear_upscale_test.cpp`/`gl_loader_test.cpp`.

- [ ] **Step 1: Write `nis_effect_test.cpp`**

```cpp
// Creates a real OpenGL context, clears the back buffer to a known color, and runs both
// ApplyNVScaler and ApplyNVSharpen through it, checking for GL errors and a crash-free run -
// see gl_loader_test.cpp's header comment for why <windows.h> is safe to use here (a
// standalone .exe, not linked into the proxy DLL). Unlike bilinear_upscale_test.cpp, this
// does not assert the output pixel color: NVScaler/NVSharpen's edge-adaptive sharpening
// intentionally changes pixel values near "edges" (including the clear color's boundary with
// whatever undefined memory was in the newly-created window), so a flat clear color is not a
// meaningful correctness oracle for this shader - only "did it run without a GL error" is
// checked here, matching the design spec's Testing section ("asserting no GL errors and no
// crash").
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "nis_effect.h"

namespace {

bool RunOnce(void (*applyFn)(float), const char* name) {
    applyFn(0.5f);
    const GlComputeApi& gl = GetGlComputeApi();
    unsigned int err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: %s left glGetError() = 0x%04X\n", name, err);
        return false;
    }
    printf("PASS: %s ran with no GL error\n", name);
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxNisEffectTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "nis_effect_test", WS_OVERLAPPEDWINDOW,
        0, 0, 128, 128, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        printf("FAIL: ChoosePixelFormat/SetPixelFormat, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || !wglMakeCurrent(hdc, hglrc)) {
        printf("FAIL: wglCreateContext/wglMakeCurrent, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float, float, float, float);
    typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int);
    typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);
    PFNGLCLEARCOLORPROC pGlClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glClearColor");
    PFNGLCLEARPROC pGlClear = (PFNGLCLEARPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glClear");
    PFNGLVIEWPORTPROC pGlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glViewport");
    const unsigned int GL_COLOR_BUFFER_BIT = 0x00004000;
    pGlViewport(0, 0, 128, 128);
    pGlClearColor(0.8f, 0.2f, 0.1f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);

    bool ok = true;
    ok = RunOnce(&ApplyNVScaler, "ApplyNVScaler") && ok;
    ok = RunOnce(&ApplyNVSharpen, "ApplyNVSharpen") && ok;
    // Run each a second time to exercise EnsureResources'/EnsureStaticResources' reuse
    // (not-first-call) path, same as bilinear_upscale_test.cpp does for ApplyBilinearUpscale.
    ok = RunOnce(&ApplyNVScaler, "ApplyNVScaler (second call)") && ok;
    ok = RunOnce(&ApplyNVSharpen, "ApplyNVSharpen (second call)") && ok;

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Add the CMake target**

Add after the existing `bilinear_upscale_test` target:
```
# Creates a real GL context, clears to a known color, and checks both ApplyNVScaler and
# ApplyNVSharpen's GPU pipelines run to completion with no GL error - see nis_effect_test.cpp.
add_executable(nis_effect_test nis_effect_test.cpp nis_effect.cpp gl_loader.cpp)
target_link_libraries(nis_effect_test opengl32 gdi32 user32)
```

- [ ] **Step 3: Build and run**

```
cmake --build --preset x86
build-x86\nis_effect_test.exe
```
(adjust the binary path the same way as Task 3 Step 2 if needed)

Expected: exit code 0, four `PASS:` lines printed, no `glGetError()` failures. If a shader fails to compile, `glCompileShader`'s info log (printed via `CompileAndLink`'s existing `printf`) is the primary debugging tool — cross-check the failing line against `E:\Gits\NVIDIAImageScaling\NIS\NIS_Scaler.h`'s corresponding original line if the error message alone isn't enough to fix it.

- [ ] **Step 4: Commit**

```
git add nis_effect_test.cpp CMakeLists.txt
git commit -m "Add real-GPU test for ApplyNVScaler/ApplyNVSharpen"
```
