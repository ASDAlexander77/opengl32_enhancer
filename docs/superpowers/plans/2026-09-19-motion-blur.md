# Camera Motion Blur Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `motionblur` post-effect stage that smears the frame along the camera's own movement, using per-pixel velocity reconstructed by reprojecting depth through the previous frame's camera, without smearing the HUD.

**Architecture:** A new `world_capture` module latches a copy of the back buffer at the first `glOrtho` of each frame — before the HUD is drawn — on the same `glFrustum`-arms/`glOrtho`-fires discrimination `modelview_capture.h` and `projection_capture.h` already use. The stage unprojects hardware depth to a view-space position (the same math `ssao.cpp`/`ssr.cpp` use), multiplies by a per-frame matrix `M = V_prev · V_current⁻¹`, reprojects, and blurs along the screen-space difference. The difference between the pristine capture and the world-only capture is the HUD mask.

**Tech Stack:** C++ (MSVC x86), OpenGL 4.3 compute shaders, std140 UBOs, CMake + Ninja, CTest.

**Spec:** `docs/superpowers/specs/2026-09-19-motion-blur-design.md`

## Global Constraints

- **Never include `<windows.h>` in anything linked into the proxy DLL.** It declares `dllimport` `wgl*`/`gl*` names that collide with `wrapper.cpp`'s `dllexport` definitions. Declare the handful of Win32 functions you need with `extern "C" __declspec(dllimport)`, as `modelview_capture.cpp` and `post_effects.cpp` do. Standalone `*_test.cpp` executables may include it freely.
- **No stage header includes `config.h`.** Stages take plain values, not the config struct. This is why `ApplyMotionBlur` takes a finished matrix rather than two cameras — see `ApplySsr`'s `viewSpaceWorldUp` parameter for the precedent.
- **A stage that does nothing must leave the image bit-exact**, not nearly unchanged, so that a stage which found nothing is indistinguishable from one that never ran.
- **Every `Apply*` returns `true` only if it actually wrote `dstTexture`.** The caller only flips its ping-pong when it returns true.
- **Cached GL objects are keyed on `GetGlContextGeneration()`.** If the generation changed, drop the handles (do not `glDelete*` them — the owning context already freed them) and rebuild.
- **The HUD-mask epsilon is `1.0/128.0`.** Both compared textures are RGBA16F copies of the same 8-bit back buffer, so the only noise floor is 8-bit quantisation at 1/255.
- **Sample count is fixed at 8** in the shader. Do not add a config key for it.
- **Config defaults:** `motionBlurStrength = 0.5f` (0..1, 0 is an exact no-op), `motionBlurMaxRadius = 0.05f` (0..0.5).
- **`motionblur` must NOT be added to the shipped `effect=` line** in `opengl32_enhancer.ini`. It goes in a commented preset only.
- **Mutation-check every test.** Break the code a test claims to cover and prove the test fails. A passing test proves nothing on its own. This is a standing user preference, not a suggestion.
- Build: `cmake --build --preset x86-release`. Test: `cd build-x86-release && ctest --output-on-failure`. All 33 existing tests must stay green.

---

## File Structure

**Create:**

| File | Responsibility |
|---|---|
| `world_capture.h` / `.cpp` | Latch a world-only copy of the back buffer at the first `glOrtho` of a frame. Knows *when*, and owns one texture. Nothing about blurring. |
| `world_capture_test.cpp` | Arm/latch/staleness discipline against a real GL context. |
| `motion_blur.h` / `.cpp` | The pure reprojection helper and the compute stage. |
| `motion_blur_test.cpp` | CPU assertions on the helper; GPU assertions on the stage. |

**Modify:** `generators/gen_wrapper_cpp.py`, `post_effects.cpp`, `post_effects.h`, `post_effects_test.cpp`, `config.h`, `config.cpp`, `config_writer.cpp`, `config_editor.cpp`, `CMakeLists.txt`, `opengl32_enhancer.ini`, `README.md`, `docs/enhancement-opportunities.md`.

---

### Task 1: `world_capture` — the pre-HUD frame latch

**Files:**
- Create: `world_capture.h`, `world_capture.cpp`, `world_capture_test.cpp`
- Modify: `generators/gen_wrapper_cpp.py` (hook emission), `CMakeLists.txt`

**Interfaces:**
- Consumes: `GetGlComputeApi()` and `GetGlContextGeneration()` from `gl_loader.h`; `GetAnaxConfig().cameraLogInterval` from `config.h`.
- Produces:
  ```cpp
  void NotifyWorldPassBegan();   // called from glFrustum
  void NotifyTwoDPassBegan();    // called from glOrtho
  void InvalidateWorldFrame();   // called at swap, AFTER the effect chain
  bool GetWorldOnlyFrame(unsigned int& texture, int& width, int& height);
  ```

- [ ] **Step 1: Write `world_capture.h`**

```cpp
#pragma once

// Captures a copy of the frame BEFORE the game draws its HUD, so a stage can tell an overlay
// pixel from a world pixel. Depth cannot do this: Quake II's R_SetGL2D disables depth testing
// to draw the HUD, and depth writes require depth testing, so HUD pixels never write depth -
// they keep whatever the world wrote behind them.
//
// WHEN, not what. `glFrustum` arms a world pass and the first `glOrtho` of the frame fires the
// latch, which is the same discrimination modelview_capture.h and projection_capture.h already
// make at the same two hook points. At the first `glOrtho` the world is finished and the 2D
// pass has not started, so the back buffer at that instant is exactly the world.
//
// Unlike the post-effect chain, this runs INSIDE the game's own GL state, so it saves and
// restores the active texture unit, the 2D texture binding, the read buffer and the read
// framebuffer binding around its one copy. See texture_effect.cpp, which does the same at
// glTexImage2D.
//
// Where it breaks, and how it degrades:
//   - 3D drawn AFTER the first glOrtho (a rendered portrait inside a dialogue box, an inset
//     viewport) is absent from the capture, so those pixels differ from the finished frame and
//     read as HUD to a consumer: they simply do not get the effect. Never corruption.
//   - A frame that issues no glOrtho leaves no capture, and GetWorldOnlyFrame() returns false.
//   - A game that draws its HUD without glOrtho never captures at all. False, not a guess.

// glFrustum: a world pass is starting.
void NotifyWorldPassBegan();

// glOrtho: the 2D pass is starting, so the world is complete. Latches on the first call of a
// frame that was armed; later calls in the same frame do nothing.
void NotifyTwoDPassBegan();

// Called at swap AFTER the effect chain has run - the chain is the consumer, so invalidating
// on entry would destroy the very frame it needs. Mirrors AdvanceCameraHistory() in
// modelview_capture.h, which is called at the same point for the same reason.
void InvalidateWorldFrame();

// The world-only frame for THIS frame, or false if none was latched. Never returns a stale
// texture: a frame with no world pass fails closed rather than handing back the previous
// frame's world, because being wrong here would be invisible.
//
// `width`/`height` are the viewport at latch time. A consumer whose own dimensions differ must
// decline rather than sample a mismatched texture.
bool GetWorldOnlyFrame(unsigned int& texture, int& width, int& height);
```

- [ ] **Step 2: Write the failing test `world_capture_test.cpp`**

Model the window/context boilerplate on `modelview_capture_test.cpp` verbatim (same `WNDCLASSA` / `ChoosePixelFormat` / `wglCreateContext` sequence). Then the cases:

```cpp
// Case 1: nothing captured before any call. MUST RUN FIRST.
{
    unsigned int tex = 123; int w = 0, h = 0;
    ok = Check(!GetWorldOnlyFrame(tex, w, h),
               "no world frame before any pass has begun") && ok;
}

// Case 2: glFrustum then glOrtho latches.
{
    NotifyWorldPassBegan();
    NotifyTwoDPassBegan();
    unsigned int tex = 0; int w = 0, h = 0;
    ok = Check(GetWorldOnlyFrame(tex, w, h) && tex != 0 && w == kWidth && h == kHeight,
               "glFrustum then glOrtho latches a world frame at viewport size") && ok;
}

// Case 3: invalidation at swap makes it stale-free.
{
    InvalidateWorldFrame();
    unsigned int tex = 0; int w = 0, h = 0;
    ok = Check(!GetWorldOnlyFrame(tex, w, h),
               "the world frame does not survive into the next frame") && ok;
}

// Case 4: glOrtho with no preceding glFrustum does not latch. This is the case that stops a
// menu-only frame - which never calls glFrustum - from being treated as a world pass.
{
    NotifyTwoDPassBegan();
    unsigned int tex = 0; int w = 0, h = 0;
    ok = Check(!GetWorldOnlyFrame(tex, w, h),
               "glOrtho alone does not latch - the arm is load-bearing") && ok;
    InvalidateWorldFrame();
}

// Case 5: a second glOrtho in the same frame does not re-latch. A Quake II frame issues
// several ortho passes; only the first one happens before the HUD.
{
    NotifyWorldPassBegan();
    NotifyTwoDPassBegan();
    unsigned int firstTex = 0; int w = 0, h = 0;
    GetWorldOnlyFrame(firstTex, w, h);

    // Change the back buffer, then fire a second glOrtho. If it re-latched, the texture's
    // contents would follow - so assert against the PIXELS, not just the handle, since the
    // module legitimately reuses one texture object across frames.
    ClearBackBufferTo(0.0f, 1.0f, 0.0f);   // green; the first latch captured red
    NotifyTwoDPassBegan();

    unsigned char px[4] = {0, 0, 0, 0};
    ReadCenterPixelOf(firstTex, px);
    ok = Check(px[0] > 200 && px[1] < 60,
               "a second glOrtho in the same frame does not re-latch") && ok;
    InvalidateWorldFrame();
}
```

`ClearBackBufferTo` and `ReadCenterPixelOf` are local helpers: the first is `glClearColor`/`glClear` resolved from the real `opengl32.dll` the way `nis_effect_test.cpp` does it; the second attaches the texture to an FBO and calls `glReadPixels` with `GL_RGBA`/`GL_UNSIGNED_BYTE`, the way `nis_effect_test.cpp`'s `RunOnce` does.

Before Case 2, clear the back buffer to red (`1.0f, 0.0f, 0.0f`) so Case 5's pixel assertion has something to distinguish.

- [ ] **Step 3: Add the test target to `CMakeLists.txt`**

Next to the `ssr_test` block:

```cmake
# Creates a real GL context and drives the glFrustum/glOrtho sequence a Quake II-family engine
# makes, checking that the world-only frame latches exactly once per frame and never survives
# into the next one - see world_capture_test.cpp. Links config.cpp and debug_log.cpp because
# the cameraLogInterval diagnostic reads the config, same as modelview_capture_test.
add_executable(world_capture_test world_capture_test.cpp world_capture.cpp gl_loader.cpp
    config.cpp debug_log.cpp)
target_link_libraries(world_capture_test opengl32 gdi32 user32)
```

Add `world_capture_test` to the `foreach(test_target ...)` list at the bottom so it gets the `gpu` label.

- [ ] **Step 4: Run the test to verify it fails**

```
cmake --build --preset x86-release --target world_capture_test
cd build-x86-release && ./world_capture_test.exe
```

Expected: a link error for the four unresolved `world_capture` symbols. That is the correct failure — the feature does not exist.

- [ ] **Step 5: Write `world_capture.cpp`**

```cpp
// See world_capture.h.
#include <cstdio>

#include "world_capture.h"
#include "config.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D              = 0x0DE1;
const unsigned int GL_TEXTURE0                = 0x84C0;
const unsigned int GL_ACTIVE_TEXTURE          = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D      = 0x8069;
const unsigned int GL_TEXTURE_MIN_FILTER      = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER      = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S          = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T          = 0x2803;
const unsigned int GL_LINEAR                  = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE           = 0x812F;
const unsigned int GL_RGBA16F                 = 0x881A;
const unsigned int GL_BACK                    = 0x0405;
const unsigned int GL_READ_BUFFER             = 0x0C02;
const unsigned int GL_READ_FRAMEBUFFER        = 0x8CA8;
const unsigned int GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const unsigned int GL_VIEWPORT                = 0x0BA2;
const unsigned int GL_NO_ERROR                = 0;

bool g_armed = false;
bool g_latched = false;
unsigned int g_texture = 0;
int g_width = 0;
int g_height = 0;
unsigned int g_generation = 0;
unsigned int g_frameCounter = 0;

// RGBA16F on purpose, matching the pipeline's own textures: the HUD mask compares this against
// post_effects.cpp's pristine capture, and if the two were stored at different precisions an
// identical pixel could differ by a quantisation step and read as HUD.
void EnsureTexture(const GlComputeApi& gl, int width, int height) {
    if (g_texture != 0 && g_width == width && g_height == height) {
        return;
    }
    if (g_texture != 0) {
        gl.glDeleteTextures(1, &g_texture);
        g_texture = 0;
    }
    gl.glGenTextures(1, &g_texture);
    gl.glBindTexture(GL_TEXTURE_2D, g_texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    g_width = width;
    g_height = height;
}

void LatchWorldFrame() {
    if (!g_armed || g_latched) {
        return;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        // Unlike the camera capture, this exists solely to feed a compute stage. If compute is
        // unavailable the consumer cannot run either, so there is nothing to capture for.
        return;
    }

    if (g_generation != GetGlContextGeneration()) {
        // The old texture belonged to a context that is gone; drop the handle rather than
        // deleting it, which would now hit an unrelated object.
        g_texture = 0;
        g_width = 0;
        g_height = 0;
        g_generation = GetGlContextGeneration();
    }

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) {
        return;
    }

    // Running inside the game's own GL state, so everything touched is put back.
    int savedActiveTexture = 0;
    gl.glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedBinding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedBinding);
    int savedReadBuffer = 0;
    gl.glGetIntegerv(GL_READ_BUFFER, &savedReadBuffer);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);

    EnsureTexture(gl, viewport[2], viewport[3]);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, g_texture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_width, g_height);

    unsigned int err = gl.glGetError();

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
    gl.glReadBuffer((unsigned int)savedReadBuffer);
    gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedBinding);
    gl.glActiveTexture((unsigned int)savedActiveTexture);

    if (err != GL_NO_ERROR) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] world_capture: glGetError() = 0x%04X after copy, "
                   "world-only frame unavailable\n", err);
            warned = true;
        }
        return;
    }

    g_latched = true;
}

// Reuses cameraLogInterval rather than adding a knob: the thing being validated is the same
// class of claim the camera latch was - a guess about WHEN - so it is validated the same way
// and in the same log.
void LogWorldCaptureIfDue() {
    int interval = GetAnaxConfig().cameraLogInterval;
    if (interval <= 0 || (g_frameCounter % (unsigned int)interval) != 0) {
        return;
    }
    printf("[opengl32_enh_cpp] world_capture: frame %u latched=%s size=%dx%d\n",
           g_frameCounter, g_latched ? "yes" : "no", g_width, g_height);
}

}  // namespace

void NotifyWorldPassBegan() {
    g_armed = true;
}

void NotifyTwoDPassBegan() {
    LatchWorldFrame();
    g_armed = false;
}

void InvalidateWorldFrame() {
    LogWorldCaptureIfDue();
    ++g_frameCounter;
    g_armed = false;
    g_latched = false;
}

bool GetWorldOnlyFrame(unsigned int& texture, int& width, int& height) {
    if (!g_latched || g_texture == 0) {
        return false;
    }
    texture = g_texture;
    width = g_width;
    height = g_height;
    return true;
}
```

- [ ] **Step 6: Run the test to verify it passes**

```
cmake --build --preset x86-release --target world_capture_test
cd build-x86-release && ./world_capture_test.exe
```

Expected: all cases PASS, exit 0.

- [ ] **Step 7: Mutation-check every case**

Apply each mutation, rebuild, confirm the named case fails, then revert. A mutation that does not fail any case means that case is decorative and must be rewritten.

| Mutation | Must fail |
|---|---|
| `NotifyWorldPassBegan` body → `{}` (never arms) | Case 2 |
| `LatchWorldFrame`'s `if (!g_armed ...)` → `if (g_latched)` | Case 4 |
| Remove `g_latched = true;` at the end of `LatchWorldFrame` | Case 2 **and** Case 5 |
| `InvalidateWorldFrame` drops `g_latched = false;` | Case 3 |

- [ ] **Step 8: Emit the wrapper hooks**

In `generators/gen_wrapper_cpp.py`, extend the existing `glFrustum` and `glOrtho` blocks (around lines 284–296) — do **not** add new `if` chains, and do **not** use `elif`; every hook falls through to the passthrough:

```python
        if name == "glFrustum":
            lines.append(f"    CaptureProjectionFrustum({params_call});")
            lines.append("    NotifyWorldProjection();")
            # See world_capture.h: the same arm also tells the world-only frame capture that a
            # world pass has begun, so the first glOrtho after it is the pre-HUD instant.
            lines.append("    NotifyWorldPassBegan();")
        if name == "glOrtho":
            lines.append("    NotifyTwoDProjection();")
            lines.append("    NotifyTwoDPassBegan();")
```

Add `#include "world_capture.h"` to the generated file's include block alongside `modelview_capture.h`.

In the `wglSwapBuffers` block, add `InvalidateWorldFrame();` immediately **after** `AdvanceCameraHistory()`, so the order is: `FinalizeCameraForFrame()` → `ApplySelectedEffect(...)` → `AdvanceCameraHistory()` → `InvalidateWorldFrame()`.

- [ ] **Step 9: Add `world_capture.cpp` to the DLL target**

In `CMakeLists.txt`, add `world_capture.cpp` to the `add_library` source list that already contains `modelview_capture.cpp`. Verify with `grep -c world_capture CMakeLists.txt` that it appears exactly three times (library, test target, test list) — a previous edit in this repo accidentally duplicated a filename on that line.

- [ ] **Step 10: Run the full suite**

```
cmake --build --preset x86-release
cd build-x86-release && ctest --output-on-failure
```

Expected: 34/34 (the 33 existing plus `world_capture_test`).

- [ ] **Step 11: Commit**

```bash
git add world_capture.h world_capture.cpp world_capture_test.cpp \
        generators/gen_wrapper_cpp.py CMakeLists.txt
git commit -m "Capture the frame before the HUD is drawn"
```

---

### Task 2: `MotionBlurReprojection` — the pure helper

**Files:**
- Create: `motion_blur.h`, `motion_blur.cpp`, `motion_blur_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `CameraMatrix` from `modelview_capture.h`.
- Produces: `void MotionBlurReprojection(const CameraMatrix& current, const CameraMatrix& previous, float out[16]);` — column-major 4×4, `out[col * 4 + row]`.

This task delivers only the pure function. `ApplyMotionBlur` arrives in Task 3.

- [ ] **Step 1: Write `motion_blur.h` (helper declaration only for now)**

```cpp
#pragma once

#include "modelview_capture.h"
#include "projection_capture.h"

// Camera motion blur: smears the frame along the camera's own movement between the previous
// frame and this one. One stage in the shared post-effect pipeline (see post_effects.cpp).
//
// CAMERA motion only, permanently. A character walking across a stationary frame does not
// smear: the proxy sees fixed-function geometry, not per-object transforms, so an object's
// velocity does not exist anywhere in the interception surface to be read. This is a property
// of what can be intercepted, not a feature deferred to later.

// Builds the matrix that carries a view-space position from THIS frame's camera into the
// PREVIOUS frame's camera: M = previous * inverse(current). Pure - no GL calls, no state.
//
// A view matrix is rigid, so the inverse is Rt-transpose and -Rt-transpose * t; no general 4x4
// inversion is needed or wanted. `out` is column-major like CameraMatrix itself, so element
// (row, col) is out[col * 4 + row].
void MotionBlurReprojection(const CameraMatrix& current, const CameraMatrix& previous,
                            float out[16]);
```

- [ ] **Step 2: Write the failing test `motion_blur_test.cpp`**

No GL context needed for these four cases, so they run first in `main()` before any window is created.

```cpp
bool NearlyEqual(float a, float b) { return fabsf(a - b) < 1e-4f; }

bool CheckReprojection() {
    bool ok = true;

    // Identity in, identity out. Weak on its own - an identity matrix is symmetric, so a
    // transposed inverse passes this case unchanged. That is exactly the trap the modelview
    // capture work hit, which is why the rotation case below is the load-bearing one.
    {
        CameraMatrix current;   // defaults to identity
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        bool isIdentity = true;
        for (int i = 0; i < 16; ++i) {
            float expected = (i % 5 == 0) ? 1.0f : 0.0f;
            if (!NearlyEqual(m[i], expected)) { isIdentity = false; }
        }
        ok = Check(isIdentity, "identical cameras reproject to the identity matrix") && ok;
    }

    // Pure translation. current translates the world by -d along view Z (the camera moved
    // forward by d); previous is identity. M = inverse(current), so it translates back by +d.
    {
        CameraMatrix current;
        current.m[14] = -40.0f;
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        ok = Check(NearlyEqual(m[14], 40.0f) && NearlyEqual(m[12], 0.0f) &&
                   NearlyEqual(m[13], 0.0f),
                   "a pure forward translation inverts to an equal backward one") && ok;
    }

    // Pure rotation - the case that catches a transposed inverse. current is +90 degrees about
    // Z (column-major: m[0]=cos, m[1]=sin, m[4]=-sin, m[5]=cos), previous is identity, so M is
    // -90 degrees about Z. Using R instead of R-transpose flips the sign of both off-diagonal
    // terms, which this catches and the identity case above cannot.
    {
        CameraMatrix current;
        current.m[0] = 0.0f;  current.m[1] = 1.0f;
        current.m[4] = -1.0f; current.m[5] = 0.0f;
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        ok = Check(NearlyEqual(m[0], 0.0f) && NearlyEqual(m[1], -1.0f) &&
                   NearlyEqual(m[4], 1.0f) && NearlyEqual(m[5], 0.0f),
                   "a +90 degree rotation inverts to -90 degrees, transpose included") && ok;
    }

    // Rotation AND translation together. The inverse translation is -R-transpose * t, so it
    // must be rotated too - a version that negated t without rotating it passes both cases
    // above and fails this one.
    {
        CameraMatrix current;
        current.m[0] = 0.0f;  current.m[1] = 1.0f;
        current.m[4] = -1.0f; current.m[5] = 0.0f;
        current.m[12] = 10.0f; current.m[13] = 0.0f; current.m[14] = 0.0f;
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        // -R-transpose * (10,0,0) with R = Rz(90): R-transpose * (10,0,0) = (0,-10,0), negated
        // gives (0,10,0).
        ok = Check(NearlyEqual(m[12], 0.0f) && NearlyEqual(m[13], 10.0f) &&
                   NearlyEqual(m[14], 0.0f),
                   "the inverse translation is rotated, not merely negated") && ok;
    }

    return ok;
}
```

- [ ] **Step 3: Add the test target to `CMakeLists.txt`**

```cmake
# Checks the pure reprojection matrix on the CPU and the blur stage against a real GL context -
# see motion_blur_test.cpp.
add_executable(motion_blur_test motion_blur_test.cpp motion_blur.cpp gl_loader.cpp
    projection_capture.cpp modelview_capture.cpp world_capture.cpp config.cpp debug_log.cpp)
target_link_libraries(motion_blur_test opengl32 gdi32 user32)
```

Add `motion_blur_test` to the `foreach(test_target ...)` list.

- [ ] **Step 4: Run the test to verify it fails**

Expected: link error, `MotionBlurReprojection` unresolved.

- [ ] **Step 5: Implement it in `motion_blur.cpp`**

```cpp
// See motion_blur.h.
#include <cstdio>

#include "motion_blur.h"
#include "gl_loader.h"

void MotionBlurReprojection(const CameraMatrix& current, const CameraMatrix& previous,
                            float out[16]) {
    // Column-major throughout: element (row, col) is m[col * 4 + row]. A view matrix is
    // [R | t] with R the world->view rotation, so R[row][col] == current.m[col * 4 + row] and
    // t[row] == current.m[12 + row].
    //
    // inverse([R | t]) == [R-transpose | -R-transpose * t] because R is orthonormal. Doing it
    // this way rather than with a general 4x4 inversion is not only cheaper, it cannot produce
    // a near-singular result on a matrix that is rigid by construction.
    float inv[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // (R-transpose)[row][col] == R[col][row] == current.m[row * 4 + col]
            inv[col * 4 + row] = current.m[row * 4 + col];
        }
    }
    for (int row = 0; row < 3; ++row) {
        float acc = 0.0f;
        for (int k = 0; k < 3; ++k) {
            acc += current.m[row * 4 + k] * current.m[12 + k];
        }
        inv[12 + row] = -acc;
    }

    // out = previous * inv
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float acc = 0.0f;
            for (int k = 0; k < 4; ++k) {
                acc += previous.m[k * 4 + row] * inv[col * 4 + k];
            }
            out[col * 4 + row] = acc;
        }
    }
}
```

- [ ] **Step 6: Run the test to verify it passes**

Expected: four PASS lines, exit 0.

- [ ] **Step 7: Mutation-check**

| Mutation | Must fail |
|---|---|
| `inv[col * 4 + row] = current.m[row * 4 + col];` → `= current.m[col * 4 + row];` (drop the transpose) | the rotation case |
| `inv[12 + row] = -acc;` → `= -current.m[12 + row];` (negate without rotating) | the rotation-and-translation case |
| `out[col * 4 + row] = acc;` → `= inv[col * 4 + row];` (ignore `previous`) | none of the four — **the test is incomplete.** Add a fifth case with a non-identity `previous` and assert the result differs from `inv` alone, then re-run this mutation and confirm it fails. |

The third row is deliberate: it is the check that finds a hole in the test rather than a hole in the code. Do not skip it.

- [ ] **Step 8: Commit**

```bash
git add motion_blur.h motion_blur.cpp motion_blur_test.cpp CMakeLists.txt
git commit -m "Add the pure camera reprojection matrix for motion blur"
```

---

### Task 3: `ApplyMotionBlur` — the compute stage

**Files:**
- Modify: `motion_blur.h`, `motion_blur.cpp`, `motion_blur_test.cpp`

**Interfaces:**
- Consumes: `MotionBlurReprojection` (Task 2); `ProjectionParams` from `projection_capture.h`; `GetGlComputeApi()`, `GetGlContextGeneration()` from `gl_loader.h`.
- Produces:
  ```cpp
  bool ApplyMotionBlur(unsigned int srcTexture, unsigned int dstTexture,
                       unsigned int depthTexture, unsigned int worldTexture,
                       unsigned int captureTexture,
                       int width, int height, const ProjectionParams& projection,
                       const float reprojection[16],
                       float strength, float maxRadius);
  ```

- [ ] **Step 1: Append the declaration to `motion_blur.h`**

```cpp
// Runs the stage, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are width x height RGBA16F 2D textures owned by the caller; depthTexture is the
// depth attachment the pipeline blitted for this frame.
//
// worldTexture is the pre-HUD frame from world_capture.h and captureTexture is the pipeline's
// pristine back-buffer capture. Where they differ by more than 1/128 the game drew an overlay,
// and that pixel is returned bit-exact - not nearly unchanged, exactly unchanged - and is also
// excluded from every other pixel's blur taps, so dialogue text never bleeds into the world
// behind it. Both must be the same size as width x height; the caller checks that.
//
// projection comes from GetCapturedProjection() and is what makes unprojecting raw depth
// possible at all. reprojection is MotionBlurReprojection()'s 16 floats.
//
// strength is GetAnaxConfig().motionBlurStrength, 0..1: a multiplier on the measured
// screen-space velocity. 0 reproduces the input bit-exact. maxRadius is
// GetAnaxConfig().motionBlurMaxRadius, a fraction of the screen: the longest smear allowed.
// That clamp is not a tuning nicety - on a scene cut or teleport the inter-frame camera delta
// is enormous and would smear the whole screen, and the clamp bounds that without needing cut
// detection.
//
// Sky needs no special case: cleared depth unprojects to the far plane, where the translation
// term vanishes and only rotation survives, which is how sky should behave.
//
// Returns true if dstTexture was actually written; returns false if GL 4.3 compute support is
// unavailable, shader init failed, or no depth/projection was supplied.
bool ApplyMotionBlur(unsigned int srcTexture, unsigned int dstTexture,
                     unsigned int depthTexture, unsigned int worldTexture,
                     unsigned int captureTexture,
                     int width, int height, const ProjectionParams& projection,
                     const float reprojection[16],
                     float strength, float maxRadius);
```

- [ ] **Step 2: Write the failing GPU tests in `motion_blur_test.cpp`**

Build the context the way `ssr_test.cpp` does. Create `src`, `dst`, `depth`, `world`, `capture` textures; fill `src` and `capture` with the same detailed pattern (a sinusoid, **not** a flat colour — a flat field gives a blur nothing to do, which is how the NIS overshoot bug survived its own test), fill `world` equal to `capture` except in one rectangle that stands in for the HUD, and fill `depth` with a mid-range constant.

```cpp
// THE load-bearing case. "The frame got blurrier" would pass with the velocity math ignored
// entirely, so this changes ONLY the previous camera between two runs and requires the same
// pixel to go from bit-exact to changed - the same framing ssr_test.cpp uses for upThreshold.
{
    CameraMatrix current;
    float still[16];
    MotionBlurReprojection(current, current, still);
    ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, still, 0.5f, 0.05f);
    unsigned char stillPx[4]; ReadPixel(dst, 64, 64, stillPx);

    CameraMatrix moved;            // previous camera rotated about Z
    moved.m[0] = 0.995f;  moved.m[1] = 0.0998f;
    moved.m[4] = -0.0998f; moved.m[5] = 0.995f;
    float rotated[16];
    MotionBlurReprojection(current, moved, rotated);
    ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, rotated, 0.5f, 0.05f);
    unsigned char movedPx[4]; ReadPixel(dst, 64, 64, movedPx);

    unsigned char srcPx[4]; ReadPixel(src, 64, 64, srcPx);
    ok = Check(stillPx[0] == srcPx[0] && stillPx[1] == srcPx[1] && stillPx[2] == srcPx[2],
               "a stationary camera leaves the pixel bit-exact") && ok;
    ok = Check(movedPx[0] != srcPx[0] || movedPx[1] != srcPx[1] || movedPx[2] != srcPx[2],
               "rotating ONLY the previous camera changes the same pixel") && ok;
}

// strength 0 is an exact no-op even with the camera moving.
{
    CameraMatrix current;
    CameraMatrix moved;
    moved.m[0] = 0.995f;  moved.m[1] = 0.0998f;
    moved.m[4] = -0.0998f; moved.m[5] = 0.995f;
    float rotated[16];
    MotionBlurReprojection(current, moved, rotated);
    ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, rotated, 0.0f, 0.05f);
    unsigned char px[4]; ReadPixel(dst, 64, 64, px);
    unsigned char srcPx[4]; ReadPixel(src, 64, 64, srcPx);
    ok = Check(px[0] == srcPx[0] && px[1] == srcPx[1] && px[2] == srcPx[2],
               "strength 0 reproduces the input bit-exact") && ok;
}

// A pixel inside the HUD rectangle is bit-exact even with the camera moving hard.
{
    CameraMatrix current;
    CameraMatrix moved;
    moved.m[0] = 0.995f;  moved.m[1] = 0.0998f;
    moved.m[4] = -0.0998f; moved.m[5] = 0.995f;
    float rotated[16];
    MotionBlurReprojection(current, moved, rotated);
    ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, rotated, 1.0f, 0.2f);
    unsigned char px[4]; ReadPixel(dst, kHudX, kHudY, px);
    unsigned char srcPx[4]; ReadPixel(src, kHudX, kHudY, srcPx);
    ok = Check(px[0] == srcPx[0] && px[1] == srcPx[1] && px[2] == srcPx[2],
               "a HUD pixel is returned bit-exact while the world blurs") && ok;
}
```

- [ ] **Step 3: Run to verify it fails**

Expected: link error, `ApplyMotionBlur` unresolved.

- [ ] **Step 4: Add the shader source to `motion_blur.cpp`**

```cpp
const char* kMotionBlurShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D colorTex;\n"
    "layout(binding = 1) uniform sampler2D depthTex;\n"
    "layout(binding = 2) uniform sampler2D worldTex;\n"
    "layout(binding = 3) uniform sampler2D captureTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform MotionBlurConfigBlock {\n"
    "    vec4 frustum;\n"        // left, right, bottom, top
    "    vec4 params;\n"         // zNear, zFar, strength, maxRadius
    "    mat4 reprojection;\n"   // previous * inverse(current)
    "};\n"
    "float LinearEyeDistance(float rawDepth, float zNear, float zFar) {\n"
    "    float ndc = rawDepth * 2.0 - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    // Lifted verbatim from ssr.cpp, which lifted it from ssao.cpp, so all three stages agree
    // about where a pixel is in space. Keeping left/right/bottom/top separate rather than
    // assuming a symmetric field of view means an off-centre frustum unprojects correctly.
    "vec3 ViewPosFor(vec2 uv, float eyeDist, float zNear) {\n"
    "    float x = (frustum.x + (frustum.y - frustum.x) * uv.x) * (eyeDist / zNear);\n"
    "    float y = (frustum.z + (frustum.w - frustum.z) * uv.y) * (eyeDist / zNear);\n"
    "    return vec3(x, y, -eyeDist);\n"
    "}\n"
    "vec2 UvForViewPos(vec3 p, float zNear) {\n"
    "    float dist = -p.z;\n"
    "    float xn = p.x * zNear / dist;\n"
    "    float yn = p.y * zNear / dist;\n"
    "    return vec2((xn - frustum.x) / (frustum.y - frustum.x),\n"
    "                 (yn - frustum.z) / (frustum.w - frustum.z));\n"
    "}\n"
    // Where the finished frame differs from the pre-HUD frame, the game drew an overlay. The
    // threshold sits just clear of 8-bit quantisation (1/255), which makes this "did the game
    // draw here" rather than a tolerance to tune - see world_capture.h.
    "bool IsHud(ivec2 c) {\n"
    "    vec3 finished = texelFetch(captureTex, c, 0).rgb;\n"
    "    vec3 world = texelFetch(worldTex, c, 0).rgb;\n"
    "    return any(greaterThan(abs(finished - world), vec3(1.0 / 128.0)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (coord.x >= size.x || coord.y >= size.y) { return; }\n"
    "    vec4 src = texelFetch(colorTex, coord, 0);\n"
    // Every early-out writes src untouched, so a pixel this stage declined to blur is
    // bit-identical to one it never saw.
    "    if (IsHud(coord)) { imageStore(outputImage, coord, src); return; }\n"
    "    float raw = texelFetch(depthTex, coord, 0).r;\n"
    "    vec2 uv = (vec2(coord) + vec2(0.5)) / vec2(size);\n"
    "    vec3 here = ViewPosFor(uv, LinearEyeDistance(raw, params.x, params.y), params.x);\n"
    "    vec3 before = (reprojection * vec4(here, 1.0)).xyz;\n"
    // Behind the previous frame's near plane there is no screen position to measure against,
    // and projecting it anyway would divide by a vanishing distance.
    "    if (before.z > -params.x) { imageStore(outputImage, coord, src); return; }\n"
    "    vec2 velocity = (uv - UvForViewPos(before, params.x)) * params.z;\n"
    "    float len = length(velocity);\n"
    "    if (len < 1e-6) { imageStore(outputImage, coord, src); return; }\n"
    "    if (len > params.w) { velocity *= params.w / len; }\n"
    "    vec3 sum = src.rgb;\n"
    "    float count = 1.0;\n"
    "    for (int i = 1; i < 8; ++i) {\n"
    "        vec2 tapUv = uv - velocity * (float(i) / 7.0);\n"
    "        ivec2 tap = ivec2(clamp(tapUv, vec2(0.0), vec2(1.0)) * vec2(size));\n"
    "        tap = clamp(tap, ivec2(0), size - ivec2(1));\n"
    "        if (IsHud(tap)) { continue; }\n"
    "        sum += texelFetch(colorTex, tap, 0).rgb;\n"
    "        count += 1.0;\n"
    "    }\n"
    "    imageStore(outputImage, coord, vec4(sum / count, src.a));\n"
    "}\n";
```

- [ ] **Step 5: Write the C++ half of `ApplyMotionBlur`**

Copy the structure of `ApplySsr` in `ssr.cpp` exactly — `PipelineState`, `CompileAndLink`, `EnsureUbo`, the generation check, the guard clauses — substituting this UBO:

```cpp
struct MotionBlurConfigData {
    float frustum[4];
    float params[4];
    float reprojection[16];
};
```

Guards, in order, each returning false:

```cpp
    if (!gl.loaded) { /* warn once, as ssr.cpp does */ return false; }
    if (depthTexture == 0 || worldTexture == 0 || captureTexture == 0) { return false; }
    if (width <= 0 || height <= 0) { return false; }
    if (projection.zNear <= 0.0f || projection.zFar <= projection.zNear) {
        /* warn once, as ssr.cpp does */ return false;
    }
```

Bindings, matching the shader's `layout(binding = N)` declarations exactly:

```cpp
    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);     gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE1);     gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glActiveTexture(GL_TEXTURE2);     gl.glBindTexture(GL_TEXTURE_2D, worldTexture);
    gl.glActiveTexture(GL_TEXTURE3);     gl.glBindTexture(GL_TEXTURE_2D, captureTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
```

`GL_TEXTURE2` is `0x84C2` and `GL_TEXTURE3` is `0x84C3`; add them to the file's constant block.

- [ ] **Step 6: Run to verify it passes**

Expected: all three GPU cases plus the four CPU cases PASS.

- [ ] **Step 7: Mutation-check**

| Mutation | Must fail |
|---|---|
| `vec2 velocity = ... * params.z;` → `vec2 velocity = vec2(0.0);` | "rotating ONLY the previous camera changes the same pixel" |
| `if (IsHud(coord)) { ... return; }` → delete the line | "a HUD pixel is returned bit-exact" |
| `if (len < 1e-6) { ... return; }` → delete the line | "strength 0 reproduces the input bit-exact" (the taps then all land on the same texel and the average is not bit-exact) |
| `if (IsHud(tap)) { continue; }` → delete the line | none of the three — **the test is incomplete.** Add a case asserting that a world pixel adjacent to the HUD rectangle does not pick up the HUD's colour, then re-run this mutation. |

- [ ] **Step 8: Commit**

```bash
git add motion_blur.h motion_blur.cpp motion_blur_test.cpp
git commit -m "Add the motion blur compute stage"
```

---

### Task 4: Pipeline wiring, configuration and documentation

**Files:**
- Modify: `post_effects.h`, `post_effects.cpp`, `post_effects_test.cpp`, `config.h`, `config.cpp`, `config_writer.cpp`, `config_editor.cpp`, `CMakeLists.txt`, `opengl32_enhancer.ini`, `README.md`, `docs/enhancement-opportunities.md`

**Interfaces:**
- Consumes: `ApplyMotionBlur`, `MotionBlurReprojection` (Tasks 2–3); `GetWorldOnlyFrame` (Task 1); `GetCapturedCamera`, `GetPreviousCamera` from `modelview_capture.h`.
- Produces: `EffectKind::MotionBlur`, `config.motionBlurStrength`, `config.motionBlurMaxRadius`.

- [ ] **Step 1: Write the failing test row in `post_effects_test.cpp`**

In the `kExpected` table inside `CheckDepthStageSet()`, add:

```cpp
        {EffectKind::MotionBlur,          true,  "motionblur"},
```

- [ ] **Step 2: Run to verify it fails**

Expected: compile error — `EffectKind::MotionBlur` does not exist.

- [ ] **Step 3: Add the enum member and its name**

In `config.h`, add `MotionBlur,` to `enum class EffectKind` immediately after `Ssr,`. In `config.cpp`, add `{EffectKind::MotionBlur, "motionblur"},` to the name table beside `{EffectKind::Ssr, "ssr"},`.

- [ ] **Step 4: Add the config fields**

In `config.h`, after the `ssrWorldUpAxis` block:

```cpp
    // MotionBlur. motionBlurMaxRadius is a fraction of the screen, not a world unit: it is the
    // longest smear allowed, and it exists because a scene cut or a teleport produces an
    // enormous inter-frame camera delta that would otherwise smear the whole frame. Clamping
    // is cheaper and more predictable than trying to detect a cut. See motion_blur.h.
    float motionBlurStrength = 0.5f;           // MotionBlur, 0 = exact no-op
    float motionBlurMaxRadius = 0.05f;         // MotionBlur
```

In `config.cpp`, in the key chain beside the `ssr*` entries:

```cpp
        } else if (strcmp(key, "motionBlurStrength") == 0) {
            config.motionBlurStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.motionBlurStrength, "motionBlurStrength");
        } else if (strcmp(key, "motionBlurMaxRadius") == 0) {
            config.motionBlurMaxRadius = ParseClampedFloat(value, 0.0f, 0.5f, config.motionBlurMaxRadius, "motionBlurMaxRadius");
```

Add both to the summary `printf` at the end of `ParseConfigFile`, following the `ssrUpThreshold=%.3f` pattern.

- [ ] **Step 5: Register both keys with the writer**

In `config_writer.cpp`, add `"motionBlurStrength",` and `"motionBlurMaxRadius",` to `kManagedKeys`, and:

```cpp
    if (strcmp(key, "motionBlurStrength") == 0)          { out = FormatFloat(config.motionBlurStrength); return true; }
    if (strcmp(key, "motionBlurMaxRadius") == 0)         { out = FormatFloat(config.motionBlurMaxRadius); return true; }
```

Forgetting this is the documented failure mode in `docs/enhancement-opportunities.md`: the editor silently drops the settings on save.

- [ ] **Step 6: Add `MotionBlur` to `StageNeedsDepth`**

In `post_effects.cpp`:

```cpp
        case EffectKind::Fog:
        case EffectKind::Ssr:
        case EffectKind::MotionBlur:
            return true;
```

- [ ] **Step 7: Run to verify the table row passes**

```
cmake --build --preset x86-release --target post_effects_test
cd build-x86-release && ./post_effects_test.exe
```

Expected: `PASS: StageNeedsDepth(motionblur) == true`.

- [ ] **Step 8: Add `captureTex` to the pipeline pool**

In `post_effects.cpp`'s `PipelineTextures`, beside `depthTex`:

```cpp
    // The pristine back-buffer capture, kept only when a stage that needs the UNMODIFIED frame
    // is listed. By the time such a stage runs, the ping-pong buffers have been through ssao
    // and whatever else precedes it, so the frame as the game drew it has to be held
    // separately. Same native-resolution reasoning as depthTex. See motion_blur.h on what it
    // is compared against.
    unsigned int captureTex = 0;
    int captureWidth = 0;
    int captureHeight = 0;
```

Allocate it in `EnsurePipelineTextures` alongside `depthTex`, gated on a new `needCapture` parameter, using `CreatePipelineTexture` (RGBA16F) so it matches `world_capture.cpp`'s format exactly.

In `ApplySelectedEffect`, beside the existing `needDepth` loop:

```cpp
    bool needCapture = HasEffectStage(config, EffectKind::MotionBlur);
```

and immediately after the existing `glCopyTexSubImage2D` into `pair[0]`, while the read framebuffer is still the default:

```cpp
    if (needCapture && g_pipeline.captureTex != 0) {
        gl.glBindTexture(GL_TEXTURE_2D, g_pipeline.captureTex);
        gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, nativeWidth, nativeHeight);
    }
```

- [ ] **Step 9: Add the stage case**

In `ApplySelectedEffect`'s switch, beside `case EffectKind::Ssr:`:

```cpp
            case EffectKind::MotionBlur: {
                // Needs four things the frame may not have: depth, a projection, both cameras,
                // and a world-only capture. Any one missing no-ops the stage rather than
                // guessing - see motion_blur.h.
                ProjectionParams mbProjection;
                CameraMatrix mbCurrent;
                CameraMatrix mbPrevious;
                unsigned int worldTex = 0;
                int worldW = 0, worldH = 0;
                if (depthCaptured && GetCapturedProjection(mbProjection) &&
                    GetCapturedCamera(mbCurrent) && GetPreviousCamera(mbPrevious) &&
                    GetWorldOnlyFrame(worldTex, worldW, worldH) &&
                    worldW == dstW && worldH == dstH &&
                    g_pipeline.captureTex != 0) {
                    float reprojection[16];
                    MotionBlurReprojection(mbCurrent, mbPrevious, reprojection);
                    wrote = ApplyMotionBlur(src, dst, g_pipeline.depthTex, worldTex,
                                            g_pipeline.captureTex, dstW, dstH, mbProjection,
                                            reprojection, config.motionBlurStrength,
                                            config.motionBlurMaxRadius);
                }
                break;
            }
```

Add `#include "motion_blur.h"` and `#include "world_capture.h"` to `post_effects.cpp`'s include block.

- [ ] **Step 10: Deliberately do NOT add the stage to the config editor**

Leave `config_editor.cpp` alone, and add a comment beside the `ssrWorldUpAxis` note explaining why, because the reason is identical.

`editor_scene.cpp:54` calls `CaptureProjectionFrustum()` **directly** rather than going through the wrapper's `glFrustum` hook, so the editor never calls `NotifyWorldProjection()` or `NotifyWorldPassBegan()`. It therefore has no captured camera and no world-only frame, which means `GetCapturedCamera()`, `GetPreviousCamera()` and `GetWorldOnlyFrame()` all return false and the stage provably no-ops there. A slider that cannot move anything is worse than an absent one: it reads as a broken effect rather than an unavailable one.

This is the same call the SSR work made for `ssrWorldUpAxis`, recorded in `config_writer_test.cpp`'s unmanaged-key test. Unlike that case, `motionBlurStrength` and `motionBlurMaxRadius` **are** managed by the writer (Step 5), because a user editing them by hand must not lose them on the next save.

- [ ] **Step 11: Add both `.cpp` files to the DLL target**

In `CMakeLists.txt`, add `motion_blur.cpp` to the `add_library` source list (`world_capture.cpp` went in during Task 1). Add both to `post_effects_test`'s source list too, or it will not link.

- [ ] **Step 12: Document in `opengl32_enhancer.ini`**

Add to the stage list near the top:

```
;   motionblur          camera motion blur (see `motionBlurStrength`, `motionBlurMaxRadius`)
```

Add a commented preset — and **do not** touch the active `effect=` line:

```
;effect=ssao, ssr, motionblur, bilinear, bloom, acestonemap, lutgrading, vignette, chromaticaberration, taa, dither, gamma
```

Add the settings block beside the `ssr*` one, covering: that it is camera motion only and an NPC crossing a still frame will not smear; that it reads depth so it must be listed before `bilinear`/`nvscaler`/`fsr`; and what `motionBlurMaxRadius` is really for.

- [ ] **Step 13: Update `README.md` and `docs/enhancement-opportunities.md`**

In the README, add `motionblur` to the stage table in the same shape as `ssr`. In `docs/enhancement-opportunities.md`, update the Tier 2 section: it currently reads *"nothing consumes it yet"*, which is already stale after the SSR world-up work, and motion blur is the second consumer. Record what was actually verified and what was not — **do not claim in-game validation before anyone has run it.**

- [ ] **Step 14: Run the full suite**

```
cmake --build --preset x86-release
cd build-x86-release && ctest --output-on-failure
```

Expected: 35/35 (33 existing + `world_capture_test` + `motion_blur_test`).

- [ ] **Step 15: Verify the writer round-trip mutation**

`config_writer_test.cpp` covers managed keys generically. Confirm it bites here: delete the `motionBlurStrength` line from `kManagedKeys`, rebuild, and confirm `config_writer_test` fails. Revert.

If it does **not** fail, that is the same gap found on the SSR branch — the test only checks keys it names. Add `motionBlurStrength` to the test's explicit round-trip assertions, then re-run the mutation.

- [ ] **Step 16: Commit**

```bash
git add -A
git commit -m "Wire camera motion blur into the effect pipeline"
```

---

## Verification beyond the tests

The unproven assumption is *"the first `glOrtho` means the world is finished"*. `cameraLogInterval` now reports whether the world capture latched each frame (Task 1, Step 5). After the branch is merged and installed:

1. Set `cameraLogInterval=60` and add `motionblur` to `effect=` in the game's ini.
2. Run Anachronox, walk and turn, quit.
3. In `opengl32_enhancer.log`, confirm `world_capture: ... latched=yes` on world frames, and that the size matches the game's render resolution.
4. Look at the game: floors and walls should smear when turning and stay sharp when still; **the HUD and dialogue boxes must stay sharp at all times**. A smeared HUD means the `glOrtho` assumption is wrong for this engine.

State plainly in any summary which of these were actually done. Do not report in-game validation that has not happened.
