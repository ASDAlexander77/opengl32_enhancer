# NIS Post-Effects — Plan B: BilinearUpscale GPU Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove out the real GPU pipeline (capture the back buffer into a texture → GLSL compute shader → blit the result back) end to end, using `BilinearUpscale` as the simplest possible effect, and wire it into the `effect=bilinear` case of the dispatcher Plan A already built. `effect=nvscaler`/`nvsharpen` remain unimplemented placeholders (Plan C).

**Architecture:** `gl_loader.h`/`.cpp` (Plan A) already resolves the GL 4.3 compute-shader entry points via `wglGetProcAddress`. This plan extends it with the handful of ordinary GL 1.1 entry points (`glGenTextures`, `glCopyTexSubImage2D`, `glGetIntegerv`, ...) the pipeline also needs — resolved via plain `GetProcAddress` on the real `opengl32.dll`, not `wglGetProcAddress` (the WGL spec only guarantees the latter for functions beyond GL 1.1). A new `bilinear_upscale.h/.cpp` implements the pipeline itself: capture via `glCopyTexSubImage2D` (no CPU round-trip), a tiny embedded GLSL compute shader (`sampler2D` → `image2D`, letting `GL_LINEAR` texture filtering do the actual bilinear work), then `glBlitFramebuffer` back onto `GL_BACK`, with GL state saved and restored around all of it.

**Tech Stack:** Plain C++ (MSVC), GLSL 430 compute shaders, Win32/WGL, CMake/Ninja — same as Plan A.

**Spec:** `docs/superpowers/specs/2026-09-15-nis-post-effects-design.md` (see its "GPU pipeline" section — this plan implements that section for the `bilinear` effect only; NVScaler/NVSharpen and the actual NIS shader port are Plan C)

## Scope note

Per the user's current priority ([[project-32bit-prototype-first]]), this plan targets the `x86` preset only. The `default`/x64 preset has a known pre-existing, unrelated link failure (see Plan A's final review) — this plan's new sources should still compile under it if you happen to check, but do not spend effort fixing or working around that failure; it's out of scope here too.

**One deliberate deviation from the spec's literal wording**, decided during this plan's design and carried into the tasks below: the spec's "GPU pipeline" section says "one FBO per texture used as blit source/destination." In practice only the **output** texture needs an FBO (as the `glBlitFramebuffer` read source) — `glCopyTexSubImage2D` (the capture step) reads from whatever is already bound as the current read framebuffer, which for a live back buffer is the default framebuffer with no FBO required. This plan creates exactly one FBO, not two.

## Global Constraints

- Never `#include <windows.h>` (or anything pulling in `<wingdi.h>`) in any file compiled into the `opengl32_enh32`/`opengl32_enh64` proxy target — it declares `wgl*`/`gl*` names as `dllimport`, colliding with `wrapper.cpp`'s `dllexport` definitions of the same names. `gl_loader.h`/`.cpp`, `bilinear_upscale.h`/`.cpp`, and `post_effects.cpp` are all compiled into that target and are bound by this. Standalone test executables (`gl_loader_test.cpp`, `bilinear_upscale_test.cpp`) are exempt and may use `<windows.h>` freely.
- Debug/trace logging uses the existing convention: `printf("[opengl32_enh_cpp] ...")`.
- Every failure mode (no GL 4.3 context, shader compile/link failure, a `glGetError()` after dispatch) logs once and falls back to doing nothing — never crashes the host app. This mirrors Plan A's `post_effects.cpp` and `pixel_invert.cpp`'s existing conventions.
- Build/verify against the `x86` CMake preset.
- `GetGlComputeApi()`'s caching contract (Plan A, already reviewed): only a successful resolution is cached; a failed attempt is retried on the next call. Code in this plan must call `GetGlComputeApi()` (not `LoadGlComputeApi()` directly) to get this behavior, exactly like the pattern `gl_loader_test.cpp` already exercises.

---

### Task 1: Extend gl_loader with the GL 1.1 entry points the pipeline needs

**Files:**
- Modify: `I:\anax_enhancer\gl_loader.h`
- Modify: `I:\anax_enhancer\gl_loader.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces (used by Task 2): 8 new fields on `GlComputeApi` — `glGenTextures`, `glDeleteTextures`, `glBindTexture`, `glTexParameteri`, `glCopyTexSubImage2D`, `glReadBuffer`, `glGetIntegerv`, `glGetError` — resolved the same way as the existing 29 fields (via `GetGlComputeApi()`/`LoadGlComputeApi()`), just through a different resolution path internally.

- [ ] **Step 1: Add the new typedefs and struct fields to `gl_loader.h`**

Add these typedefs after the existing `PFNGLTEXSTORAGE2DPROC` typedef (the last one in the file):

```cpp
typedef void (__stdcall *PFNGLGENTEXTURESPROC)(int n, unsigned int* textures);
typedef void (__stdcall *PFNGLDELETETEXTURESPROC)(int n, const unsigned int* textures);
typedef void (__stdcall *PFNGLBINDTEXTUREPROC)(unsigned int target, unsigned int texture);
typedef void (__stdcall *PFNGLTEXPARAMETERIPROC)(unsigned int target, unsigned int pname, int param);
typedef void (__stdcall *PFNGLCOPYTEXSUBIMAGE2DPROC)(unsigned int target, int level, int xoffset, int yoffset,
    int x, int y, int width, int height);
typedef void (__stdcall *PFNGLREADBUFFERPROC)(unsigned int mode);
typedef void (__stdcall *PFNGLGETINTEGERVPROC)(unsigned int pname, int* params);
typedef unsigned int (__stdcall *PFNGLGETERRORPROC)(void);
```

Add these fields to `GlComputeApi`, right before the closing `bool loaded = false;` line:

```cpp
    PFNGLGENTEXTURESPROC glGenTextures = nullptr;
    PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
    PFNGLBINDTEXTUREPROC glBindTexture = nullptr;
    PFNGLTEXPARAMETERIPROC glTexParameteri = nullptr;
    PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D = nullptr;
    PFNGLREADBUFFERPROC glReadBuffer = nullptr;
    PFNGLGETINTEGERVPROC glGetIntegerv = nullptr;
    PFNGLGETERRORPROC glGetError = nullptr;
```

Update the file's top comment (currently says "Resolves the GL 4.3+ function pointers...") to reflect that it now also covers a handful of GL 1.1 entry points, resolved differently:

```cpp
// Resolves the extra GL function pointers the post-process compute-shader effects need
// beyond what wrapper.cpp's generated per-function forwarding already covers
// (BilinearUpscale/NVScaler/NVSharpen - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md). Two categories:
//   - GL 4.3+ functions (compute shaders, image load/store, framebuffer blit, buffers):
//     not part of opengl32.dll's static export table, resolved via wglGetProcAddress.
//   - A handful of GL 1.1 functions (glGenTextures, glGetIntegerv, ...): part of the
//     static export table, but NOT reliably resolvable via wglGetProcAddress (the WGL
//     spec only guarantees it for functions beyond GL 1.1) - resolved via plain
//     GetProcAddress on the real opengl32.dll module instead.
// Both categories are resolved independently of wrapper.cpp's own exported forwarding
// (see gl_loader.cpp's header comment for why) and both live in the same GlComputeApi
// struct/loaded flag, since callers need both together and a context-capability check
// that covers only one category would be meaningless.
```

- [ ] **Step 2: Update `gl_loader.cpp` to resolve the new fields via plain `GetProcAddress`, not `wglGetProcAddress`**

Replace the `namespace { ... }` block (everything between `namespace {` and the matching `}  // namespace`) with:

```cpp
namespace {

HMODULE GetRealOpenGL32Module() {
    static HMODULE real = [] {
        HMODULE result = LoadLibraryA("C:\\Windows\\System32\\opengl32.dll");
        if (result == nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: FAILED to load real opengl32.dll, GetLastError=%lu\n", GetLastError());
        }
        return result;
    }();
    return real;
}

PFNWGLGETPROCADDRESSPROC GetRealWglGetProcAddress() {
    static PFNWGLGETPROCADDRESSPROC fn = [] {
        HMODULE real = GetRealOpenGL32Module();
        if (real == nullptr) {
            return static_cast<PFNWGLGETPROCADDRESSPROC>(nullptr);
        }
        auto result = reinterpret_cast<PFNWGLGETPROCADDRESSPROC>(GetProcAddress(real, "wglGetProcAddress"));
        if (result == nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve wglGetProcAddress itself, GetLastError=%lu\n", GetLastError());
        }
        return result;
    }();
    return fn;
}

template <typename T>
bool Resolve(const char* name, T& outFn) {
    PFNWGLGETPROCADDRESSPROC wglGetProcAddress = GetRealWglGetProcAddress();
    outFn = wglGetProcAddress ? reinterpret_cast<T>(wglGetProcAddress(name)) : nullptr;
    if (outFn == nullptr) {
        printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve '%s'\n", name);
        return false;
    }
    return true;
}

// GL 1.1 core entry points are NOT reliably resolvable via wglGetProcAddress - see this
// file's header comment. The real opengl32.dll's ordinary static export table (plain
// GetProcAddress) is the correct way to resolve them.
template <typename T>
bool ResolveLegacy(const char* name, T& outFn) {
    HMODULE real = GetRealOpenGL32Module();
    outFn = real ? reinterpret_cast<T>(GetProcAddress(real, name)) : nullptr;
    if (outFn == nullptr) {
        printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve '%s'\n", name);
        return false;
    }
    return true;
}

}  // namespace
```

(This is a refactor of the existing `GetRealWglGetProcAddress()` — the `LoadLibraryA` call that used to live directly inside it is now the separate, reusable `GetRealOpenGL32Module()`, used by both the existing wgl-extension path and the new legacy path.)

Then, inside `LoadGlComputeApi`, add these 8 lines right after the existing `ok &= Resolve("glTexStorage2D", api.glTexStorage2D);` line (the last one):

```cpp
    ok &= ResolveLegacy("glGenTextures", api.glGenTextures);
    ok &= ResolveLegacy("glDeleteTextures", api.glDeleteTextures);
    ok &= ResolveLegacy("glBindTexture", api.glBindTexture);
    ok &= ResolveLegacy("glTexParameteri", api.glTexParameteri);
    ok &= ResolveLegacy("glCopyTexSubImage2D", api.glCopyTexSubImage2D);
    ok &= ResolveLegacy("glReadBuffer", api.glReadBuffer);
    ok &= ResolveLegacy("glGetIntegerv", api.glGetIntegerv);
    ok &= ResolveLegacy("glGetError", api.glGetError);
```

Update the two log messages right after (currently say "all GL 4.3 compute entry points resolved OK" / "one or more GL 4.3 entry points unavailable ...") to drop the now-inaccurate "4.3" qualifier, e.g.:

```cpp
    if (ok) {
        printf("[opengl32_enh_cpp] gl_loader: all GL entry points resolved OK\n");
    } else {
        printf("[opengl32_enh_cpp] gl_loader: one or more GL entry points unavailable - "
               "compute-shader effects (bilinear/nvscaler/nvsharpen) are disabled on this context\n");
    }
```

Do not otherwise change `LoadGlComputeApi`'s or `GetGlComputeApi()`'s structure — the `&=` non-short-circuiting pattern and the retry-on-failure caching behavior both stay exactly as they are.

- [ ] **Step 3: Build and run the existing `gl_loader_test`, verify it still passes with the larger API surface**

Run:
```bash
cmake --build --preset x86 --target gl_loader_test
./build-x86/gl_loader_test.exe
```
Expected: same output shape as before (`GL_VERSION = ...`, `LoadGlComputeApi -> PASS`, `GetGlComputeApi -> PASS`), exit code 0 — now silently covering the 8 new entry points too, since `gl_loader_test.cpp` already asserts on `LoadGlComputeApi`'s aggregate `ok` result and needs no changes itself. If any of the 8 new entries fail to resolve (unlikely — they're GL 1.1 core, universally available), the existing `FAILED to resolve '<name>'` line will name exactly which one and the test will legitimately report FAIL; treat that as a real environment problem to investigate, not something to work around.

- [ ] **Step 4: Commit**

```bash
git add gl_loader.h gl_loader.cpp
git commit -m "$(cat <<'EOF'
Extend gl_loader with the GL 1.1 entries the pipeline effects need

Adds glGenTextures/glDeleteTextures/glBindTexture/glTexParameteri/
glCopyTexSubImage2D/glReadBuffer/glGetIntegerv/glGetError to
GlComputeApi, resolved via plain GetProcAddress on the real
opengl32.dll rather than wglGetProcAddress - the WGL spec doesn't
guarantee the latter covers GL 1.1 core functions. Not yet consumed -
that's Task 2.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: BilinearUpscale GPU pipeline

**Files:**
- Create: `I:\anax_enhancer\bilinear_upscale.h`
- Create: `I:\anax_enhancer\bilinear_upscale.cpp`
- Create: `I:\anax_enhancer\bilinear_upscale_test.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt` (add `bilinear_upscale.cpp` to the proxy target's sources; add a new `bilinear_upscale_test` executable)

**Interfaces:**
- Consumes: `GlComputeApi`/`GetGlComputeApi()` (Task 1).
- Produces (used by Task 3): `void ApplyBilinearUpscale();`, declared in `bilinear_upscale.h`.

- [ ] **Step 1: Write `bilinear_upscale.h`**

```cpp
#pragma once

// Runs the BilinearUpscale post effect: captures the current back buffer into a texture,
// resamples it via a GLSL compute shader (a GL_LINEAR-filtered sampler2D read into an
// image2D write - "upscale" is really just a resample; at the current default scale of
// 1.0 it's a same-size round trip that still exercises the full pipeline, see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md), then blits the result
// back onto the back buffer. Requires a GL 4.3+ context (see gl_loader.h's
// GetGlComputeApi()) - on anything older, or if shader compilation/linking fails, this
// logs once and silently no-ops on every call, same as every other failure mode in this
// DLL.
void ApplyBilinearUpscale();
```

- [ ] **Step 2: Write `bilinear_upscale.cpp`**

```cpp
// See bilinear_upscale.h. Implements the GPU pipeline described in
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's "GPU pipeline" section:
// capture the back buffer into a texture via glCopyTexSubImage2D (no CPU readback, unlike
// pixel_invert.cpp's InvertBackBufferColors), run a compute shader, blit the result back.
//
// Only one FBO is created (for the output texture, as the glBlitFramebuffer read source)
// rather than one per texture as the spec's wording suggested: glCopyTexSubImage2D reads
// from whatever is bound as the current read framebuffer, which for a live back buffer is
// already the default framebuffer - no FBO of its own is needed for the capture step.
#include <cstdio>

#include "bilinear_upscale.h"
#include "gl_loader.h"

namespace {

// GL constants used here, defined by hand rather than pulling in <gl/gl.h> - see Global
// Constraints in the plan / wrapper.cpp's header comment for why.
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
const unsigned int GL_NO_ERROR                 = 0;

// Samples the input texture with GL_LINEAR filtering (set on the texture in EnsureTextures
// below) at each output texel's center, normalized by the output image's own size -
// GL_LINEAR does the actual bilinear interpolation; this shader just drives it. binding=0
// matches the texture unit ApplyBilinearUpscale() below binds the input texture to;
// binding=1 matches the image unit the output texture is bound to.
const char* kComputeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba8, binding = 1) uniform writeonly image2D outputImage;\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) {\n"
    "        return;\n"
    "    }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    imageStore(outputImage, outCoord, color);\n"
    "}\n";

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;

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
    gl.glShaderSource(shader, 1, &kComputeShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[1024];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] bilinear_upscale: FAILED to compile compute shader: %s\n", log);
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
        char log[1024];
        int logLen = 0;
        gl.glGetProgramInfoLog(program, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] bilinear_upscale: FAILED to link compute program: %s\n", log);
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

    g_state.inputTexture = inputTexture;
    g_state.outputTexture = outputTexture;
    g_state.outputFbo = fbo;
    g_state.width = width;
    g_state.height = height;
    g_state.texturesValid = true;
}

}  // namespace

void ApplyBilinearUpscale() {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] bilinear_upscale: GL 4.3 compute support unavailable "
                   "on this context, effect disabled\n");
            warned = true;
        }
        return;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] bilinear_upscale: shader init failed, effect disabled "
                   "for the rest of this process\n");
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

    EnsureTextures(gl, width, height);

    // Save the GL state this pipeline is about to touch, so the host app's own state comes
    // back untouched afterward.
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

    // Capture: copy the current back buffer straight into the input texture, GPU-to-GPU.
    // Nothing in this function has touched GL_READ_FRAMEBUFFER yet, so the default
    // framebuffer (the live back buffer) is still the read source at this point.
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.inputTexture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    // Dispatch: run the compute shader, sampling the input texture and writing the output
    // texture as an image.
    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.inputTexture);
    gl.glBindImageTexture(1, g_state.outputTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    // Present: blit the output texture back onto the real back buffer.
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, g_state.outputFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] bilinear_upscale: glGetError() = 0x%04X after dispatch\n", err);
    }

    // Restore.
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
    gl.glUseProgram((unsigned int)savedProgram);
    gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
    gl.glActiveTexture((unsigned int)savedActiveTexture);
}
```

- [ ] **Step 3: Write `bilinear_upscale_test.cpp` (real-GPU pipeline test)**

```cpp
// Creates a real OpenGL context, clears the back buffer to a known solid color, runs
// ApplyBilinearUpscale() directly (not through wglSwapBuffers - this exercises the GPU
// pipeline itself, not the dispatcher; Task 3 covers the dispatcher wiring), and reads
// back the result to confirm the pipeline reproduced the color without a GL error. See
// gl_loader_test.cpp for why <windows.h> is safe here and for the window/context creation
// pattern reused below.
#include <windows.h>
#include <cmath>
#include <cstdio>

#include "bilinear_upscale.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_VIEWPORT         = 0x0BA2;
const unsigned int GL_COLOR_BUFFER_BIT = 0x00004000;
const unsigned int GL_RGBA             = 0x1908;
const unsigned int GL_UNSIGNED_BYTE    = 0x1401;

typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float r, float g, float b, float a);
typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int mask);
typedef void (__stdcall *PFNGLREADPIXELSPROC)(int x, int y, int width, int height,
    unsigned int format, unsigned int type, void* pixels);

template <typename T>
T Resolve(const char* name) {
    return (T)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
}

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    return condition;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxBilinearUpscaleTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "bilinear_upscale_test", WS_OVERLAPPEDWINDOW,
        0, 0, 256, 256, nullptr, nullptr, wc.hInstance, nullptr);
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

    bool ok = true;

    PFNGLCLEARCOLORPROC pGlClearColor = Resolve<PFNGLCLEARCOLORPROC>("glClearColor");
    PFNGLCLEARPROC pGlClear = Resolve<PFNGLCLEARPROC>("glClear");
    PFNGLREADPIXELSPROC pGlReadPixels = Resolve<PFNGLREADPIXELSPROC>("glReadPixels");
    ok &= Check(pGlClearColor && pGlClear && pGlReadPixels,
                "resolved glClearColor/glClear/glReadPixels");

    const GlComputeApi& gl = GetGlComputeApi();
    ok &= Check(gl.loaded, "GetGlComputeApi() resolved on this context");

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    ok &= Check(width > 0 && height > 0, "context reports a nonzero viewport");
    printf("Viewport: %dx%d\n", width, height);

    // Clear to a known solid color, run the pipeline twice (the second call also exercises
    // EnsureTextures' "same size, reuse existing textures" path), then read back.
    pGlClearColor(0.8f, 0.2f, 0.1f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);
    ApplyBilinearUpscale();
    unsigned int errAfterFirst = gl.glGetError();
    ok &= Check(errAfterFirst == 0, "no GL error after first ApplyBilinearUpscale() call");

    ApplyBilinearUpscale();
    unsigned int errAfterSecond = gl.glGetError();
    ok &= Check(errAfterSecond == 0,
                "no GL error after second ApplyBilinearUpscale() call (same size, texture reuse)");

    unsigned char pixel[4] = {0, 0, 0, 0};
    pGlReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    printf("Center pixel after bilinear pass: r=%d g=%d b=%d a=%d\n",
           pixel[0], pixel[1], pixel[2], pixel[3]);
    // Expect close to (204, 51, 26) = (0.8, 0.2, 0.1) in 8-bit. Generous tolerance: this
    // isn't testing color precision, just that the pipeline reproduced approximately the
    // right color instead of corrupting or zeroing it.
    bool colorPlausible = std::abs((int)pixel[0] - 204) < 20
                        && std::abs((int)pixel[1] - 51) < 20
                        && std::abs((int)pixel[2] - 26) < 20;
    ok &= Check(colorPlausible, "bilinear pass reproduced the cleared color at scale 1.0");

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("\n%s\n", ok ? "All checks passed." : "Some checks FAILED.");
    return ok ? 0 : 1;
}
```

- [ ] **Step 4: Wire `bilinear_upscale.cpp` and its test into `CMakeLists.txt`**

Modify the existing proxy target sources line:
```cmake
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp)
```
to:
```cmake
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp)
```

Add a new executable, near the existing `gl_loader_test` block:
```cmake
# Creates a real GL context, clears to a known color, and checks ApplyBilinearUpscale()'s
# GPU pipeline (capture -> compute dispatch -> blit) reproduces it without a GL error - see
# bilinear_upscale_test.cpp.
add_executable(bilinear_upscale_test bilinear_upscale_test.cpp bilinear_upscale.cpp gl_loader.cpp)
target_link_libraries(bilinear_upscale_test opengl32 gdi32 user32)
```

- [ ] **Step 5: Build and run `bilinear_upscale_test`, verify it passes**

Run:
```bash
cmake --build --preset x86 --target bilinear_upscale_test
./build-x86/bilinear_upscale_test.exe
```
Expected: every `Check(...)` line prints `PASS:`, the center pixel is close to `r=204 g=51 b=26`, final line `All checks passed.`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add bilinear_upscale.h bilinear_upscale.cpp bilinear_upscale_test.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Add BilinearUpscale GPU pipeline (capture -> compute -> blit)

Implements the real GPU pipeline from the spec's "GPU pipeline"
section: glCopyTexSubImage2D captures the back buffer into a texture
(no CPU readback), a small embedded GLSL compute shader resamples it
via GL_LINEAR-filtered sampling, glBlitFramebuffer presents the result
- all wrapped with GL state save/restore. Verified end-to-end against
real GPU hardware via bilinear_upscale_test.cpp. Not yet wired into
the effect dispatcher - that's Task 3.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Wire into the effect dispatcher and verify end-to-end

**Files:**
- Modify: `I:\anax_enhancer\post_effects.cpp`

**Interfaces:**
- Consumes: `ApplyBilinearUpscale()` (Task 2).
- Produces: nothing new — this is the integration point where `effect=bilinear` becomes real.

- [ ] **Step 1: Split `EffectKind::Bilinear` out of the "not implemented yet" case in `post_effects.cpp`**

Add the include:
```cpp
#include "bilinear_upscale.h"
```
right after the existing `#include "pixel_invert.h"` line.

Replace the `switch` statement's body:
```cpp
    switch (config.effect) {
        case EffectKind::None:
            break;
        case EffectKind::Invert:
            InvertBackBufferColors();
            break;
        case EffectKind::Bilinear:
        case EffectKind::NVScaler:
        case EffectKind::NVSharpen: {
            // GPU pipeline (capture -> compute dispatch -> blit back) lands in a follow-up
            // plan (see the plan's Scope note). Until then, these are recognized and
            // logged, and fall back to doing nothing, same as effect=none.
            static bool warned = false;
            if (!warned) {
                printf("[opengl32_enh_cpp] post_effects: effect='%s' is not implemented yet, "
                       "falling back to none\n", EffectName(config.effect));
                warned = true;
            }
            break;
        }
    }
```
with:
```cpp
    switch (config.effect) {
        case EffectKind::None:
            break;
        case EffectKind::Invert:
            InvertBackBufferColors();
            break;
        case EffectKind::Bilinear:
            ApplyBilinearUpscale();
            break;
        case EffectKind::NVScaler:
        case EffectKind::NVSharpen: {
            // NIS shader port lands in a follow-up plan. Until then, these are recognized
            // and logged, and fall back to doing nothing, same as effect=none.
            static bool warned = false;
            if (!warned) {
                printf("[opengl32_enh_cpp] post_effects: effect='%s' is not implemented yet, "
                       "falling back to none\n", EffectName(config.effect));
                warned = true;
            }
            break;
        }
    }
```

- [ ] **Step 2: Build**

Run:
```bash
cmake --build --preset x86
```
Expected: succeeds with no errors.

- [ ] **Step 3: Manually verify the dispatcher actually reaches `ApplyBilinearUpscale()` through the real DLL**

`bilinear_upscale_test.cpp` (Task 2) already proves the pipeline itself works against real hardware, but it calls `ApplyBilinearUpscale()` directly, not through `post_effects.cpp`'s dispatch or the built DLL's exported `wglSwapBuffers`. This step closes that last gap, using the same throwaway-verification approach Plan A's Task 3 used (do NOT commit this file - write it, use it, delete it):

Write a small standalone program that:
1. Creates a real GL context the same way `bilinear_upscale_test.cpp` does (window, pixel format, `wglCreateContext`/`wglMakeCurrent`).
2. Clears the back buffer to a known color.
3. Loads `build-x86\opengl32_enh32.dll` via `LoadLibraryA`/`GetProcAddress` (same pattern as `wrapper_test.cpp`) and gets its exported `wglSwapBuffers` function pointer.
4. Writes `build-x86\anax_enhancer.ini` with `effect=bilinear`.
5. Calls the DLL's exported `wglSwapBuffers(hdc)` through the resolved pointer — this runs `ApplySelectedEffect()` (which now dispatches to `ApplyBilinearUpscale()`) before attempting the real forward call.
6. Confirms: no crash, no unexpected `printf` output (specifically, the "not implemented yet" line must NOT appear for `effect=bilinear` anymore — only the ordinary `gl_loader`/`bilinear_upscale` trace lines), and (if you want the strongest check) reads back the buffer with `glReadPixels` before/immediately after to confirm the pipeline actually touched it.

Report the captured output in your task report. If you hit a wrinkle getting this real-context-plus-loaded-DLL combination working (e.g. two different GL contexts somehow ending up involved), stop and report DONE_WITH_CONCERNS explaining exactly what you verified vs. couldn't, rather than guessing.

- [ ] **Step 4: Commit**

```bash
git add post_effects.cpp
git commit -m "$(cat <<'EOF'
Wire ApplyBilinearUpscale() into the effect dispatcher

effect=bilinear now runs the real GPU pipeline from Task 2 instead of
falling back to the "not implemented yet" no-op. nvscaler/nvsharpen
remain placeholders pending the NIS shader port (a follow-up plan).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review Notes

- **Spec coverage:** the spec's "GPU pipeline" section is implemented in full for the `bilinear` case (Task 2), with the one documented FBO-count deviation (see Scope note) and its rationale carried into Task 2's code comments. "File/module layout"'s `bilinear_upscale.h/.cpp` entry is covered by Task 2; the `post_effects.cpp` dispatch entry for `bilinear` is covered by Task 3. `nis_config.h`/`nis_effect.h/.cpp` remain explicitly out of scope (Plan C).
- **Placeholder scan:** no TBD/TODO. The `nvscaler`/`nvsharpen` "not implemented yet" branch in `post_effects.cpp` (Task 3) is unchanged, real, shipped behavior carried over from Plan A - not new placeholder content from this plan.
- **Type consistency:** `GlComputeApi`'s 8 new fields (Task 1) are consumed by the exact same names in `bilinear_upscale.cpp` (Task 2). `ApplyBilinearUpscale()` (Task 2's declared signature) matches its call site in `post_effects.cpp` (Task 3) exactly.
