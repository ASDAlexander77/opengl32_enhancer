# Modelview Capture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Record the game's camera (view) matrix once per frame, recording-only, so later work can reproject between frames.

**Architecture:** A new `modelview_capture.{h,cpp}` module holds a small state machine fed by notification hooks emitted into the generated `wrapper.cpp`. It never computes a matrix: it decides *when* the modelview holds the camera and then reads the driver's own `GL_MODELVIEW_MATRIX` via `glGetFloatv`, exactly once per frame. `glFrustum` arms a world pass, `glOrtho` disarms it, modelview edits count only at push/pop depth 0, and the read is latched at the first depth-0 `glPushMatrix` — which is what excludes entity transforms.

**Tech Stack:** C++ (MSVC x86, `/std:c++17`), CMake + Ninja via `CMakePresets.json`, CTest. Tests are standalone `main()` executables that create a real Win32 window and a real OpenGL context and print `PASS`/`FAIL`. Python 3 for the wrapper generator.

**Spec:** `docs/superpowers/specs/2026-09-19-modelview-capture-design.md`

## Global Constraints

- **Recording only.** No hook may alter a call's arguments, skip forwarding, or change what the game renders. Every hook in the generated wrapper is emitted *before* the existing passthrough line and falls through to it.
- **Single render thread assumed.** No synchronization, matching `projection_capture.h` and `gl_loader.h`. Do not add mutexes or atomics.
- **No stage behaviour changes.** `ssr.cpp`, `taa.cpp`, `ssao.cpp` and `post_effects.cpp` are not modified by this plan. Nothing consumes the capture yet.
- **`glGetFloatv` is resolved locally**, via `GetSystemDirectoryA` + `LoadLibraryA` + `GetProcAddress` in `modelview_capture.cpp` — **not** through `gl_loader.h`, which gates on GL 4.3 as a unit. Matrix capture must keep working where the compute stages cannot run.
- **No `<windows.h>` in the generated wrapper.** `generators/gen_wrapper_cpp.py` deliberately avoids it (see its module docstring). `modelview_capture.cpp` is an ordinary translation unit and may include it, as `window_override.cpp` does.
- **GL constants used:** `GL_MODELVIEW = 0x1700`, `GL_PROJECTION = 0x1701`, `GL_MODELVIEW_MATRIX = 0x0BA6`. Declare them as local `const unsigned int` in an anonymous namespace; do not include a GL header.
- **Diagnostics are `printf`.** `debug_log.h` redirects stdout to `opengl32_enhancer.log` inside a game; there is no logging API.
- **Config default must be an exact no-op.** `cameraLogInterval = 0` (off).
- **Build commands** (run from the repo root):
  - Configure once: `cmake --preset x86`
  - Build: `cmake --build --preset x86`
  - Test: `cd build-x86 && ctest --output-on-failure`
  - Single test: `cd build-x86 && ctest -R modelview_capture_test --output-on-failure`
- **Do not edit `wrapper.cpp` or `wrapper.def` by hand.** They are generated: `cd generators && python gen_wrapper_cpp.py`.
- **Do not touch `config_writer.cpp` or `config_editor.cpp`.** Their allow-list is "keys the editor can change"; `windowWidth`, `windowHeight` and `frameDumpKey` are all deliberately absent, and unmanaged lines are copied through untouched. `cameraLogInterval` belongs with those.

---

## File Structure

| File | Responsibility |
|---|---|
| `modelview_capture.h` (new) | The hook API, `CameraMatrix`/`CameraPose`, accessors, and the documented limits. |
| `modelview_capture.cpp` (new) | The state machine, the `glGetFloatv` resolution, the decomposition, and the optional per-N-frames log. |
| `modelview_capture_test.cpp` (new) | Real-GL unit test: drives the actual call sequences and asserts the gate, not the arithmetic. |
| `generators/gen_wrapper_cpp.py` (modify) | Emits the sixteen hook calls. |
| `wrapper.cpp` (regenerate) | Generated output. Never hand-edited. |
| `CMakeLists.txt` (modify) | DLL source list + the new test target and its `gpu` label. |
| `config.h`, `config.cpp` (modify) | `cameraLogInterval`. |
| `opengl32_enhancer.ini`, `README.md`, `docs/enhancement-opportunities.md` (modify) | Documentation. |

---

### Task 1: The module, and the happy-path latch

Establishes the file, the interface, the `glGetFloatv` resolution, and the simplest behaviour that is worth anything: after a world pass, the camera is captured; with no world pass, nothing is.

The gate that makes it *correct* (ortho, push/pop depth) comes in Task 2. Do not implement it here — the Task 2 tests must be able to fail.

**Files:**
- Create: `modelview_capture.h`
- Create: `modelview_capture.cpp`
- Create: `modelview_capture_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `struct CameraMatrix { float m[16]; }`; `void NotifyWorldProjection()`; `void NotifyMatrixEdited()`; `void FinalizeCameraForFrame()`; `void AdvanceCameraHistory()`; `bool GetCapturedCamera(CameraMatrix& out)`.

- [ ] **Step 1: Write the failing test**

Create `modelview_capture_test.cpp`. This is the whole file for now; Tasks 2 and 3 add cases to it.

Note the ordering constraint: the "nothing captured yet" assertion **must run first**, before anything latches, because the module keeps process-global state and exports no reset (see the spec's Interface section).

```cpp
// Creates a real OpenGL context and drives modelview_capture.h's hooks alongside the real GL
// calls a Quake II-family engine makes, then checks the captured matrix against what the driver
// itself reports. The point is to verify the GATE - which matrix gets picked - not matrix
// arithmetic, which this module never performs. See modelview_capture.h.
#include <windows.h>
#include <cstdio>

#include "modelview_capture.h"

namespace {

const unsigned int GL_PROJECTION       = 0x1701;
const unsigned int GL_MODELVIEW        = 0x1700;
const unsigned int GL_MODELVIEW_MATRIX = 0x0BA6;

typedef void (__stdcall *PfnMatrixMode)(unsigned int);
typedef void (__stdcall *PfnLoadIdentity)(void);
typedef void (__stdcall *PfnFrustum)(double, double, double, double, double, double);
typedef void (__stdcall *PfnOrtho)(double, double, double, double, double, double);
typedef void (__stdcall *PfnRotatef)(float, float, float, float);
typedef void (__stdcall *PfnTranslatef)(float, float, float);
typedef void (__stdcall *PfnPushMatrix)(void);
typedef void (__stdcall *PfnPopMatrix)(void);
typedef void (__stdcall *PfnGetFloatv)(unsigned int, float*);

PfnMatrixMode   pMatrixMode   = nullptr;
PfnLoadIdentity pLoadIdentity = nullptr;
PfnFrustum      pFrustum      = nullptr;
PfnOrtho        pOrtho        = nullptr;
PfnRotatef      pRotatef      = nullptr;
PfnTranslatef   pTranslatef   = nullptr;
PfnPushMatrix   pPushMatrix   = nullptr;
PfnPopMatrix    pPopMatrix    = nullptr;
PfnGetFloatv    pGetFloatv    = nullptr;

int g_failures = 0;

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
}

bool MatricesEqual(const float* a, const float* b) {
    for (int i = 0; i < 16; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

void PrintMatrix(const char* label, const float* m) {
    printf("  %s = [%.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f]\n",
           label, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
           m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
}

// The projection half of Quake II's R_SetupGL: glMatrixMode(GL_PROJECTION), glLoadIdentity,
// then MYgluPerspective, which reaches GL as glFrustum. Both the real call and the hook.
void BeginWorldPass() {
    pMatrixMode(GL_PROJECTION);
    NotifyMatrixMode(GL_PROJECTION);
    pLoadIdentity();
    NotifyMatrixEdited();
    pFrustum(-1.0, 1.0, -0.75, 0.75, 1.0, 4096.0);
    NotifyWorldProjection();
}

// The modelview half: glLoadIdentity, five glRotatef, one glTranslatef, exactly as R_SetupGL
// issues them. eye is the world-space view origin the engine passes as -vieworg.
void SetWorldCamera(float pitch, float yaw, float roll, float eyeX, float eyeY, float eyeZ) {
    pMatrixMode(GL_MODELVIEW);
    NotifyMatrixMode(GL_MODELVIEW);
    pLoadIdentity();
    NotifyMatrixEdited();
    pRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    NotifyMatrixEdited();
    pRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    NotifyMatrixEdited();
    pRotatef(-roll, 1.0f, 0.0f, 0.0f);
    NotifyMatrixEdited();
    pRotatef(-pitch, 0.0f, 1.0f, 0.0f);
    NotifyMatrixEdited();
    pRotatef(-yaw, 0.0f, 0.0f, 1.0f);
    NotifyMatrixEdited();
    pTranslatef(-eyeX, -eyeY, -eyeZ);
    NotifyMatrixEdited();
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxModelviewCaptureTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "modelview_capture_test", WS_OVERLAPPEDWINDOW,
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

    HMODULE realGl = GetModuleHandleA("opengl32.dll");
    pMatrixMode   = (PfnMatrixMode)GetProcAddress(realGl, "glMatrixMode");
    pLoadIdentity = (PfnLoadIdentity)GetProcAddress(realGl, "glLoadIdentity");
    pFrustum      = (PfnFrustum)GetProcAddress(realGl, "glFrustum");
    pOrtho        = (PfnOrtho)GetProcAddress(realGl, "glOrtho");
    pRotatef      = (PfnRotatef)GetProcAddress(realGl, "glRotatef");
    pTranslatef   = (PfnTranslatef)GetProcAddress(realGl, "glTranslatef");
    pPushMatrix   = (PfnPushMatrix)GetProcAddress(realGl, "glPushMatrix");
    pPopMatrix    = (PfnPopMatrix)GetProcAddress(realGl, "glPopMatrix");
    pGetFloatv    = (PfnGetFloatv)GetProcAddress(realGl, "glGetFloatv");
    if (pMatrixMode == nullptr || pLoadIdentity == nullptr || pFrustum == nullptr ||
        pOrtho == nullptr || pRotatef == nullptr || pTranslatef == nullptr ||
        pPushMatrix == nullptr || pPopMatrix == nullptr || pGetFloatv == nullptr) {
        printf("FAIL: could not resolve the GL 1.1 matrix entry points\n");
        return 1;
    }

    // --- Case 1: nothing captured before any world pass. MUST run first: the module keeps
    // process-global state and deliberately exports no test-only reset, so this is the only
    // point at which "not captured yet" can be observed.
    {
        CameraMatrix camera;
        Check(!GetCapturedCamera(camera),
              "no glFrustum yet: GetCapturedCamera() returns false rather than guessing");
    }

    // --- Case 2: a full R_SetupGL sequence, latched at swap, equals what the driver holds.
    {
        BeginWorldPass();
        SetWorldCamera(10.0f, 45.0f, 0.0f, 100.0f, 200.0f, 30.0f);

        float fromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, fromDriver);

        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        Check(got, "after a world pass: GetCapturedCamera() returns true");
        if (got && !MatricesEqual(camera.m, fromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("driver  ", fromDriver);
        }
        Check(got && MatricesEqual(camera.m, fromDriver),
              "captured matrix is bit-identical to the driver's GL_MODELVIEW_MATRIX");

        AdvanceCameraHistory();
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf(g_failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Add the CMake target so the test can build**

In `CMakeLists.txt`, immediately after the `ssr_test` block (search for `add_executable(ssr_test`), add:

```cmake
# Creates a real GL context, drives the matrix-call sequence a Quake II-family engine makes
# (world pass, entity push/pop, HUD ortho pass) alongside modelview_capture.h's hooks, and
# checks the captured camera against the driver's own GL_MODELVIEW_MATRIX - see
# modelview_capture_test.cpp. Links config.cpp and debug_log.cpp because the optional
# cameraLogInterval diagnostic reads the config, the same way window_override_test does.
add_executable(modelview_capture_test modelview_capture_test.cpp modelview_capture.cpp
    config.cpp debug_log.cpp)
target_link_libraries(modelview_capture_test opengl32 gdi32 user32)
```

Then add `modelview_capture_test` to the **second** `foreach(test_target ...)` list in the same file — the one followed by `set_tests_properties(${test_target} PROPERTIES LABELS gpu)`. Put it after `ssr_test`.

- [ ] **Step 3: Run the test to verify it fails**

Run:
```sh
cmake --preset x86
cmake --build --preset x86 --target modelview_capture_test
```
Expected: FAIL at compile time — `Cannot open include file: 'modelview_capture.h'`.

- [ ] **Step 4: Write the header**

Create `modelview_capture.h`:

```cpp
#pragma once

// Records the game's CAMERA (view) matrix once per frame, so stages can relate this frame's
// pixels to the previous frame's. Recording only - like projection_capture.h, every hook here
// is a notification issued before the real call is forwarded, and none of them changes what the
// game renders.
//
// projection_capture.h already records the world PROJECTION. That is only half of what is needed
// to place a pixel in space: the projection says how view space maps to the screen, but not
// where the camera is or which way it points. taa.h states the consequence outright - "this
// proxy has no access to the app's per-object motion vectors" - and ssr.h is forced to measure
// its up-facing gate in view space rather than world space for the same reason.
//
// This module never computes a matrix. It decides WHEN the modelview matrix holds the camera,
// and then reads the driver's own GL_MODELVIEW_MATRIX. That way the captured value is
// bit-for-bit what the game's GL actually holds and cannot drift from it.
//
// Knowing when is the hard part, and it is a heuristic, not a fact. id Tech 2-era engines draw
// the HUD last under an orthographic projection, and every entity in the world pass overwrites
// the modelview with its own object transform, so "the matrix at swap time" is neither. The
// rules, which mirror and extend projection_capture.h's:
//
//   - glFrustum arms a world pass; glOrtho disarms it. Exactly the discrimination
//     projection_capture.h already relies on, reused.
//   - Modelview edits count only at push/pop depth 0. Quake II-family engines issue every
//     entity transform inside glPushMatrix/glPopMatrix - R_RotateForEntity, R_DrawSkyBox, the
//     alias and brush model paths - so an edit at depth >= 1 is an object transform.
//   - The read is deferred to the first moment the camera is known to be final: a depth-0
//     glPushMatrix, a glOrtho, or the swap, whichever comes first. All three are safe because
//     the modelview still holds the camera at each - glPushMatrix copies the top of stack
//     without modifying it, and Quake II's R_SetGL2D issues its glOrtho before it touches the
//     modelview. Exactly one glGetFloatv per frame results.
//   - The first world pass of a frame wins. A later glFrustum in the same frame (a viewmodel at
//     a different FOV, a scope) is a sub-pass of the same camera.
//
// Against Quake II's R_SetupGL this reads: arm at glFrustum; the glLoadIdentity, five glRotatef
// and glTranslatef that follow set the pending flag; the first entity glPushMatrix latches the
// complete camera matrix.
//
// Where it breaks, stated plainly:
//   - A frame with no world pass at all (a menu-only frame) leaves the last camera standing,
//     stale. Same behaviour as the captured frustum.
//   - An engine that establishes its 3D view without glFrustum - a hand-built matrix through
//     glLoadMatrixf - captures nothing, and GetCapturedCamera() stays false. Consumers must
//     no-op rather than guess.
//   - An engine that edits the modelview at depth 0 BEFORE its camera sequence and then never
//     pushes would latch the wrong matrix.
//
// Not synchronized; assumes all calls come from the single render thread, same as gl_loader.h
// and projection_capture.h.

// A view matrix in GL's own layout: column-major, sixteen floats, exactly as
// glGetFloatv(GL_MODELVIEW_MATRIX) hands it over. Element (row, col) is m[col * 4 + row].
// Defaults to the identity so a caller that ignores the bool gets something harmless.
struct CameraMatrix {
    float m[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.0f, 0.0f, 0.0f, 1.0f};
};

// The same thing in world space, which is what a human reading a log and a stage reasoning
// about "up" both actually want. R is the view matrix's upper-left 3x3; its ROWS are the
// camera's axes expressed in world coordinates, which is why right/up/forward read off columns
// of the stored array.
// Deliberately zero rather than an identity-looking default: a zero basis is obviously
// uninitialised at a glance in a log, where (0,0,-1) would read as a real direction.
struct CameraPose {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float right[3]    = {0.0f, 0.0f, 0.0f};
    float up[3]       = {0.0f, 0.0f, 0.0f};
    float forward[3]  = {0.0f, 0.0f, 0.0f};
};

// --- Hooks, called from the generated wrapper (see generators/gen_wrapper_cpp.py) before the
// --- corresponding real call is forwarded. Recording only.

// glFrustum: a world pass begins.
void NotifyWorldProjection();

// glOrtho: the 2D/HUD pass begins. Latches the camera if one is pending, then disarms.
void NotifyTwoDProjection();

// glMatrixMode: which stack subsequent matrix calls apply to.
void NotifyMatrixMode(unsigned int mode);

// glPushMatrix / glPopMatrix: tracked so entity transforms can be told from camera setup.
// A push from depth 0 with a camera pending is the primary latch point.
void NotifyMatrixPush();
void NotifyMatrixPop();

// Any of the eleven calls that modify the current matrix: glLoadIdentity, glLoadMatrixf/d,
// glMultMatrixf/d, glRotatef/d, glTranslatef/d, glScalef/d. Deliberately does not take the
// arguments - this module never replays the algebra, it only notes that something changed.
void NotifyMatrixEdited();

// wglSwapBuffers, BEFORE the post-effect chain: last chance to latch, for a frame that drew no
// entities and no HUD.
void FinalizeCameraForFrame();

// wglSwapBuffers, AFTER the post-effect chain: this frame's camera becomes the previous one and
// the per-frame state is cleared. Runs after the chain so a stage reading during it sees a
// stable current/previous pair.
void AdvanceCameraHistory();

// --- Accessors.

// The camera latched for the current frame, or false if no world pass has ever been captured.
bool GetCapturedCamera(CameraMatrix& out);

// The camera latched for the previous frame, or false until two frames have been captured.
// What reprojection needs; nothing consumes it yet.
bool GetPreviousCamera(CameraMatrix& out);

// Turns a view matrix into a world-space position and basis. Pure function, no state.
void DecomposeCamera(const CameraMatrix& in, CameraPose& out);
```

- [ ] **Step 5: Write the minimal implementation**

Create `modelview_capture.cpp`. This task implements only arm / pending / latch-at-swap / history rotation. `NotifyTwoDProjection`, `NotifyMatrixMode`, `NotifyMatrixPush`, `NotifyMatrixPop` and `DecomposeCamera` are defined but left inert, so Task 2's and Task 3's tests can fail honestly.

```cpp
// See modelview_capture.h.
#include <windows.h>
#include <cstdio>

#include "modelview_capture.h"

namespace {

const unsigned int kGlModelviewMatrix = 0x0BA6;

typedef void (__stdcall *PfnGlGetFloatv)(unsigned int pname, float* params);

// Resolved here rather than through gl_loader.h on purpose: that loader gates GL 4.3 compute
// support and its GL 1.1 fallbacks behind one shared flag, so on a machine where the compute
// stages cannot run it hands back nothing at all - and matrix capture has no reason to die with
// them. GetSystemDirectoryA rather than a hardcoded path, matching wrapper.cpp: Windows need
// not be installed on C: or be called "Windows".
PfnGlGetFloatv ResolveGlGetFloatv() {
    static PfnGlGetFloatv fn = nullptr;
    static bool tried = false;
    if (tried) {
        return fn;
    }
    tried = true;

    char sysDir[MAX_PATH];
    UINT sysDirLen = GetSystemDirectoryA(sysDir, sizeof(sysDir));
    if (sysDirLen == 0 || sysDirLen >= sizeof(sysDir)) {
        printf("[opengl32_enh_cpp] modelview: FAILED to get system directory, GetLastError=%lu\n",
               GetLastError());
        return nullptr;
    }
    char dllPath[MAX_PATH + 16];
    snprintf(dllPath, sizeof(dllPath), "%s\\opengl32.dll", sysDir);

    HMODULE real = LoadLibraryA(dllPath);
    if (real == nullptr) {
        printf("[opengl32_enh_cpp] modelview: FAILED to load real opengl32.dll, GetLastError=%lu\n",
               GetLastError());
        return nullptr;
    }
    fn = (PfnGlGetFloatv)GetProcAddress(real, "glGetFloatv");
    if (fn == nullptr) {
        printf("[opengl32_enh_cpp] modelview: FAILED to resolve glGetFloatv, GetLastError=%lu\n",
               GetLastError());
    }
    return fn;
}

CameraMatrix g_current;
CameraMatrix g_previous;
bool g_hasCurrent = false;
bool g_hasPrevious = false;

bool g_armed = false;    // between glFrustum and glOrtho (or the end of the frame)
bool g_pending = false;  // a depth-0 modelview edit happened while armed
bool g_latched = false;  // the camera for this frame has been read
bool g_loggedFirst = false;

// Reads the driver's modelview matrix, if one is pending and none has been taken this frame.
// Idempotent within a frame, which is what lets all three latch points call it unconditionally.
void LatchCamera() {
    if (!g_pending || g_latched) {
        return;
    }
    PfnGlGetFloatv glGetFloatv = ResolveGlGetFloatv();
    if (glGetFloatv == nullptr) {
        return;
    }

    CameraMatrix taken;
    glGetFloatv(kGlModelviewMatrix, taken.m);
    g_current = taken;
    g_hasCurrent = true;
    g_latched = true;
    g_pending = false;

    if (!g_loggedFirst) {
        printf("[opengl32_enh_cpp] modelview: captured first camera matrix, camera-relative "
               "stages can now be placed\n");
        g_loggedFirst = true;
    }
}

}  // namespace

void NotifyWorldProjection() {
    g_armed = true;
}

void NotifyTwoDProjection() {
}

void NotifyMatrixMode(unsigned int mode) {
    (void)mode;
}

void NotifyMatrixPush() {
}

void NotifyMatrixPop() {
}

void NotifyMatrixEdited() {
    if (g_armed && !g_latched) {
        g_pending = true;
    }
}

void FinalizeCameraForFrame() {
    LatchCamera();
}

void AdvanceCameraHistory() {
    if (g_latched) {
        g_previous = g_current;
        g_hasPrevious = true;
    }
    g_armed = false;
    g_pending = false;
    g_latched = false;
}

bool GetCapturedCamera(CameraMatrix& out) {
    if (!g_hasCurrent) {
        return false;
    }
    out = g_current;
    return true;
}

bool GetPreviousCamera(CameraMatrix& out) {
    if (!g_hasPrevious) {
        return false;
    }
    out = g_previous;
    return true;
}

void DecomposeCamera(const CameraMatrix& in, CameraPose& out) {
    (void)in;
    (void)out;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run:
```sh
cmake --build --preset x86 --target modelview_capture_test
cd build-x86 && ctest -R modelview_capture_test --output-on-failure
```
Expected: PASS — three `PASS` lines and `ALL PASS`, exit 0.

If `ctest` reports the test as not found, the `foreach` edit in Step 2 was missed.

- [ ] **Step 7: Commit**

```sh
git add modelview_capture.h modelview_capture.cpp modelview_capture_test.cpp CMakeLists.txt
git commit -m "Add modelview capture module with the happy-path camera latch

Reads the driver's own GL_MODELVIEW_MATRIX at swap time when a glFrustum has
armed a world pass, rather than replaying fixed-function matrix algebra, so the
captured value cannot drift from what the game's GL holds.

The gate that makes the pick correct - the HUD's ortho pass and entity
push/pop - lands next; this commit deliberately captures too eagerly so those
tests can fail.

Co-Authored-By: Claude Opus 5 (1M context) <noreply\@anthropic.com>"
```

---

### Task 2: The gate — exclude the HUD pass and entity transforms

This is the task that makes the capture *correct* rather than merely present. Each case below passes trivially if the gate is ignored, which is why they are written as "must not change" assertions against a matrix captured earlier.

**Files:**
- Modify: `modelview_capture.cpp`
- Modify: `modelview_capture_test.cpp`

**Interfaces:**
- Consumes: everything Task 1 produced.
- Produces: working `NotifyTwoDProjection()`, `NotifyMatrixMode(unsigned int)`, `NotifyMatrixPush()`, `NotifyMatrixPop()`.

- [ ] **Step 1: Write the failing tests**

In `modelview_capture_test.cpp`, add these two helpers to the anonymous namespace, after `SetWorldCamera`:

```cpp
// Quake II's R_SetGL2D: the HUD pass. Note the order - the projection is switched to ortho
// BEFORE the modelview is touched, which is exactly what makes glOrtho a safe latch point.
void BeginHudPass() {
    pMatrixMode(GL_PROJECTION);
    NotifyMatrixMode(GL_PROJECTION);
    pLoadIdentity();
    NotifyMatrixEdited();
    pOrtho(0.0, 256.0, 256.0, 0.0, -99999.0, 99999.0);
    NotifyTwoDProjection();
    pMatrixMode(GL_MODELVIEW);
    NotifyMatrixMode(GL_MODELVIEW);
    pLoadIdentity();
    NotifyMatrixEdited();
}

// R_RotateForEntity, as every Quake II entity path issues it: inside a push/pop pair.
void DrawEntity(float x, float y, float z, float yaw) {
    pPushMatrix();
    NotifyMatrixPush();
    pTranslatef(x, y, z);
    NotifyMatrixEdited();
    pRotatef(yaw, 0.0f, 0.0f, 1.0f);
    NotifyMatrixEdited();
    pPopMatrix();
    NotifyMatrixPop();
}
```

Then, in `main()`, replace the `AdvanceCameraHistory();` line that ends Case 2 with the three cases below (Case 2's own assertions stay as they are):

```cpp
        AdvanceCameraHistory();
    }

    // --- Case 3: the camera is taken AT the first entity push, so a depth-0 modelview edit
    // afterwards - a viewmodel or an alpha pass setting up its own transform - cannot replace
    // it. Asserting only that a balanced push/pop leaves the camera alone would prove nothing:
    // the pop restores it either way, so that passes with no gate at all.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 10.0f, 20.0f, 30.0f);

        float cameraFromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, cameraFromDriver);

        DrawEntity(500.0f, 600.0f, 700.0f, 45.0f);   // the camera is latched here

        pTranslatef(1000.0f, 0.0f, 0.0f);            // depth 0, after the latch
        NotifyMatrixEdited();
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, cameraFromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("camera  ", cameraFromDriver);
        }
        Check(got && MatricesEqual(camera.m, cameraFromDriver),
              "camera is latched at the first entity push, not at swap");

        AdvanceCameraHistory();
    }

    // --- Case 4: the HUD pass must not become the camera. It runs LAST in a real frame, so
    // "the most recent modelview at swap time" is precisely the wrong answer.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 180.0f, 0.0f, -40.0f, 15.0f, 8.0f);

        float cameraFromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, cameraFromDriver);

        BeginHudPass();
        pTranslatef(32.0f, 32.0f, 0.0f);
        NotifyMatrixEdited();
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, cameraFromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("camera  ", cameraFromDriver);
        }
        Check(got && MatricesEqual(camera.m, cameraFromDriver),
              "HUD ortho pass does not overwrite the camera");

        AdvanceCameraHistory();
    }

    // --- Case 5: a push/pop pair BEFORE the camera sequence must not latch early. Nothing is
    // pending at that point, so there is nothing to take, and the camera established afterwards
    // is still the one captured.
    {
        BeginWorldPass();
        pMatrixMode(GL_MODELVIEW);
        NotifyMatrixMode(GL_MODELVIEW);
        DrawEntity(1.0f, 2.0f, 3.0f, 90.0f);

        SetWorldCamera(5.0f, 270.0f, 0.0f, 77.0f, 88.0f, 99.0f);

        float cameraFromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, cameraFromDriver);

        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, cameraFromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("camera  ", cameraFromDriver);
        }
        Check(got && MatricesEqual(camera.m, cameraFromDriver),
              "a push/pop pair before the camera sequence does not latch early");

        AdvanceCameraHistory();
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```sh
cmake --build --preset x86 --target modelview_capture_test
cd build-x86 && ctest -R modelview_capture_test --output-on-failure
```
Expected: FAIL, on Cases 3 and 4.

- Case 3 fails because with no depth-0 push latch, nothing is taken until swap, and the depth-0 `glTranslatef` issued after the entity has moved the modelview by then.
- Case 4 fails because with `NotifyTwoDProjection` inert the pass stays armed, so the HUD's `glLoadIdentity` and `glTranslatef` set `pending` again and swap latches the HUD's matrix.

Case 5 will already pass. That is expected and it is still worth having: it guards against a gate that latches on *any* push rather than only a pending one, which is the obvious way to get this wrong in the other direction.

- [ ] **Step 3: Implement the gate**

In `modelview_capture.cpp`, add to the anonymous namespace, next to `kGlModelviewMatrix`:

```cpp
const unsigned int kGlModelview = 0x1700;
```

and next to the other state variables:

```cpp
// GL's initial matrix mode is GL_MODELVIEW, so that is the honest starting value.
unsigned int g_matrixMode = kGlModelview;
int g_modelviewDepth = 0;
```

Replace the four inert hooks with:

```cpp
void NotifyTwoDProjection() {
    // Safe as a latch point because an engine switches the PROJECTION to ortho before it
    // touches the modelview - the modelview still holds the camera at this instant.
    LatchCamera();
    g_armed = false;
}

void NotifyMatrixMode(unsigned int mode) {
    g_matrixMode = mode;
}

void NotifyMatrixPush() {
    if (g_matrixMode != kGlModelview) {
        return;
    }
    // The primary latch point: the first push away from depth 0 is the engine starting to draw
    // something with its own transform, and a push copies the top of stack without modifying
    // it, so the camera is still there to be read.
    if (g_modelviewDepth == 0) {
        LatchCamera();
    }
    ++g_modelviewDepth;
}

void NotifyMatrixPop() {
    if (g_matrixMode != kGlModelview) {
        return;
    }
    if (g_modelviewDepth > 0) {
        --g_modelviewDepth;
    }
}
```

and replace `NotifyMatrixEdited` with:

```cpp
void NotifyMatrixEdited() {
    // Depth 0 is what separates camera setup from an entity's own transform; see the header.
    if (g_armed && !g_latched && g_matrixMode == kGlModelview && g_modelviewDepth == 0) {
        g_pending = true;
    }
}
```

Finally, add the depth reset to `AdvanceCameraHistory`, so an unbalanced push/pop cannot drift across frames:

```cpp
void AdvanceCameraHistory() {
    if (g_latched) {
        g_previous = g_current;
        g_hasPrevious = true;
    }
    g_armed = false;
    g_pending = false;
    g_latched = false;
    g_modelviewDepth = 0;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run:
```sh
cmake --build --preset x86 --target modelview_capture_test
cd build-x86 && ctest -R modelview_capture_test --output-on-failure
```
Expected: PASS — six `PASS` lines and `ALL PASS`, exit 0.

- [ ] **Step 5: Commit**

```sh
git add modelview_capture.cpp modelview_capture_test.cpp
git commit -m "Gate modelview capture on the world pass and stack depth

Without this the capture takes whatever touched the modelview last, which in an
id Tech 2-era frame is the HUD's ortho pass, and before that every entity's own
transform. glOrtho now disarms the pass and latches, and a modelview edit only
counts at push/pop depth 0 - which is where Quake II establishes the camera and
nowhere an entity transform can reach.

Co-Authored-By: Claude Opus 5 (1M context) <noreply\@anthropic.com>"
```

---

### Task 3: Frame history and world-space decomposition

**Files:**
- Modify: `modelview_capture.cpp`
- Modify: `modelview_capture_test.cpp`

**Interfaces:**
- Consumes: everything Tasks 1 and 2 produced.
- Produces: working `DecomposeCamera(const CameraMatrix&, CameraPose&)`; confirmed `GetPreviousCamera` semantics.

- [ ] **Step 1: Write the failing tests**

In `modelview_capture_test.cpp`, add this helper to the anonymous namespace, after `DrawEntity`:

```cpp
bool NearlyEqual(float a, float b, float tolerance) {
    float d = a - b;
    return (d < 0.0f ? -d : d) <= tolerance;
}

bool Vec3NearlyEqual(const float* v, float x, float y, float z, float tolerance) {
    return NearlyEqual(v[0], x, tolerance) && NearlyEqual(v[1], y, tolerance) &&
           NearlyEqual(v[2], z, tolerance);
}
```

Then add these three cases in `main()`, after Case 5's closing brace:

```cpp
    // --- Case 6: history. During a frame, "previous" must be the LAST frame's camera - which
    // is the whole point of keeping it, and what reprojection will consume.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f);
        float frameOne[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, frameOne);
        FinalizeCameraForFrame();
        AdvanceCameraHistory();

        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 11.0f, 22.0f, 33.0f);
        float frameTwo[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, frameTwo);
        FinalizeCameraForFrame();

        CameraMatrix current, previous;
        bool gotCurrent = GetCapturedCamera(current);
        bool gotPrevious = GetPreviousCamera(previous);
        Check(gotCurrent && MatricesEqual(current.m, frameTwo),
              "GetCapturedCamera() returns this frame's camera");
        Check(gotPrevious && MatricesEqual(previous.m, frameOne),
              "GetPreviousCamera() returns the previous frame's camera, not this one's");

        AdvanceCameraHistory();
    }

    // --- Case 7: a second glFrustum within one frame - a viewmodel at its own FOV, a scope -
    // is a sub-pass of the same camera and must not replace what was already latched.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 4.0f, 5.0f, 6.0f);
        float worldCamera[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, worldCamera);
        DrawEntity(0.0f, 0.0f, 0.0f, 0.0f);   // latches here

        BeginWorldPass();                      // a second frustum, same frame
        SetWorldCamera(0.0f, 90.0f, 0.0f, 900.0f, 900.0f, 900.0f);
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, worldCamera)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("expected", worldCamera);
        }
        Check(got && MatricesEqual(camera.m, worldCamera),
              "a second glFrustum in the same frame does not replace the latched camera");

        AdvanceCameraHistory();
    }

    // --- Case 8: decomposition. The identity view matrix is the unambiguous case, and the
    // Quake II sequence is the real one: whatever rotations precede it, the recovered world
    // position must be the eye the engine translated by.
    {
        CameraMatrix identity;
        CameraPose pose;
        DecomposeCamera(identity, pose);
        Check(Vec3NearlyEqual(pose.position, 0.0f, 0.0f, 0.0f, 1e-4f),
              "DecomposeCamera: identity view matrix is at the world origin");
        Check(Vec3NearlyEqual(pose.right, 1.0f, 0.0f, 0.0f, 1e-4f) &&
              Vec3NearlyEqual(pose.up, 0.0f, 1.0f, 0.0f, 1e-4f) &&
              Vec3NearlyEqual(pose.forward, 0.0f, 0.0f, -1.0f, 1e-4f),
              "DecomposeCamera: identity view matrix looks down -Z with +Y up");

        BeginWorldPass();
        SetWorldCamera(12.0f, 34.0f, 0.0f, 128.0f, -64.0f, 48.0f);
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        DecomposeCamera(camera, pose);
        if (got) {
            printf("  recovered eye = %.3f/%.3f/%.3f (expected 128.000/-64.000/48.000)\n",
                   pose.position[0], pose.position[1], pose.position[2]);
        }
        Check(got && Vec3NearlyEqual(pose.position, 128.0f, -64.0f, 48.0f, 1e-2f),
              "DecomposeCamera: recovers the eye position from a real R_SetupGL matrix");

        AdvanceCameraHistory();
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```sh
cmake --build --preset x86 --target modelview_capture_test
cd build-x86 && ctest -R modelview_capture_test --output-on-failure
```
Expected: FAIL. The three Case 8 assertions fail — `DecomposeCamera` is still inert, so `pose` keeps its defaults and the recovered eye reads `0.000/0.000/0.000`. Cases 6 and 7 should already pass; they pin behaviour Task 1 and Task 2 established.

- [ ] **Step 3: Implement the decomposition**

In `modelview_capture.cpp`, replace the inert `DecomposeCamera` with:

```cpp
void DecomposeCamera(const CameraMatrix& in, CameraPose& out) {
    // Column-major: element (row, col) is m[col * 4 + row]. The view matrix is [R | t] with R
    // the world->view rotation, so R's ROWS are the camera's axes in world coordinates - and a
    // row of R is a stride-4 walk of the stored array.
    const float* m = in.m;

    out.right[0]   =  m[0];  out.right[1]   =  m[4];  out.right[2]   =  m[8];
    out.up[0]      =  m[1];  out.up[1]      =  m[5];  out.up[2]      =  m[9];
    out.forward[0] = -m[2];  out.forward[1] = -m[6];  out.forward[2] = -m[10];

    // The eye in world space is -R^T * t. R is orthonormal for any camera built from rotations
    // and a translation, so the transpose is the inverse and no general inversion is needed -
    // a game that scales its modelview before drawing the world would break that assumption,
    // and no id Tech 2-era engine does.
    const float tx = m[12], ty = m[13], tz = m[14];
    out.position[0] = -(m[0] * tx + m[1] * ty + m[2]  * tz);
    out.position[1] = -(m[4] * tx + m[5] * ty + m[6]  * tz);
    out.position[2] = -(m[8] * tx + m[9] * ty + m[10] * tz);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run:
```sh
cmake --build --preset x86 --target modelview_capture_test
cd build-x86 && ctest -R modelview_capture_test --output-on-failure
```
Expected: PASS — twelve `PASS` lines and `ALL PASS`, exit 0. The printed `recovered eye` line should read `128.000/-64.000/48.000`.

- [ ] **Step 5: Commit**

```sh
git add modelview_capture.cpp modelview_capture_test.cpp
git commit -m "Add camera frame history and world-space decomposition

DecomposeCamera turns the captured view matrix into a world position and basis:
R's rows are the camera's axes, and the eye is -R^T*t, which holds because a
camera built from rotations and a translation has an orthonormal R.

Also pins two behaviours the state machine already had but nothing asserted:
previous is the LAST frame's camera during the current one, and a second
glFrustum inside a frame is a sub-pass that cannot replace what was latched.

Co-Authored-By: Claude Opus 5 (1M context) <noreply\@anthropic.com>"
```

---

### Task 4: Wire the hooks into the generated wrapper

Until this task, nothing calls the module. After it, the DLL captures inside a real game.

**Files:**
- Modify: `generators/gen_wrapper_cpp.py`
- Regenerate: `wrapper.cpp` (and `wrapper.def`, which will be unchanged)
- Modify: `CMakeLists.txt:43`
- Modify: `README.md`

**Interfaces:**
- Consumes: the whole `modelview_capture.h` hook API.
- Produces: a `wrapper.cpp` in which sixteen entry points notify the capture before forwarding.

- [ ] **Step 1: Add the include and the hook set to the generator**

In `generators/gen_wrapper_cpp.py`, find the include block inside `emit_cpp` (the run of `lines.append('#include "..."')` calls) and add after the `projection_capture.h` line:

```python
    lines.append('#include "modelview_capture.h"')
```

Then, above `def emit_cpp(funcs):`, add the set of matrix-modifying entry points:

```python
# Every fixed-function call that modifies the current matrix. modelview_capture.h is told that
# one happened, never what it was - it reads the driver's resulting matrix rather than replaying
# the algebra. See its header comment.
MATRIX_EDIT_FUNCS = {
    "glLoadIdentity",
    "glLoadMatrixf", "glLoadMatrixd",
    "glMultMatrixf", "glMultMatrixd",
    "glRotatef", "glRotated",
    "glTranslatef", "glTranslated",
    "glScalef", "glScaled",
}
```

- [ ] **Step 2: Emit the hook calls**

Still in `generators/gen_wrapper_cpp.py`, inside the `for name, args, ret in funcs:` loop, replace this existing block:

```python
        if name == "wglSwapBuffers":
            lines.append(f"    ApplySelectedEffect({params_call});")
```

with:

```python
        if name == "wglSwapBuffers":
            # See modelview_capture.h: last chance to latch this frame's camera, before any
            # stage could read it, and the history rotation afterwards so a stage reading
            # during the chain sees a stable current/previous pair.
            lines.append("    FinalizeCameraForFrame();")
            lines.append(f"    ApplySelectedEffect({params_call});")
            lines.append("    AdvanceCameraHistory();")
```

Then replace this existing block:

```python
        if name == "glFrustum":
            # See projection_capture.h: records the world projection so depth-consuming stages
            # can unproject raw depth. Recording only - this deliberately falls through to the
            # normal passthrough below rather than calling cache_var itself, since the game's
            # own glFrustum call must still happen exactly as it asked for it.
            lines.append(f"    CaptureProjectionFrustum({params_call});")
```

with:

```python
        if name == "glFrustum":
            # See projection_capture.h: records the world projection so depth-consuming stages
            # can unproject raw depth. Recording only - this deliberately falls through to the
            # normal passthrough below rather than calling cache_var itself, since the game's
            # own glFrustum call must still happen exactly as it asked for it.
            lines.append(f"    CaptureProjectionFrustum({params_call});")
            # See modelview_capture.h: the same call also marks the start of a world pass, which
            # is what tells the camera capture that the modelview it is about to see is the
            # camera's and not the HUD's.
            lines.append("    NotifyWorldProjection();")
        if name == "glOrtho":
            # The other half of that discrimination - the 2D/HUD pass begins here.
            lines.append("    NotifyTwoDProjection();")
        if name == "glMatrixMode":
            lines.append(f"    NotifyMatrixMode({params_call});")
        if name == "glPushMatrix":
            lines.append("    NotifyMatrixPush();")
        if name == "glPopMatrix":
            lines.append("    NotifyMatrixPop();")
        if name in MATRIX_EDIT_FUNCS:
            lines.append("    NotifyMatrixEdited();")
```

All of these are plain `if` (not `elif`) and all fall through to the existing passthrough at the bottom of the loop, exactly as `glFrustum` already does. Do **not** convert them to `elif`, and do **not** place them in the `elif` chain that starts at `glTexImage2D` — that chain exists for hooks which forward the call themselves.

- [ ] **Step 3: Regenerate and inspect**

Run:
```sh
cd generators && python gen_wrapper_cpp.py && cd ..
git diff --stat wrapper.cpp wrapper.def
```
Expected: `wrapper.cpp` changed, `wrapper.def` unchanged (no new exports — all sixteen entry points were already forwarded).

Verify the hooks landed, and that the passthrough survived:
```sh
grep -c "NotifyMatrixEdited();" wrapper.cpp
grep -A3 "glPushMatrix(void)" wrapper.cpp
```
Expected: `11` for the first. The second must show `NotifyMatrixPush();` followed by the existing `__proc_glPushMatrix();` call — if the passthrough is gone, the game's matrix stack breaks.

- [ ] **Step 4: Add the module to the DLL and build everything**

In `CMakeLists.txt` line 43, add `modelview_capture.cpp` to the `add_library(${WRAPPER_NAME_CXX} SHARED ...)` source list, immediately after `projection_capture.cpp`.

Run:
```sh
cmake --build --preset x86
cd build-x86 && ctest --output-on-failure
```
Expected: the whole suite passes, including `opengl32_enh32_test` — which loads the proxy DLL and calls through it with no current GL context, so it exercises the new hooks in exactly the state where `glGetFloatv` resolution could crash. That is the point of running it here.

- [ ] **Step 5: Document the widened interception surface**

In `README.md`, in the `## How it works` section, replace step 4:

```markdown
4. `glTexImage2D` is intercepted separately for the optional
   [texture effects](#texture-effects), and `glTexParameteri`/`glTexParameterf`
   for [anisotropic filtering](#anisotropic-filtering).
```

with:

```markdown
4. `glTexImage2D` is intercepted separately for the optional
   [texture effects](#texture-effects), and `glTexParameteri`/`glTexParameterf`
   for [anisotropic filtering](#anisotropic-filtering).
5. The matrix calls — `glFrustum`, `glOrtho`, `glMatrixMode`,
   `glPushMatrix`/`glPopMatrix` and the calls that modify the current matrix —
   are watched without being changed, to work out where the camera is and which
   way it points. Depth-based stages need that to relate a pixel to a point in
   the world. Nothing uses it yet; it is groundwork.
```

- [ ] **Step 6: Commit**

```sh
git add generators/gen_wrapper_cpp.py wrapper.cpp CMakeLists.txt README.md
git commit -m "Hook the matrix entry points into the generated wrapper

Sixteen entry points now notify modelview_capture.h before forwarding, so the
camera is captured inside a real game. All sixteen were already exported and
passed through, so wrapper.def is unchanged and no call behaves differently.

Co-Authored-By: Claude Opus 5 (1M context) <noreply\@anthropic.com>"
```

---

### Task 5: The in-game verification log

The unit tests prove the capture behaves as designed. They cannot prove it picked the *right* matrix in Anachronox, because that depends on what the engine actually does. This task is what makes that checkable.

**Files:**
- Modify: `config.h`
- Modify: `config.cpp`
- Modify: `modelview_capture.cpp`
- Modify: `opengl32_enhancer.ini`
- Modify: `README.md`
- Modify: `docs/enhancement-opportunities.md`

**Interfaces:**
- Consumes: `GetAnaxConfig()` from `config.h`, `RedirectStdoutToDebugLog()` from `debug_log.h`, `DecomposeCamera` from `modelview_capture.h`.
- Produces: `cameraLogInterval` on `AnaxConfig`.

- [ ] **Step 1: Add the config key**

In `config.h`, add immediately after the `frameDumpPath` member:

```cpp
    // Prints the camera position and orientation that modelview_capture.h reconstructs to the
    // debug log every N frames. 0 (default) is off. This exists because the capture's heuristic
    // can only be CONFIRMED inside a real game - the unit tests show it behaves as designed,
    // not that the engine does what the design assumes. Independent of the effect= pipeline,
    // and of the capture itself, which always runs and costs one glGetFloatv per frame.
    int cameraLogInterval = 0;
```

In `config.cpp`, add a branch immediately after the `frameDumpPath` branch:

```cpp
        } else if (strcmp(key, "cameraLogInterval") == 0) {
            config.cameraLogInterval = ParseClampedInt(value, 0, 100000, config.cameraLogInterval,
                                                       "cameraLogInterval");
```

In the same file's summary `printf`, add `"cameraLogInterval=%d, "` to the format string immediately after the `frameDumpKey=0x%02X, frameDumpPath='%s', ` fragment, and `config.cameraLogInterval,` to the argument list immediately after `config.frameDumpPath,`.

- [ ] **Step 2: Verify the config round-trips**

Run:
```sh
cmake --build --preset x86 --target config_test
cd build-x86 && ctest -R "config_test|config_writer_test" --output-on-failure
```
Expected: PASS. `config_writer_test` matters here — it confirms that a key absent from the writer's managed-key list survives a save untouched, which is why `cameraLogInterval` deliberately is not added to that list.

- [ ] **Step 3: Emit the log**

In `modelview_capture.cpp`, add to the includes:

```cpp
#include "config.h"
#include "debug_log.h"
```

Add to the anonymous namespace's state:

```cpp
unsigned int g_frameCounter = 0;
```

Add this function to the anonymous namespace, after `LatchCamera`:

```cpp
// The in-game half of this module's verification: the unit tests show the state machine behaves
// as designed, but only a real game can show the design matched the engine. Walk a straight
// line with this on and the logged position must track; turn on the spot and the basis must
// swing while the position holds.
void LogCameraIfDue() {
    const int interval = GetAnaxConfig().cameraLogInterval;
    if (interval <= 0 || !g_hasCurrent) {
        return;
    }
    if (g_frameCounter % (unsigned int)interval != 0) {
        return;
    }
    RedirectStdoutToDebugLog();

    CameraPose pose;
    DecomposeCamera(g_current, pose);
    printf("[opengl32_enh_cpp] modelview: frame %u camera at %.1f/%.1f/%.1f, "
           "forward %.3f/%.3f/%.3f, up %.3f/%.3f/%.3f\n",
           g_frameCounter,
           pose.position[0], pose.position[1], pose.position[2],
           pose.forward[0], pose.forward[1], pose.forward[2],
           pose.up[0], pose.up[1], pose.up[2]);
}
```

Call it from `AdvanceCameraHistory`, which is the one hook that runs exactly once per frame, and advance the counter there too:

```cpp
void AdvanceCameraHistory() {
    LogCameraIfDue();
    ++g_frameCounter;

    if (g_latched) {
        g_previous = g_current;
        g_hasPrevious = true;
    }
    g_armed = false;
    g_pending = false;
    g_latched = false;
    g_modelviewDepth = 0;
}
```

Also add the `RedirectStdoutToDebugLog();` call to the first-capture message in `LatchCamera`, immediately before its `printf`, so that line reaches the log file too.

- [ ] **Step 4: Run the whole suite**

Run:
```sh
cmake --build --preset x86
cd build-x86 && ctest --output-on-failure
```
Expected: all tests pass. `modelview_capture_test` still prints twelve `PASS` lines and no camera-log lines, because `cameraLogInterval` defaults to 0 and the test directory has no ini.

- [ ] **Step 5: Document the key**

In `opengl32_enhancer.ini`, add after the `frameDumpPath=` line:

```ini

; cameraLogInterval: prints the camera position and orientation this DLL reconstructs from the
; game's matrix calls to opengl32_enhancer.log, every N frames. 0 (the default) is off.
;
; This is a diagnostic, not an effect - nothing renders differently with it on. It exists
; because working out which matrix is the camera's is a heuristic about how the engine draws,
; and the only way to confirm it holds for YOUR game is to watch the numbers while you move.
; Walk a straight line: the position should slide steadily. Turn on the spot: the forward and
; up vectors should swing while the position stays put. If the numbers jump around while you
; stand still, the capture is picking up something that is not the camera - say so in a bug
; report, with the game's name.
;
; 60 is about once a second; lower values write a lot of lines.
cameraLogInterval=0
```

In `README.md`, add to the `## Troubleshooting` section, after the "Effects are disabled with a message about GL 4.3" entry:

```markdown
**Depth-based stages (`ssao`, `dof`, `fog`, `ssr`) look wrong in one particular
game.** These need to know where the camera is, which this proxy works out by
watching the game's matrix calls — a heuristic about how the engine draws, not
something it is told. Set `cameraLogInterval=60` in the ini, play for a few
seconds, and read `opengl32_enhancer.log`: the logged position should slide as
you walk and hold still as you turn. If it jumps around while you stand still,
the capture is picking up something that is not the camera.
```

- [ ] **Step 6: Record the outcome in the enhancement doc**

In `docs/enhancement-opportunities.md`, replace the `## Tier 2 — capture the modelview matrix (recommended first)` heading line with:

```markdown
## Tier 2 — capture the modelview matrix

**Done** — `modelview_capture.h`, shipped 2026-09-19. Design:
`docs/superpowers/specs/2026-09-19-modelview-capture-design.md`.

The capture is recording-only and nothing consumes it yet: SSR still measures
its `up` gate in view space, TAA still uses its raw-difference heuristic. Those
rewires are the next increment, deliberately separated so a capture bug shows
up as a wrong number in a log rather than as a stage regression.

The predicted hazard was real and is handled rather than dodged. `glFrustum`
arms a world pass and `glOrtho` disarms it — the same discrimination
`projection_capture.h` already made — and a second axis was needed on top:
modelview edits count only at push/pop depth 0, because a Quake II frame
overwrites the modelview once per entity. The matrix is read back from the
driver with `glGetFloatv` rather than recomputed, so the captured value cannot
drift from what the game's GL holds; the whole module decides *when*, never
*what*.

What remains a guess is the "when". It is validated against Quake II's
`R_SetupGL` and against Anachronox by eye (`cameraLogInterval`), and an engine
that builds its view matrix without `glFrustum`, or edits the modelview at depth
0 before its camera sequence, would defeat it. `GetCapturedCamera()` returns
false in the first case rather than guessing; the second fails silently, and is
the reason `cameraLogInterval` exists.
```

Then, in the `## Suggested order` section, replace item 2:

```markdown
2. **Modelview capture** (Tier 2) — unlocks the most, changes nothing on its own.
```

with:

```markdown
2. ~~**Modelview capture** (Tier 2).~~ Done — see above. The consumers it
   unlocks (SSR's world-space `up`, real TAA, motion blur, temporally
   accumulated SSAO) are now each a separate, smaller piece of work.
```

- [ ] **Step 7: Commit**

```sh
git add config.h config.cpp modelview_capture.cpp opengl32_enhancer.ini README.md docs/enhancement-opportunities.md
git commit -m "Add cameraLogInterval, the in-game check for the camera capture

The unit tests show the capture behaves as designed; they cannot show the design
matched the engine, because which matrix is the camera's is a heuristic about
how a game draws. This logs the reconstructed position and basis every N frames
so that can be confirmed by walking around. Off by default.

Deliberately not added to config_writer.cpp's managed-key list: that list is the
keys the editor can change, windowWidth and frameDumpKey are absent for the same
reason, and unmanaged lines are copied through untouched on save.

Co-Authored-By: Claude Opus 5 (1M context) <noreply\@anthropic.com>"
```

---

## Acceptance

Automated, and the last thing to run:

```sh
cmake --build --preset x86
cd build-x86 && ctest --output-on-failure
```

Manual, and the only check that can confirm the heuristic picked the right
matrix — it needs a human and a real game:

1. Build the release DLL: `cmake --preset x86-release && cmake --build --preset x86-release --target opengl32_enh32`.
2. Install `build-x86-release/opengl32_enh32.dll` as `opengl32.dll` in the Anachronox folder, alongside an ini containing `cameraLogInterval=60`.
3. Play for roughly a minute: walk a straight line, stop, turn a full circle, look up and down.
4. Read `opengl32_enhancer.log` in the game folder. Expected: `captured first camera` appears once; the per-frame lines show the position sliding steadily while walking and holding still while turning, and the `forward` vector swinging while turning and holding while walking straight.

A position that jitters while standing still, or that never changes, means the
latch is taking something other than the camera — report which, with the log.
