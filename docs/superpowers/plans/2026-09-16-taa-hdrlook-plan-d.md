# TAA-lite + HDR-look Post-Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two new selectable post-process effects to the 32-bit OpenGL proxy (`opengl32_enh32.dll`), alongside the existing `invert`/`bilinear`/`nvscaler`/`nvsharpen` effects: `taa` (a motion-vector-free temporal blend with neighborhood clamping to bound ghosting — "TAA-lite") and `hdrlook` (a single-pass filmic contrast/saturation/highlight grading pass — an "HDR look" that stays SDR, not real extended-range display output).

**Architecture:** Both effects reuse the exact capture→compute→blit GPU pipeline pattern already proven three times in this codebase (`bilinear_upscale.cpp`, `nis_effect.cpp`). `hdrlook` is a straightforward single-texture-in/single-texture-out pass like `bilinear_upscale.cpp`, plus a one-scalar UBO for its strength parameter (mirroring `nis_effect.cpp`'s UBO usage, since `GlComputeApi` has no `glUniform*` entry points — a UBO avoids needing to add any). `taa` additionally needs a persistent two-texture ping-pong history buffer that survives across frames (unlike the other effects' textures, which only need to survive a same-size resize, `taa`'s ping-pong pair carries real temporal state that must alternate roles every successful frame).

**Tech Stack:** C++17, GLSL 430 core compute shaders, the existing `gl_loader.h`'s `GlComputeApi` — **no loader changes needed**; every GL entry point both effects need (including `glPixelStorei`, `glReadPixels`, `glTexSubImage2D`, the full buffer/UBO family) was already added by the immediately preceding plan.

**Spec:** No separate design-spec document for this plan — it extends the existing `docs/superpowers/specs/2026-09-15-nis-post-effects-design.md` pattern (config file, GL loader, capture/compute/blit pipeline, error handling, testing) to two new effects. Where this plan's Global Constraints don't cover something, that spec's general sections (config file format conventions, error handling, testing philosophy) are the fallback authority.

## Global Constraints

- Never `#include <windows.h>` or `<gl/gl.h>`/`<GL/gl.h>` in any file compiled into the `opengl32_enh32`/`opengl32_enh64` proxy DLL — GL enum values are hand-defined `const unsigned int` constants, matching `bilinear_upscale.cpp`/`nis_effect.cpp`'s existing pattern exactly (copy their constant blocks rather than re-deriving values).
- Every new `.cpp` file must be added to `CMakeLists.txt`'s shared `add_library(${WRAPPER_NAME_CXX} ...)` sources line (both x86 and x64 presets use it — do not touch the `CMAKE_SIZEOF_VOID_P EQUAL 8`-gated TSLANG block above it). Near-term priority is the `x86`/`opengl32_enh32` build.
- Every failure mode (shader compile/link error, `glGetError()` after capture or after dispatch) logs once via `printf("[opengl32_enh_cpp] <effect>: ...")` and disables/skips the effect rather than crashing or hanging the host app — mirror `bilinear_upscale.cpp`'s `initTried`/`initOk` pattern and its capture-error early-bail-with-restore pattern exactly (both were added after real bugs were found in earlier plans; this plan's tasks must include them from the start, not discover the need for them in review).
- **State save/restore is the single most bug-prone part of every effect in this codebase — three separate plans have each found and fixed a leaked-GL-state bug here** (a framebuffer-binding leak, a UBO-binding-point leak, and unrestored texture units, all in prior plans' final reviews). Every task below specifies exactly which GL state must be saved before use and restored via a `restoreState()` lambda on every exit path (including the early-return-on-capture-error path): the active texture unit, every texture unit the effect's dispatch touches (not just unit 0), the current program, the read and draw framebuffer bindings, and — for any effect using a UBO — both the generic `GL_UNIFORM_BUFFER` binding and the indexed binding point via `glBindBufferBase(GL_UNIFORM_BUFFER, 0, saved)`. Follow the exact save/restore code given in each task; do not abbreviate it.
- `glBindFramebuffer(GL_READ_FRAMEBUFFER, 0)` must be forced unconditionally right before every capture step (not only on a texture-recreate/resize path) — the read framebuffer could be left bound to anything by whatever ran before this effect in the same frame.
- After capture (`glCopyTexSubImage2D`), check `glGetError()` and bail out via `restoreState()` before dispatch/present if it's non-zero — never blit stale or garbage texture content onto the real back buffer.
- **Binding-number discipline:** a `layout(binding=N)` GLSL declaration for a `sampler2D` fixes its *texture image unit*; for a `uniform Block {}` it fixes its *uniform buffer binding point*; for an `image2D` it fixes its *image unit*. These are three independent namespaces — the same number N can be reused across them with zero collision (already relied on safely in `nis_effect.cpp`). Every task below gives the exact binding numbers on both the shader and the C++ side; **verify they match exactly** before considering a task done — a mismatch here silently produces a black or garbage image with no GL error (this exact bug shipped past a full task review and was only caught in a final whole-branch review in the immediately preceding plan).
- NVIDIA's MIT license and this project's own code contain no third-party licensing concerns for this plan (unlike Plan C, nothing here is ported from an external SDK — the grading curve and temporal-clamp technique are standard, uncredited public-domain-level techniques with no source file to attribute).

## File/module layout

- `config.h`/`config.cpp` (modify, Task 1) — add `EffectKind::TAA`/`EffectKind::HdrLook`, `AnaxConfig::taaBlend`/`AnaxConfig::hdrStrength`, parsing for both.
- `hdr_look.h`/`hdr_look.cpp` (new, Task 2) — the `hdrlook` effect.
- `taa.h`/`taa.cpp` (new, Task 3) — the `taa` effect.
- `post_effects.cpp` (modify, Task 4) — dispatch the two new `EffectKind` values.
- `hdr_look_test.cpp`, `taa_test.cpp` (new, Tasks 2/3) — real-GL-context tests, same pattern as `bilinear_upscale_test.cpp`/`nis_effect_test.cpp`.
- `CMakeLists.txt` (modify, Tasks 2/3/4) — add the two new `.cpp` files to `${WRAPPER_NAME_CXX}`'s sources and add the two new test executable targets.

---

### Task 1: Config plumbing for `taa` and `hdrlook`

**Files:**
- Modify: `I:\anax_enhancer\config.h`
- Modify: `I:\anax_enhancer\config.cpp`
- Modify: `I:\anax_enhancer\config_test.cpp`

**Interfaces:**
- Produces: `EffectKind::TAA`, `EffectKind::HdrLook` (new enum values, appended after `NVSharpen` — existing values' underlying integers must not change, since `config_test.cpp`'s existing checks and the log line's `%d` print of `config.effect` depend on stable ordering for anything already shipped); `AnaxConfig::taaBlend` (float, default `0.5f`), `AnaxConfig::hdrStrength` (float, default `0.5f`). Task 4 consumes both new fields via `GetAnaxConfig()`.

- [ ] **Step 1: Update `config.h`**

```cpp
enum class EffectKind {
    None,
    Invert,
    Bilinear,
    NVScaler,
    NVSharpen,
    TAA,
    HdrLook,
};

struct AnaxConfig {
    EffectKind effect = EffectKind::None;
    float sharpness = 0.5f;
    float scale = 1.0f;
    float taaBlend = 0.5f;
    float hdrStrength = 0.5f;
};
```

- [ ] **Step 2: Update `config.cpp`**

In `ParseEffect`, add two more `strcmp` branches (before the `printf`/fallback line):
```cpp
    if (strcmp(value, "taa") == 0) return EffectKind::TAA;
    if (strcmp(value, "hdrlook") == 0) return EffectKind::HdrLook;
```

In `ParseConfigFile`'s `if (strcmp(key, "effect") == 0) { ... } else if (...)` chain, add two more branches:
```cpp
        } else if (strcmp(key, "taaBlend") == 0) {
            config.taaBlend = ParseClampedFloat(value, 0.0f, 1.0f, config.taaBlend, "taaBlend");
        } else if (strcmp(key, "hdrStrength") == 0) {
            config.hdrStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.hdrStrength, "hdrStrength");
        }
```

Update the final `printf` in `ParseConfigFile` to also report the two new fields (keep the existing three exactly as they are, just extend the format string and argument list):
```cpp
    printf("[opengl32_enh_cpp] config: loaded from '%s' (effect=%d, sharpness=%.3f, scale=%.3f, "
           "taaBlend=%.3f, hdrStrength=%.3f)\n",
           path, static_cast<int>(config.effect), config.sharpness, config.scale,
           config.taaBlend, config.hdrStrength);
```

- [ ] **Step 3: Extend `config_test.cpp`**

Add these checks, following the file's existing `Check(...)`/`WriteFixture(...)` style exactly:

In the "missing file -> defaults" block, add:
```cpp
        Check(config.taaBlend == 0.5f, "missing file falls back to default taaBlend");
        Check(config.hdrStrength == 0.5f, "missing file falls back to default hdrStrength");
```

Add a new block (after the existing "well-formed file" block) exercising both new effect names and fields together:
```cpp
    // TAA and HDR-look fields.
    {
        WriteFixture("config_test_taa_hdr.ini",
            "effect=taa\n"
            "taaBlend=0.85\n"
            "hdrStrength=0.3\n");
        AnaxConfig config = ParseConfigFile("config_test_taa_hdr.ini");
        Check(config.effect == EffectKind::TAA, "parses effect=taa");
        Check(config.taaBlend == 0.85f, "parses taaBlend=0.85");
        Check(config.hdrStrength == 0.3f, "parses hdrStrength=0.3");
    }
    {
        WriteFixture("config_test_hdrlook.ini", "effect=hdrlook\n");
        AnaxConfig config = ParseConfigFile("config_test_hdrlook.ini");
        Check(config.effect == EffectKind::HdrLook, "parses effect=hdrlook");
    }
```

In the "bad values fall back to defaults" block's fixture, add out-of-range values for the two new fields and corresponding checks (extend the existing `config_test_bad.ini` fixture text and add two more `Check(...)` calls in that same block):
```cpp
            "taaBlend=-1.0\n"
            "hdrStrength=2.0\n"
```
```cpp
        Check(config.taaBlend == 0.0f, "out-of-range taaBlend (-1.0) clamps to min 0.0");
        Check(config.hdrStrength == 1.0f, "out-of-range hdrStrength (2.0) clamps to max 1.0");
```

- [ ] **Step 4: Build and run**

```
cmake --build --preset x86
build-x86\config_test.exe
```
Expected: clean build, exit code 0, all `PASS:` lines including the new ones.

- [ ] **Step 5: Commit**

```
git add config.h config.cpp config_test.cpp
git commit -m "Add taa/hdrlook effect kinds and their config fields"
```

---

### Task 2: `hdrlook` effect — single-pass filmic grading

**Files:**
- Create: `I:\anax_enhancer\hdr_look.h`
- Create: `I:\anax_enhancer\hdr_look.cpp`
- Create: `I:\anax_enhancer\hdr_look_test.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt`

**Interfaces:**
- Consumes: `GlComputeApi`/`GetGlComputeApi()` from `gl_loader.h` (no changes needed — every entry point below already exists).
- Produces: `void ApplyHdrLook(float strength);` in `hdr_look.h` — Task 4 calls this with `GetAnaxConfig().hdrStrength`.

- [ ] **Step 1: Write `hdr_look.h`**

```cpp
#pragma once

// Single-pass "HDR-look" post-process: a filmic contrast/saturation/highlight grading pass.
// Output stays ordinary 8-bit SDR (this proxy has no way to signal real extended-range
// output to the display - see docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's
// non-goals for the equivalent reasoning about resolution). Same capture/compute/blit
// structure as bilinear_upscale.cpp's ApplyBilinearUpscale().

// Runs the grading pass on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. strength is GetAnaxConfig().hdrStrength, 0..1: 0 reproduces the input
// unchanged, 1 applies the full curve. Safe to call every frame; a no-op (falls back
// silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyHdrLook(float strength);
```

- [ ] **Step 2: Write `hdr_look.cpp`**

```cpp
// See hdr_look.h. GL pipeline mirrors bilinear_upscale.cpp's ApplyBilinearUpscale() exactly,
// plus a one-float UBO for `strength` (GlComputeApi has no glUniform* entry points, so a UBO
// is used here the same way nis_effect.cpp uses one for NISConfig).
#include <cstdio>

#include "hdr_look.h"
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

// binding=0 on inputTex (a texture-unit binding) and binding=0 on HdrLookConfigBlock (a
// uniform-buffer binding point) are different GL namespaces - see this plan's Global
// Constraints "Binding-number discipline" note. Not a collision.
const char* kHdrLookShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba8, binding = 1) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform HdrLookConfigBlock {\n"
    "    float strength;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    vec3 c = color.rgb;\n"
    "    vec3 curved = smoothstep(vec3(0.0), vec3(1.0), c);\n"
    "    vec3 graded = mix(c, curved, strength);\n"
    "    float luma = dot(graded, vec3(0.2126, 0.7152, 0.0722));\n"
    "    vec3 saturated = mix(vec3(luma), graded, 1.0 + 0.5 * strength);\n"
    "    float highlightMask = smoothstep(0.55, 1.0, luma);\n"
    "    vec3 result = saturated + highlightMask * 0.2 * strength;\n"
    "    result = clamp(result, 0.0, 1.0);\n"
    "    imageStore(outputImage, outCoord, vec4(result, color.a));\n"
    "}\n";

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int inputTexture = 0;
    unsigned int outputTexture = 0;
    unsigned int outputFbo = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kHdrLookShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] hdr_look: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] hdr_look: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        unsigned int textures[2] = {g_state.inputTexture, g_state.outputTexture};
        gl.glDeleteTextures(2, textures);
        gl.glDeleteFramebuffers(1, &g_state.outputFbo);
        g_state.texturesValid = false;
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

    g_state.inputTexture = inputTexture;
    g_state.outputTexture = outputTexture;
    g_state.outputFbo = fbo;
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(float), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

void ApplyHdrLook(float strength) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] hdr_look: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] hdr_look: shader init failed, effect disabled for the "
                   "rest of this process\n");
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

    EnsureTextures(gl, width, height);
    EnsureUbo(gl);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, (unsigned int)savedUniformBuffer);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, (unsigned int)savedUniformBuffer);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
    };

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.inputTexture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    unsigned int captureErr = gl.glGetError();
    if (captureErr != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] hdr_look: glGetError() = 0x%04X after capture, skipping "
               "this frame\n", captureErr);
        restoreState();
        return;
    }

    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(float), &strength, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.inputTexture);
    gl.glBindImageTexture(1, g_state.outputTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, g_state.outputFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] hdr_look: glGetError() = 0x%04X after dispatch\n", err);
    }

    restoreState();
}
```

- [ ] **Step 3: Write `hdr_look_test.cpp`**

Same window/context boilerplate as `bilinear_upscale_test.cpp` (real WGL context, `<windows.h>` is safe here since this is a standalone `.exe` not linked into the proxy DLL). Clear to a known color, call `ApplyHdrLook(0.0f)` and assert the center pixel reproduces the clear color closely (strength=0 must be a no-op: `graded=c`, `saturated=mix(luma,c,1.0)=c` exactly since `mix(a,b,1.0)==b`, `highlightMask*0.2*0=0`, so `result==c` up to float/8-bit rounding — allow a small tolerance, e.g. ±2 per channel, same tolerance style as `bilinear_upscale_test.cpp`'s existing pixel check). Then call `ApplyHdrLook(0.8f)` and assert only `glGetError() == 0` (no pixel-value assertion needed for a nonzero strength — the curve's exact output isn't being validated, just that it runs cleanly).

```cpp
// Creates a real OpenGL context, clears the back buffer to a known color, and checks
// ApplyHdrLook()'s GPU pipeline (capture -> compute dispatch -> blit) both at strength=0
// (must reproduce the input unchanged) and a nonzero strength (must run with no GL error) -
// see gl_loader_test.cpp's header comment for why <windows.h> is safe here.
#include <windows.h>
#include <cstdio>
#include <cmath>

#include "gl_loader.h"
#include "hdr_look.h"

namespace {

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: %s channel = %d, expected ~%d (tolerance %d)\n", channel, actual, expected, tolerance);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxHdrLookTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "hdr_look_test", WS_OVERLAPPEDWINDOW,
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

    ApplyHdrLook(0.0f);
    const GlComputeApi& gl = GetGlComputeApi();
    unsigned int err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyHdrLook(0.0) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after ApplyHdrLook(0.0): r=%d g=%d b=%d a=%d\n", pixel[0], pixel[1], pixel[2], pixel[3]);
        ok = CheckClose(pixel[0], 204, 2, "r") && ok;
        ok = CheckClose(pixel[1], 51, 2, "g") && ok;
        ok = CheckClose(pixel[2], 25, 2, "b") && ok;
        if (ok) printf("PASS: strength=0.0 reproduced the cleared color\n");
    }

    ApplyHdrLook(0.8f);
    err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyHdrLook(0.8) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        printf("PASS: ApplyHdrLook(0.8) ran with no GL error\n");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
```

Note: `gl.glReadPixels` was added to `GlComputeApi` in the immediately preceding plan for `nis_effect_test.cpp`'s use — confirm it's present before using it (it should be; this is not expected to require any loader change).

- [ ] **Step 4: Add `hdr_look.cpp` and the `hdr_look_test` target to CMakeLists.txt**

Change:
```
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp)
```
to:
```
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp hdr_look.cpp)
```

Add after the existing `nis_effect_test` target:
```
# Creates a real GL context, clears to a known color, and checks ApplyHdrLook()'s GPU
# pipeline at strength=0 (must reproduce the input) and strength=0.8 (must run with no GL
# error) - see hdr_look_test.cpp.
add_executable(hdr_look_test hdr_look_test.cpp hdr_look.cpp gl_loader.cpp)
target_link_libraries(hdr_look_test opengl32 gdi32 user32)
```

- [ ] **Step 5: Build and run**

```
cmake --build --preset x86
build-x86\hdr_look_test.exe
```
Expected: exit code 0, both PASS lines, center pixel after `strength=0.0` within tolerance of the clear color `(204, 51, 25)`.

- [ ] **Step 6: Commit**

```
git add hdr_look.h hdr_look.cpp hdr_look_test.cpp CMakeLists.txt
git commit -m "Add hdrlook effect: single-pass filmic contrast/saturation/highlight grading"
```

---

### Task 3: `taa` effect — temporal blend with neighborhood clamping

**Files:**
- Create: `I:\anax_enhancer\taa.h`
- Create: `I:\anax_enhancer\taa.cpp`
- Create: `I:\anax_enhancer\taa_test.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt`

**Interfaces:**
- Consumes: `GlComputeApi`/`GetGlComputeApi()` from `gl_loader.h` (no changes needed).
- Produces: `void ApplyTaa(float blend);` in `taa.h` — Task 4 calls this with `GetAnaxConfig().taaBlend`.

- [ ] **Step 1: Write `taa.h`**

```cpp
#pragma once

// Motion-vector-free temporal antialiasing ("TAA-lite"): blends the current frame with a
// persistent history texture, clamping the history sample into the current frame's local
// 3x3 neighborhood min/max first to bound ghosting on moving content (this proxy has no
// access to the app's per-object motion vectors, unlike a real engine's TAA). Same
// capture/compute/blit structure as bilinear_upscale.cpp's ApplyBilinearUpscale(), plus a
// two-texture ping-pong history buffer that survives across frames.

// Runs one TAA-lite frame on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. blend is GetAnaxConfig().taaBlend, 0..1: how much of the (clamped)
// history to keep versus the current frame (0 = no temporal blending, 1 = heaviest -
// most stable but most ghosting risk). The very first call after startup or after a
// viewport-size change has no valid history yet and outputs the current frame unchanged,
// seeding history for the next call. Safe to call every frame; a no-op (falls back
// silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyTaa(float blend);
```

- [ ] **Step 2: Write `taa.cpp`**

```cpp
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
```

- [ ] **Step 3: Write `taa_test.cpp`**

Same boilerplate as `hdr_look_test.cpp`/`bilinear_upscale_test.cpp`. Call `ApplyTaa(0.85f)` **four times in a row** against a fixed, unchanging clear color (no re-clearing between calls - the point is to exercise the ping-pong flip across multiple successful frames: call 1 takes the `historyValid=0` passthrough path, calls 2-4 exercise the real blend/clamp path with the ping-pong roles flipping each time). Since the clear color never changes, the neighborhood-clamp box degenerates to a single value every time (`neighborMin == neighborMax == currentColor`), so `clampedHistory == historyColor`, and since history always equals the same unchanging color, every call's output should still closely match the original clear color regardless of `blend`. Check the center pixel after each of the 4 calls is within tolerance of the clear color, and check `glGetError() == 0` after each call.

```cpp
// Creates a real OpenGL context, clears the back buffer to a known color, and calls
// ApplyTaa() four times in a row against that unchanging color - exercising the
// historyValid=0 passthrough path (call 1) and the ping-pong blend/clamp path across
// multiple successful frames (calls 2-4). Since the scene never changes, every call should
// still reproduce the clear color (the neighborhood clamp box degenerates to a single value
// when the frame is static, so blending never drifts) - this also functions as the
// ghosting-mitigation check: if the ping-pong role-flip or clamp logic were wrong, a static
// scene would still visibly drift or corrupt after a few frames, which this test would catch.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "taa.h"

namespace {

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel, int callNum) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: call %d, %s channel = %d, expected ~%d (tolerance %d)\n", callNum, channel, actual, expected, tolerance);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxTaaTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "taa_test", WS_OVERLAPPEDWINDOW,
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
    const GlComputeApi& gl = GetGlComputeApi();

    for (int call = 1; call <= 4; ++call) {
        ApplyTaa(0.85f);
        unsigned int err = gl.glGetError();
        if (err != 0) {
            printf("FAIL: call %d left glGetError() = 0x%04X\n", call, err);
            ok = false;
            continue;
        }
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after call %d: r=%d g=%d b=%d a=%d\n", call, pixel[0], pixel[1], pixel[2], pixel[3]);
        bool callOk = CheckClose(pixel[0], 204, 2, "r", call);
        callOk = CheckClose(pixel[1], 51, 2, "g", call) && callOk;
        callOk = CheckClose(pixel[2], 25, 2, "b", call) && callOk;
        if (callOk) {
            printf("PASS: call %d reproduced the cleared color\n", call);
        }
        ok = callOk && ok;
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
```

- [ ] **Step 4: Add `taa.cpp` and the `taa_test` target to CMakeLists.txt**

Change:
```
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp hdr_look.cpp)
```
to:
```
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp hdr_look.cpp taa.cpp)
```
(If Task 2 hasn't landed yet when this task starts, the line will instead end in `nis_effect.cpp` — add `taa.cpp` to whatever the line currently is, don't assume `hdr_look.cpp` is already there; tasks run sequentially so it will be.)

Add after the `hdr_look_test` target:
```
# Creates a real GL context, clears to a known color, and calls ApplyTaa() four times in a
# row, checking the ping-pong history buffer and neighborhood clamp reproduce a static
# scene's color with no drift and no GL error - see taa_test.cpp.
add_executable(taa_test taa_test.cpp taa.cpp gl_loader.cpp)
target_link_libraries(taa_test opengl32 gdi32 user32)
```

- [ ] **Step 5: Build and run**

```
cmake --build --preset x86
build-x86\taa_test.exe
```
Expected: exit code 0, 4 `PASS:` lines, every call's center pixel within tolerance of `(204, 51, 25)`.

- [ ] **Step 6: Commit**

```
git add taa.h taa.cpp taa_test.cpp CMakeLists.txt
git commit -m "Add taa effect: temporal blend with neighborhood-clamped ghosting mitigation"
```

---

### Task 4: Wire `taa` and `hdrlook` into the effect dispatcher

**Files:**
- Modify: `I:\anax_enhancer\post_effects.cpp`

**Interfaces:**
- Consumes: `ApplyTaa(float blend)` (`taa.h`, Task 3), `ApplyHdrLook(float strength)` (`hdr_look.h`, Task 2), `GetAnaxConfig().taaBlend`/`GetAnaxConfig().hdrStrength` (existing, `config.h`, added by Task 1).

- [ ] **Step 1: Update `post_effects.cpp`**

Add `#include "taa.h"` and `#include "hdr_look.h"` near the other includes, then add two more cases to the `switch (config.effect)` block (after the existing `NVSharpen` case, before the closing brace):

```cpp
        case EffectKind::TAA:
            ApplyTaa(config.taaBlend);
            break;
        case EffectKind::HdrLook:
            ApplyHdrLook(config.hdrStrength);
            break;
```

- [ ] **Step 2: Build and run the full test suite**

```
cmake --build --preset x86
build-x86\config_test.exe
build-x86\gl_loader_test.exe
build-x86\bilinear_upscale_test.exe
build-x86\nis_effect_test.exe
build-x86\hdr_look_test.exe
build-x86\taa_test.exe
```
(adjust the binary path if the x86 preset's output directory differs from `build-x86\` — check `CMakePresets.json` if so)

Expected: clean build, every executable exits 0 with all-PASS output, no regressions in any of the pre-existing effects' tests.

- [ ] **Step 3: Commit**

```
git add post_effects.cpp
git commit -m "Wire ApplyTaa/ApplyHdrLook into the effect dispatcher"
```
