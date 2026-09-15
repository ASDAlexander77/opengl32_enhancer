# NIS Post-Effects — Plan A: Config + GL 4.3 Loader + Dispatcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the foundational plumbing for the NIS post-effects feature — a config file that selects an effect, a GL 4.3 compute-shader capability loader, and a dispatcher that replaces the current unconditional `InvertBackBufferColors()` call in `wglSwapBuffers` — so that `effect=none`/`invert` work exactly as today (regression-safe) and `effect=bilinear`/`nvscaler`/`nvsharpen` are recognized and safely no-op with a clear log message, ready for the GPU pipeline to be filled in by follow-up plans.

**Architecture:** Three small, independently-testable C++ modules (`config`, `gl_loader`, `post_effects`) added to the existing `opengl32_enh32`/`opengl32_enh64` proxy target, wired in at the single `wglSwapBuffers` hook point the generated `wrapper.cpp` already calls into. `gl_loader` resolves GL 4.3 compute-shader entry points itself (none are in opengl32.dll's static export table, so the generated per-function forwarding in `wrapper.cpp` doesn't cover them) via `wglGetProcAddress` on the real system `opengl32.dll`, independent of the proxy's own forwarding.

**Tech Stack:** Plain C++ (MSVC), Win32 (no `<windows.h>` inside anything linked into the proxy DLL — see Global Constraints), CMake/Ninja, the project's existing hand-rolled-typedef convention.

**Spec:** `docs/superpowers/specs/2026-09-15-nis-post-effects-design.md`

## Scope note (read before starting)

The spec covers three effects end-to-end (BilinearUpscale, NVScaler, NVSharpen) plus their shared GPU pipeline. That's too much for one plan to stay testable at each step, so this is **Plan A of three**:

- **Plan A (this plan):** config file, GL 4.3 capability loader, dispatcher. `none`/`invert` fully working (unchanged behavior). `bilinear`/`nvscaler`/`nvsharpen` recognized, logged, safely fall back to `none`.
- **Plan B (follow-up, not in this plan):** the actual GPU pipeline (capture texture → compute dispatch → blit back, state save/restore) proven out via the simplest effect, `BilinearUpscale`.
- **Plan C (follow-up, not in this plan):** vendor `NIS_Config.h`/`NIS_Scaler.h`/`NIS_Main.glsl`, the shader-generation script, and wire up `NVScaler`/`NVSharpen` on top of Plan B's pipeline.

## Global Constraints

- Never `#include <windows.h>` (or anything that pulls in `<wingdi.h>`) in any file that gets compiled into the `opengl32_enh32`/`opengl32_enh64` proxy target — it declares `wgl*`/`gl*` names as `dllimport`, which collides with `wrapper.cpp`'s `dllexport` definitions of the same names. Hand-declare only the specific kernel32 entry points needed, exactly like `wrapper.cpp`/`generators/gen_wrapper_cpp.py`/`pixel_invert.cpp` already do. Test executables (not linked into the proxy DLL) are exempt and may use `<windows.h>` freely.
- Debug/trace logging uses the existing convention: `printf("[opengl32_enh_cpp] ...")`.
- Build/verify against the `x86` CMake preset per [[project-32bit-prototype-first]] (the `x64`/`default` preset must also keep building, since the plain-C++ proxy sources are shared, but manual verification happens on `x86`).

---

### Task 1: Config file reader

**Files:**
- Create: `I:\anax_enhancer\config.h`
- Create: `I:\anax_enhancer\config.cpp`
- Create: `I:\anax_enhancer\config_test.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt` (add `config.cpp` to the proxy target's sources; add a new `config_test` executable)

**Interfaces:**
- Produces (used by Task 3): `enum class EffectKind { None, Invert, Bilinear, NVScaler, NVSharpen };`, `struct AnaxConfig { EffectKind effect; float sharpness; float scale; };`, `AnaxConfig ParseConfigFile(const char* path);`, `const AnaxConfig& GetAnaxConfig();` — all declared in `config.h`.

- [ ] **Step 1: Write `config.h`**

```cpp
#pragma once

// Selects and configures the post-process effect applied in wglSwapBuffers (see
// post_effects.h). Read from an INI-style anax_enhancer.ini file next to this DLL - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md for the file format.

enum class EffectKind {
    None,
    Invert,
    Bilinear,
    NVScaler,
    NVSharpen,
};

struct AnaxConfig {
    EffectKind effect = EffectKind::None;
    float sharpness = 0.5f;
    float scale = 1.0f;
};

// Parses an INI-style config file at the given path. A missing file, a missing key, or an
// unparseable/out-of-range value falls back to the default for just that field and logs
// once via printf. Never fails outright - always returns a usable AnaxConfig. Exposed
// separately from GetAnaxConfig() below so it can be unit-tested without depending on this
// code actually being loaded as a DLL next to a real config file.
AnaxConfig ParseConfigFile(const char* path);

// Reads anax_enhancer.ini from the same directory as this DLL, once, and caches the result
// for the rest of the process lifetime (later calls are cheap and return the same object).
const AnaxConfig& GetAnaxConfig();
```

- [ ] **Step 2: Write `config_test.cpp` (the failing test)**

```cpp
// Exercises ParseConfigFile() against hand-written fixture .ini files. Run manually and
// check its exit code (this project has no unit-test framework/ctest wiring - see
// wrapper_test.cpp for the same "small executable, check exit code" pattern).
#include <cstdio>
#include <cstring>

#include "config.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        printf("PASS: %s\n", what);
    }
}

void WriteFixture(const char* path, const char* contents) {
    FILE* f = fopen(path, "w");
    if (f == nullptr) {
        printf("FAIL: could not create fixture '%s'\n", path);
        exit(1);
    }
    fputs(contents, f);
    fclose(f);
}

}  // namespace

int main() {
    // Missing file -> defaults.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.effect == EffectKind::None, "missing file falls back to effect=none");
        Check(config.sharpness == 0.5f, "missing file falls back to default sharpness");
        Check(config.scale == 1.0f, "missing file falls back to default scale");
    }

    // Well-formed file.
    {
        WriteFixture("config_test_valid.ini",
            "effect=nvsharpen\n"
            "sharpness=0.75\n"
            "scale=0.8\n");
        AnaxConfig config = ParseConfigFile("config_test_valid.ini");
        Check(config.effect == EffectKind::NVSharpen, "valid file parses effect=nvsharpen");
        Check(config.sharpness == 0.75f, "valid file parses sharpness=0.75");
        Check(config.scale == 0.8f, "valid file parses scale=0.8");
    }

    // Comments, blank lines, surrounding whitespace, inline comments.
    {
        WriteFixture("config_test_comments.ini",
            "; this is a comment\n"
            "\n"
            "  effect = bilinear   \n"
            "scale=0.6 ; half res\n");
        AnaxConfig config = ParseConfigFile("config_test_comments.ini");
        Check(config.effect == EffectKind::Bilinear, "comments/whitespace: effect=bilinear parsed");
        Check(config.scale == 0.6f, "comments/whitespace: scale=0.6 parsed with inline comment stripped");
    }

    // Bad values fall back to defaults for just that field, not the whole config.
    {
        WriteFixture("config_test_bad.ini",
            "effect=not_a_real_effect\n"
            "sharpness=nope\n"
            "scale=5.0\n");
        AnaxConfig config = ParseConfigFile("config_test_bad.ini");
        Check(config.effect == EffectKind::None, "unrecognized effect falls back to none");
        Check(config.sharpness == 0.5f, "unparseable sharpness falls back to default");
        Check(config.scale == 1.0f, "out-of-range scale (5.0) clamps to max 1.0");
    }

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
```

- [ ] **Step 3: Add the `config_test` executable to `CMakeLists.txt` and confirm it fails to build**

Add this near the bottom of `CMakeLists.txt`, after the existing `${WRAPPER_NAME_CXX}_test` block (it needs no TSLANG/arch gating — pure CRT + kernel32, builds the same on both presets):

```cmake
# Exercises config.cpp's INI parsing directly (no DLL/GL involved) - see config_test.cpp.
add_executable(config_test config_test.cpp config.cpp)
```

Run:
```bash
cmake --build --preset x86 --target config_test
```
Expected: FAIL to link/compile — `config.cpp` doesn't exist yet, `config.h` has no matching implementation.

- [ ] **Step 4: Write `config.cpp` (minimal implementation to make the test pass)**

```cpp
// See config.h. Reads anax_enhancer.ini (INI-style key=value lines, ';'/'#' comments,
// inline ';' comments) next to this DLL.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

// Same no-<windows.h> discipline as the rest of this DLL - see wrapper.cpp's header
// comment / generators/gen_wrapper_cpp.py: <windows.h> declares dllimport wgl*/gl* names
// that collide with wrapper.cpp's dllexport definitions of the same names.
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HMODULE;

extern "C" {
    __declspec(dllimport) BOOL __stdcall GetModuleHandleExA(DWORD dwFlags, const char* lpModuleName, HMODULE* phModule);
    __declspec(dllimport) DWORD __stdcall GetModuleFileNameA(HMODULE hModule, char* lpFilename, DWORD nSize);
}

namespace {

const DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 0x00000004;
const DWORD GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x00000002;

// Any function defined in this module works as the address anchor for
// GetModuleHandleExA(..._FROM_ADDRESS, ...) below - it identifies "the module this code
// is running as", which is this DLL when linked into the proxy, or the test .exe itself
// when linked into config_test.
void AddressAnchor() {}

bool GetIniPathNextToThisModule(char* outPath, size_t outPathSize) {
    HMODULE hModule = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<const char*>(&AddressAnchor), &hModule)) {
        return false;
    }
    char modulePath[512];
    DWORD len = GetModuleFileNameA(hModule, modulePath, sizeof(modulePath));
    if (len == 0 || len >= sizeof(modulePath)) {
        return false;
    }
    char* lastSlash = nullptr;
    for (char* p = modulePath; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            lastSlash = p;
        }
    }
    if (lastSlash == nullptr) {
        return false;
    }
    *(lastSlash + 1) = '\0';
    return snprintf(outPath, outPathSize, "%sanax_enhancer.ini", modulePath) > 0;
}

void Trim(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') {
        ++start;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

EffectKind ParseEffect(const char* value) {
    if (strcmp(value, "none") == 0) return EffectKind::None;
    if (strcmp(value, "invert") == 0) return EffectKind::Invert;
    if (strcmp(value, "bilinear") == 0) return EffectKind::Bilinear;
    if (strcmp(value, "nvscaler") == 0) return EffectKind::NVScaler;
    if (strcmp(value, "nvsharpen") == 0) return EffectKind::NVSharpen;
    printf("[opengl32_enh_cpp] config: unrecognized effect '%s', falling back to none\n", value);
    return EffectKind::None;
}

float ParseClampedFloat(const char* value, float minValue, float maxValue, float fallback, const char* key) {
    char* end = nullptr;
    float parsed = strtof(value, &end);
    if (end == value) {
        printf("[opengl32_enh_cpp] config: '%s' value '%s' is not a number, using default %.3f\n", key, value, fallback);
        return fallback;
    }
    if (parsed < minValue || parsed > maxValue) {
        float clamped = parsed < minValue ? minValue : maxValue;
        printf("[opengl32_enh_cpp] config: '%s' value %.3f out of range [%.3f, %.3f], clamping to %.3f\n",
               key, parsed, minValue, maxValue, clamped);
        return clamped;
    }
    return parsed;
}

AnaxConfig LoadConfig() {
    AnaxConfig config;

    char iniPath[512];
    if (!GetIniPathNextToThisModule(iniPath, sizeof(iniPath))) {
        printf("[opengl32_enh_cpp] config: FAILED to determine module directory, using defaults (effect=none)\n");
        return config;
    }
    return ParseConfigFile(iniPath);
}

}  // namespace

AnaxConfig ParseConfigFile(const char* path) {
    AnaxConfig config;

    FILE* f = fopen(path, "r");
    if (f == nullptr) {
        printf("[opengl32_enh_cpp] config: no config file at '%s', using defaults (effect=none)\n", path);
        return config;
    }

    char line[256];
    while (fgets(line, sizeof(line), f) != nullptr) {
        Trim(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') {
            continue;
        }
        char* eq = strchr(line, '=');
        if (eq == nullptr) {
            continue;
        }
        *eq = '\0';
        char* key = line;
        char* value = eq + 1;
        char* comment = strchr(value, ';');
        if (comment != nullptr) {
            *comment = '\0';
        }
        Trim(key);
        Trim(value);

        if (strcmp(key, "effect") == 0) {
            config.effect = ParseEffect(value);
        } else if (strcmp(key, "sharpness") == 0) {
            config.sharpness = ParseClampedFloat(value, 0.0f, 1.0f, config.sharpness, "sharpness");
        } else if (strcmp(key, "scale") == 0) {
            config.scale = ParseClampedFloat(value, 0.5f, 1.0f, config.scale, "scale");
        }
    }
    fclose(f);

    printf("[opengl32_enh_cpp] config: loaded from '%s' (effect=%d, sharpness=%.3f, scale=%.3f)\n",
           path, static_cast<int>(config.effect), config.sharpness, config.scale);
    return config;
}

const AnaxConfig& GetAnaxConfig() {
    static const AnaxConfig config = LoadConfig();
    return config;
}
```

- [ ] **Step 5: Build and run `config_test`, verify it passes**

Run:
```bash
cmake --build --preset x86 --target config_test
./build-x86/config_test.exe
```
Expected: every `Check(...)` line prints `PASS:`, final line `All checks passed.`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add config.h config.cpp config_test.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Add anax_enhancer.ini config reader for post-process effect selection

Parses effect/sharpness/scale from an INI file next to the DLL, with
per-field fallback to defaults on any missing/bad value. Not yet wired
into wglSwapBuffers - that's Task 3.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: GL 4.3 compute-shader capability loader

**Files:**
- Create: `I:\anax_enhancer\gl_loader.h`
- Create: `I:\anax_enhancer\gl_loader.cpp`
- Create: `I:\anax_enhancer\gl_loader_test.cpp`
- Modify: `I:\anax_enhancer\CMakeLists.txt` (add `gl_loader.cpp` to the proxy target's sources; add a new `gl_loader_test` executable)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces (used by Plan B): `struct GlComputeApi { ... bool loaded; };` and `const GlComputeApi& GetGlComputeApi();`, declared in `gl_loader.h`.

- [ ] **Step 1: Write `gl_loader.h`**

```cpp
#pragma once

// Resolves the GL 4.3+ function pointers the post-process compute-shader effects need
// (BilinearUpscale/NVScaler/NVSharpen - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md). None of these are part of
// opengl32.dll's static export table, so wrapper.cpp's generated per-function forwarding
// doesn't cover them - this resolves them independently via wglGetProcAddress on the real
// system opengl32.dll.

typedef void (__stdcall *PFNGLBINDIMAGETEXTUREPROC)(unsigned int unit, unsigned int texture, int level,
    unsigned char layered, int layer, unsigned int access, unsigned int format);
typedef void (__stdcall *PFNGLDISPATCHCOMPUTEPROC)(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ);
typedef void (__stdcall *PFNGLMEMORYBARRIERPROC)(unsigned int barriers);
typedef unsigned int (__stdcall *PFNGLCREATESHADERPROC)(unsigned int type);
typedef void (__stdcall *PFNGLSHADERSOURCEPROC)(unsigned int shader, int count, const char* const* string, const int* length);
typedef void (__stdcall *PFNGLCOMPILESHADERPROC)(unsigned int shader);
typedef void (__stdcall *PFNGLGETSHADERIVPROC)(unsigned int shader, unsigned int pname, int* params);
typedef void (__stdcall *PFNGLGETSHADERINFOLOGPROC)(unsigned int shader, int bufSize, int* length, char* infoLog);
typedef void (__stdcall *PFNGLDELETESHADERPROC)(unsigned int shader);
typedef unsigned int (__stdcall *PFNGLCREATEPROGRAMPROC)(void);
typedef void (__stdcall *PFNGLATTACHSHADERPROC)(unsigned int program, unsigned int shader);
typedef void (__stdcall *PFNGLLINKPROGRAMPROC)(unsigned int program);
typedef void (__stdcall *PFNGLGETPROGRAMIVPROC)(unsigned int program, unsigned int pname, int* params);
typedef void (__stdcall *PFNGLGETPROGRAMINFOLOGPROC)(unsigned int program, int bufSize, int* length, char* infoLog);
typedef void (__stdcall *PFNGLDELETEPROGRAMPROC)(unsigned int program);
typedef void (__stdcall *PFNGLUSEPROGRAMPROC)(unsigned int program);
typedef void (__stdcall *PFNGLGENFRAMEBUFFERSPROC)(int n, unsigned int* framebuffers);
typedef void (__stdcall *PFNGLDELETEFRAMEBUFFERSPROC)(int n, const unsigned int* framebuffers);
typedef void (__stdcall *PFNGLBINDFRAMEBUFFERPROC)(unsigned int target, unsigned int framebuffer);
typedef void (__stdcall *PFNGLFRAMEBUFFERTEXTURE2DPROC)(unsigned int target, unsigned int attachment,
    unsigned int textarget, unsigned int texture, int level);
typedef void (__stdcall *PFNGLBLITFRAMEBUFFERPROC)(int srcX0, int srcY0, int srcX1, int srcY1,
    int dstX0, int dstY0, int dstX1, int dstY1, unsigned int mask, unsigned int filter);
typedef void (__stdcall *PFNGLGENBUFFERSPROC)(int n, unsigned int* buffers);
typedef void (__stdcall *PFNGLDELETEBUFFERSPROC)(int n, const unsigned int* buffers);
typedef void (__stdcall *PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);
typedef void (__stdcall *PFNGLBINDBUFFERBASEPROC)(unsigned int target, unsigned int index, unsigned int buffer);
typedef void (__stdcall *PFNGLBUFFERDATAPROC)(unsigned int target, long long size, const void* data, unsigned int usage);
typedef void (__stdcall *PFNGLBUFFERSUBDATAPROC)(unsigned int target, long long offset, long long size, const void* data);
typedef void (__stdcall *PFNGLACTIVETEXTUREPROC)(unsigned int texture);
typedef void (__stdcall *PFNGLTEXSTORAGE2DPROC)(unsigned int target, int levels, unsigned int internalformat, int width, int height);

struct GlComputeApi {
    PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = nullptr;
    PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = nullptr;
    PFNGLMEMORYBARRIERPROC glMemoryBarrier = nullptr;
    PFNGLCREATESHADERPROC glCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC glDeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC glAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
    PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = nullptr;
    PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
    PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
    PFNGLBINDBUFFERBASEPROC glBindBufferBase = nullptr;
    PFNGLBUFFERDATAPROC glBufferData = nullptr;
    PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
    PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
    PFNGLTEXSTORAGE2DPROC glTexStorage2D = nullptr;

    bool loaded = false;
};

// Resolves every pointer above via wglGetProcAddress against the real system opengl32.dll.
// A context must already be current (true at every call site - see post_effects.cpp).
// Returns false (leaving api.loaded false) if any pointer comes back null; callers must
// then treat every compute-shader effect as unavailable and fall back to effect=none.
bool LoadGlComputeApi(GlComputeApi& api);

// Cached, process-lifetime singleton wrapping LoadGlComputeApi(): the first call resolves
// the API (logging once on failure), later calls return the same cached result.
const GlComputeApi& GetGlComputeApi();
```

- [ ] **Step 2: Write `gl_loader_test.cpp` (the failing test)**

```cpp
// Creates a real OpenGL context against a hidden window and checks that gl_loader.cpp can
// resolve every GL 4.3 compute-shader entry point it needs. This is the only place in the
// new post-effects code that creates a context from scratch - everywhere else
// (gl_loader.cpp, post_effects.cpp, ...) runs inside a context the host app already made
// current via wglSwapBuffers. Free to use <windows.h> here: unlike wrapper.cpp, this file
// is a standalone .exe and doesn't itself export dllexport definitions of any wgl*/gl*
// names, so there's no collision (see Global Constraints in the plan).
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxGlLoaderTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "gl_loader_test", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
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

    typedef const unsigned char* (__stdcall *PFNGLGETSTRINGPROC)(unsigned int);
    PFNGLGETSTRINGPROC pGlGetString =
        (PFNGLGETSTRINGPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetString");
    const unsigned int GL_VERSION_ENUM = 0x1F02;
    const unsigned char* version = pGlGetString ? pGlGetString(GL_VERSION_ENUM) : (const unsigned char*)"<unknown>";
    printf("Real GL context created. GL_VERSION = %s\n", version);

    GlComputeApi api;
    bool ok = LoadGlComputeApi(api);
    printf("LoadGlComputeApi -> %s\n", ok ? "PASS (all entry points resolved)" : "FAIL (see FAILED lines above)");

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
```

- [ ] **Step 3: Add the `gl_loader_test` executable to `CMakeLists.txt` and confirm it fails to build**

```cmake
# Creates a real GL context and checks gl_loader.cpp resolves every GL 4.3 compute-shader
# entry point it needs - see gl_loader_test.cpp.
add_executable(gl_loader_test gl_loader_test.cpp gl_loader.cpp)
target_link_libraries(gl_loader_test opengl32 gdi32 user32)
```

Run:
```bash
cmake --build --preset x86 --target gl_loader_test
```
Expected: FAIL — `gl_loader.cpp` doesn't exist yet.

- [ ] **Step 4: Write `gl_loader.cpp` (minimal implementation to make the test pass)**

```cpp
#include <cstdio>

#include "gl_loader.h"

// Same no-<windows.h> discipline as the rest of this DLL (see Global Constraints in the
// plan / wrapper.cpp's header comment). Resolved independently of wrapper.cpp's own
// exported wglGetProcAddress wrapper on purpose: that export only exists once this file is
// linked into the proxy DLL, but gl_loader.cpp is also linked directly into
// gl_loader_test.cpp as a standalone .exe (see Task 2's Interfaces) where no such export
// exists. Loading the real opengl32.dll and resolving wglGetProcAddress from it directly
// works identically in both cases, mirroring wrapper.cpp's own EnsureRealOpenGL32 pattern.
typedef unsigned long DWORD;
typedef void* HMODULE;
typedef void* (__stdcall *PFNWGLGETPROCADDRESSPROC)(const char* procName);

extern "C" {
    __declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
    __declspec(dllimport) DWORD __stdcall GetLastError(void);
}

namespace {

PFNWGLGETPROCADDRESSPROC GetRealWglGetProcAddress() {
    static PFNWGLGETPROCADDRESSPROC fn = [] {
        HMODULE real = LoadLibraryA("C:\\Windows\\System32\\opengl32.dll");
        if (real == nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: FAILED to load real opengl32.dll, GetLastError=%lu\n", GetLastError());
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

}  // namespace

bool LoadGlComputeApi(GlComputeApi& api) {
    // Intentionally non-short-circuiting (&=, not &&): every missing entry point should be
    // logged in one run, not just the first one found.
    bool ok = true;
    ok &= Resolve("glBindImageTexture", api.glBindImageTexture);
    ok &= Resolve("glDispatchCompute", api.glDispatchCompute);
    ok &= Resolve("glMemoryBarrier", api.glMemoryBarrier);
    ok &= Resolve("glCreateShader", api.glCreateShader);
    ok &= Resolve("glShaderSource", api.glShaderSource);
    ok &= Resolve("glCompileShader", api.glCompileShader);
    ok &= Resolve("glGetShaderiv", api.glGetShaderiv);
    ok &= Resolve("glGetShaderInfoLog", api.glGetShaderInfoLog);
    ok &= Resolve("glDeleteShader", api.glDeleteShader);
    ok &= Resolve("glCreateProgram", api.glCreateProgram);
    ok &= Resolve("glAttachShader", api.glAttachShader);
    ok &= Resolve("glLinkProgram", api.glLinkProgram);
    ok &= Resolve("glGetProgramiv", api.glGetProgramiv);
    ok &= Resolve("glGetProgramInfoLog", api.glGetProgramInfoLog);
    ok &= Resolve("glDeleteProgram", api.glDeleteProgram);
    ok &= Resolve("glUseProgram", api.glUseProgram);
    ok &= Resolve("glGenFramebuffers", api.glGenFramebuffers);
    ok &= Resolve("glDeleteFramebuffers", api.glDeleteFramebuffers);
    ok &= Resolve("glBindFramebuffer", api.glBindFramebuffer);
    ok &= Resolve("glFramebufferTexture2D", api.glFramebufferTexture2D);
    ok &= Resolve("glBlitFramebuffer", api.glBlitFramebuffer);
    ok &= Resolve("glGenBuffers", api.glGenBuffers);
    ok &= Resolve("glDeleteBuffers", api.glDeleteBuffers);
    ok &= Resolve("glBindBuffer", api.glBindBuffer);
    ok &= Resolve("glBindBufferBase", api.glBindBufferBase);
    ok &= Resolve("glBufferData", api.glBufferData);
    ok &= Resolve("glBufferSubData", api.glBufferSubData);
    ok &= Resolve("glActiveTexture", api.glActiveTexture);
    ok &= Resolve("glTexStorage2D", api.glTexStorage2D);

    api.loaded = ok;
    if (ok) {
        printf("[opengl32_enh_cpp] gl_loader: all GL 4.3 compute entry points resolved OK\n");
    } else {
        printf("[opengl32_enh_cpp] gl_loader: one or more GL 4.3 entry points unavailable - "
               "compute-shader effects (bilinear/nvscaler/nvsharpen) are disabled on this context\n");
    }
    return ok;
}

const GlComputeApi& GetGlComputeApi() {
    static const GlComputeApi api = [] {
        GlComputeApi result;
        LoadGlComputeApi(result);
        return result;
    }();
    return api;
}
```

- [ ] **Step 5: Build and run `gl_loader_test`, verify it passes**

Run:
```bash
cmake --build --preset x86 --target gl_loader_test
./build-x86/gl_loader_test.exe
```
Expected: prints a real `GL_VERSION` string, then `LoadGlComputeApi -> PASS (all entry points resolved)`, exit code 0. (If the dev machine's GPU/driver is genuinely below GL 4.3, this will legitimately print `FAIL` with the specific missing entry points — that's the loader correctly doing its job, not a bug in this code. Confirm which case you're in before moving on.)

- [ ] **Step 6: Commit**

```bash
git add gl_loader.h gl_loader.cpp gl_loader_test.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
Add GL 4.3 compute-shader entry point loader

Resolves the ~28 GL 4.3 functions the post-process compute-shader
effects need via wglGetProcAddress, independent of wrapper.cpp's
per-function forwarding (those functions aren't part of opengl32.dll's
static export table). Not yet wired into wglSwapBuffers - that's Task 3.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Dispatcher, generator wiring, and end-to-end verification

**Files:**
- Create: `I:\anax_enhancer\post_effects.h`
- Create: `I:\anax_enhancer\post_effects.cpp`
- Modify: `I:\anax_enhancer\generators\gen_wrapper_cpp.py` (call `ApplySelectedEffect()` instead of `InvertBackBufferColors()`; fix the `CPP_OUT`/`DEF_OUT` path constants — see below)
- Modify: `I:\anax_enhancer\wrapper.cpp` (regenerated, not hand-edited)
- Modify: `I:\anax_enhancer\CMakeLists.txt` (add `post_effects.cpp` to the proxy target's sources)

**Interfaces:**
- Consumes: `GetAnaxConfig()`/`EffectKind`/`AnaxConfig` (Task 1), `InvertBackBufferColors()` (existing `pixel_invert.h`).
- Produces: `void ApplySelectedEffect();`, called from the generated `wrapper.cpp`'s `wglSwapBuffers`.

- [ ] **Step 1: Write `post_effects.h`**

```cpp
#pragma once

// Entry point called from wglSwapBuffers (see wrapper.cpp / generators/gen_wrapper_cpp.py)
// just before the real swap. Reads the cached config (see config.h) and dispatches to the
// selected effect. Never fails or throws: every effect that isn't implemented yet, or that
// needs GL 4.3 compute-shader support this context doesn't have, silently no-ops (the real
// swap still happens, unmodified).
void ApplySelectedEffect();
```

- [ ] **Step 2: Write `post_effects.cpp`**

```cpp
#include <cstdio>

#include "post_effects.h"
#include "config.h"
#include "pixel_invert.h"

namespace {

const char* EffectName(EffectKind effect) {
    switch (effect) {
        case EffectKind::None: return "none";
        case EffectKind::Invert: return "invert";
        case EffectKind::Bilinear: return "bilinear";
        case EffectKind::NVScaler: return "nvscaler";
        case EffectKind::NVSharpen: return "nvsharpen";
    }
    return "<unknown>";
}

}  // namespace

void ApplySelectedEffect() {
    const AnaxConfig& config = GetAnaxConfig();

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
}
```

- [ ] **Step 3: Wire `post_effects.h` and `ApplySelectedEffect()` into the generator**

In `generators\gen_wrapper_cpp.py`:

1. Fix the output paths (currently stale — the generated files were renamed from `wrapper32.cpp`/`wrapper32.def` to `wrapper.cpp`/`wrapper.def` by hand, but the generator still writes the old names):

```python
CPP_OUT = r"..\wrapper.cpp"
DEF_OUT = r"..\wrapper.def"
```

2. Add the new include, next to the existing `pixel_invert.h` one:

```python
    lines.append("#include <cstdio>")
    lines.append('#include "pixel_invert.h"')
    lines.append('#include "post_effects.h"')
```

3. Replace the call inserted at the `wglSwapBuffers` hook:

```python
        if name == "wglSwapBuffers":
            lines.append("    ApplySelectedEffect();")
```

(This replaces the existing `lines.append("    InvertBackBufferColors();")` line under the same `if name == "wglSwapBuffers":` check.)

- [ ] **Step 4: Regenerate `wrapper.cpp`/`wrapper.def` and diff the result**

Run:
```bash
cd generators
python gen_wrapper_cpp.py
cd ..
git diff wrapper.cpp wrapper.def
```
Expected diff: `#include "post_effects.h"` added near the top, and the `wglSwapBuffers` body now calls `ApplySelectedEffect();` instead of `InvertBackBufferColors();`. Nothing else should change (confirms the `CPP_OUT`/`DEF_OUT` fix didn't otherwise alter output — if the diff is much larger than that, stop and investigate before continuing).

- [ ] **Step 5: Add `post_effects.cpp` to the proxy target in `CMakeLists.txt`**

Modify the existing line:
```cmake
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp)
```
to:
```cmake
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp)
```

- [ ] **Step 6: Build both presets**

Run:
```bash
cmake --build --preset x86
cmake --build --preset default
```
Expected: both succeed with no errors (the x64 preset only builds the plain-C++ proxy target plus the TSLANG project — Task 1/2's new sources compile the same way there).

- [ ] **Step 7: Manual regression + smoke check on the x86 build**

This exercises the actual behavior change end to end, since there's no automated GL-context test for `post_effects.cpp` itself in this plan (Plan B adds one alongside the real GPU pipeline).

1. In `build-x86/`, create `anax_enhancer.ini` with `effect=invert`.
2. Run `build-x86/opengl32_enh32_test.exe` (or inject the DLL into a real 32-bit OpenGL app per your existing prototyping setup) and confirm behavior is unchanged from before this plan (invert still works — this is the regression check).
3. Change `anax_enhancer.ini` to `effect=nvscaler` and re-run. Confirm the log now prints:
   ```
   [opengl32_enh_cpp] post_effects: effect='nvscaler' is not implemented yet, falling back to none
   ```
   exactly once (not once per frame), and that rendering proceeds normally (no crash, no visual change).
4. Delete `anax_enhancer.ini` entirely and re-run. Confirm the log prints the "no config file... using defaults (effect=none)" message and rendering proceeds normally.

- [ ] **Step 8: Commit**

```bash
git add post_effects.h post_effects.cpp generators/gen_wrapper_cpp.py wrapper.cpp wrapper.def CMakeLists.txt
git commit -m "$(cat <<'EOF'
Wire config-driven effect dispatch into wglSwapBuffers

wglSwapBuffers now calls the new ApplySelectedEffect() (reading
anax_enhancer.ini) instead of unconditionally running the invert
effect. none/invert behave exactly as before; bilinear/nvscaler/
nvsharpen are recognized and log a clear "not implemented yet"
fallback until the GPU pipeline lands in a follow-up plan.

Also fixes gen_wrapper_cpp.py's CPP_OUT/DEF_OUT constants, which still
pointed at the old wrapper32.cpp/wrapper32.def filenames after those
were renamed to wrapper.cpp/wrapper.def.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review Notes

- **Spec coverage:** "Config file" → Task 1. "GL version requirement and loader" → Task 2. "Error handling" (config + loader halves) → Tasks 1 & 2. The dispatcher half of "File/module layout" (`post_effects.h/.cpp`, generator wiring, CMake wiring) → Task 3. "GPU pipeline", "Shader source and NIS reuse", and the `nis_config.h`/`nis_effect.h/.cpp`/`bilinear_upscale.h/.cpp` file-layout entries are explicitly out of scope for Plan A (see Scope note) and land in Plans B/C.
- **Placeholder scan:** no TBD/TODO; the `bilinear`/`nvscaler`/`nvsharpen` "not implemented yet" branch in `post_effects.cpp` is real, intentional, shipped behavior for this plan (logged fallback), not a stand-in for missing plan content.
- **Type consistency:** `EffectKind`/`AnaxConfig` (Task 1) match their use in `post_effects.cpp` (Task 3). `GlComputeApi`/`GetGlComputeApi()` (Task 2) aren't consumed yet in this plan — they're the interface Plan B builds on — and are exercised directly by `gl_loader_test.cpp` in Task 2.
