# Modelview capture: reconstructing the camera each frame

Tier 2 of `docs/enhancement-opportunities.md`. Approved 2026-09-19.

## Goal

Record the game's **camera (view) matrix** once per frame, so that later work
can reproject between frames. Nothing in this increment consumes it.

`projection_capture.h` already records the world *projection*. That is half of
what is needed to relate a pixel to a point in space: projection says how view
space maps to the screen, but not where the camera is or which way it points.
The missing half is what caps several shipped stages at once —

- `taa.h` states it outright: *"this proxy has no access to the app's per-object
  motion vectors, unlike a real engine's TAA"*. The neighbourhood-clamp and
  raw-difference heuristics in `taa.cpp` exist entirely to work around it.
- `ssr.h` measures its `upThreshold` gate in **view** space rather than world
  space, so pitching the camera swings the gate off true. SSR is the first
  shipped stage whose quality is directly capped by this absence.

This spec covers the capture only. Consumers come later, separately.

## Non-goals

- **No stage changes behaviour.** SSR keeps its view-space `up`, TAA keeps its
  raw-difference heuristic. No motion blur, no temporally accumulated SSAO, no
  frame generation. The capture must be trusted before anything moves.
- **No matrix-stack emulation.** The driver's own matrix is read back rather
  than recomputed; see "Why sample rather than emulate".
- **No modification of any call.** Like `projection_capture.cpp`, every hook is
  recording-only and falls through to the normal passthrough. This increment
  cannot change what the game renders.
- **No object-level transforms.** Entity matrices are deliberately discarded.
  Per-object motion vectors remain out of reach; only *camera* motion is
  recovered.

## Why sample rather than emulate

Two ways to learn the modelview matrix: replay the fixed-function matrix
algebra on the CPU, or ask the driver.

We ask the driver — `glGetFloatv(GL_MODELVIEW_MATRIX)` — because the captured
value is then bit-for-bit what the game's GL actually holds and cannot drift
from it. Emulation would mean a second implementation of fixed-function matrix
semantics (column-major storage, post-multiplication, `glRotatef`'s axis
normalization and Rodrigues form, and the `d` variants of all six operations)
that can silently disagree with the driver. Its only advantages — no current
context needed, no loader dependency — are hypothetical here, since this code
only ever runs inside a GL entry point with a live context.

The hard part is not *how* to obtain the matrix. It is *when*, and that problem
is identical either way.

## The hazard, and the heuristic

`projection_capture.h` documents the shape of the problem: id Tech 2-era
engines draw the HUD last under an orthographic projection, so "the most recent
matrix at swap time" is the HUD's, not the world's. The modelview has the same
hazard twice over — the HUD pass resets it, *and* every entity drawn in the
world pass overwrites it with an object transform.

The heuristic mirrors the projection one and adds a second axis:

1. **`glFrustum` arms, `glOrtho` disarms.** This is exactly the existing
   discrimination, reused: a world pass is in progress only between them.
2. **Depth 0 only.** Track `glMatrixMode` and per-mode push/pop depth. Entity
   transforms in Quake II-family engines are all issued inside
   `glPushMatrix`/`glPopMatrix` — `R_RotateForEntity`, `R_DrawSkyBox`, the
   alias and brush model paths — so a modelview edit at depth >= 1 is an object
   transform and is ignored.

### The latch

Sampling on every matrix call would work but wastes a driver query per call.
Instead the read is deferred to the one moment the camera is known to be final:

- **`glFrustum`** — a new world pass begins. Rotate `current -> previous`,
  clear `pending` and `latched`. History rotates *here*, not at swap, so a
  stage reading at swap time sees a stable current/previous pair.
- **`glOrtho`** — disarm.
- A matrix edit while **armed, mode == `GL_MODELVIEW`, depth == 0** sets
  `pending = true`. No GL call is made.
- **Latch** — read `GL_MODELVIEW_MATRIX` at the first of: `glPushMatrix` while
  pending, `glOrtho` while pending, or `wglSwapBuffers` while pending. Set
  `latched`; ignore everything until the next `glFrustum`.

Exactly one `glGetFloatv` per frame.

All three latch points are safe because the modelview still holds the camera at
each: `glPushMatrix` copies the top of stack without modifying it, and Quake
II's `R_SetGL2D` issues `glMatrixMode(GL_PROJECTION); glLoadIdentity();
glOrtho(...)` *before* it touches the modelview. Latching at the *first* push is
what excludes entity transforms; the `pending` guard is what stops a push/pop
pair occurring before the camera sequence from latching prematurely.

Against Quake II's `R_SetupGL` this reads: arm at `glFrustum`; the
`glLoadIdentity`, five `glRotatef` and `glTranslatef` that follow set pending;
the first entity `glPushMatrix` latches the complete camera matrix.

### Where it breaks

To be stated in the header, in the manner of `projection_capture.h`:

- A frame with **no world pass** (a menu-only frame) leaves the previous
  camera standing, stale. Same behaviour as the captured frustum.
- An engine that establishes its 3D view **without `glFrustum`** — a hand-built
  matrix through `glLoadMatrixf` — captures nothing, and
  `GetCapturedCamera()` stays false. Consumers must no-op rather than guess.
- An engine that edits the modelview at depth 0 **before** its camera sequence
  and then never pushes would latch the wrong matrix.

This is the engine-specific part, and it is a heuristic, not a fact.

## Interception surface

Sixteen hook points in `generators/gen_wrapper_cpp.py`, all issued *before*
forwarding, all recording-only. `glFrustum` and `wglSwapBuffers` already have
hooks and gain one more each.

| Entry point | Call |
|---|---|
| `glFrustum` | `NotifyWorldProjection()` |
| `glOrtho` | `NotifyTwoDProjection()` |
| `glMatrixMode` | `NotifyMatrixMode(mode)` |
| `glPushMatrix` / `glPopMatrix` | `NotifyMatrixPush()` / `NotifyMatrixPop()` |
| `glLoadIdentity` | `NotifyMatrixEdited()` |
| `glLoadMatrixf` / `glLoadMatrixd` | `NotifyMatrixEdited()` |
| `glMultMatrixf` / `glMultMatrixd` | `NotifyMatrixEdited()` |
| `glRotatef` / `glRotated` | `NotifyMatrixEdited()` |
| `glTranslatef` / `glTranslated` | `NotifyMatrixEdited()` |
| `glScalef` / `glScaled` | `NotifyMatrixEdited()` |
| `wglSwapBuffers` | `FinalizeCameraForFrame()`, before `ApplySelectedEffect` |

All sixteen are already exported and forwarded in `wrapper.def`; no new exports
are needed.

## Interface

`modelview_capture.h`, shaped after `projection_capture.h`: single render
thread assumed, no synchronization, same as `gl_loader.h`.

```c
// Column-major, exactly as glGetFloatv(GL_MODELVIEW_MATRIX) returns it.
struct CameraMatrix { float m[16]; };

// World-space, derived from the above. R is the upper-left 3x3 of the view
// matrix; its rows are the camera's axes expressed in world coordinates.
struct CameraPose {
    float position[3];                 // -R^T * t, t = (m[12], m[13], m[14])
    float right[3];                    // (m[0], m[4], m[8])
    float up[3];                       // (m[1], m[5], m[9])
    float forward[3];                  // -(m[2], m[6], m[10])
};

bool GetCapturedCamera(CameraMatrix& out);   // false until the first latch
bool GetPreviousCamera(CameraMatrix& out);   // false until the second
void DecomposeCamera(const CameraMatrix& in, CameraPose& out);
```

`DecomposeCamera` exists because the in-game verification log needs readable
numbers. `GetPreviousCamera` is the single forward-looking element: three lines
at arm time, and the thing reprojection will need.

### Resolving `glGetFloatv`

`modelview_capture.cpp` resolves `glGetFloatv` itself, once, via
`GetSystemDirectoryA` + `LoadLibraryA` + `GetProcAddress` on the real
`opengl32.dll` — deliberately **not** through `gl_loader.h`, which gates on GL
4.3 as a unit (see its header comment on why the two categories share one
flag). Matrix capture must keep working on a machine where the compute stages
cannot run at all.

`GetSystemDirectoryA` rather than a hardcoded path, matching `wrapper.cpp`.
Noted in passing: `gl_loader.cpp` still hardcodes `C:\Windows\System32`. Out of
scope here.

## Verification

### Unit tests

`modelview_capture_test.cpp`, linking `modelview_capture.cpp` against a real GL
context (the harness in `ssao_test.cpp`). The test drives real GL calls and
asserts the captured matrix against what the driver itself reports, so it
verifies the *gate*, not the arithmetic.

Framed so that the cases which would pass trivially with the gate ignored are
the ones that fail:

1. A full `R_SetupGL` sequence — `glFrustum`, then `glLoadIdentity` + five
   `glRotatef` + `glTranslatef` — latches a matrix equal to the driver's.
2. A HUD pass (`glOrtho`, `glLoadIdentity`, 2D transforms) after the world pass
   must **not** overwrite the captured camera.
3. An entity `glPushMatrix` + `glTranslatef`/`glRotatef` + `glPopMatrix` must
   **not** overwrite it.
4. A frame with no `glFrustum` leaves `GetCapturedCamera()` false.
5. A push/pop pair *before* the camera sequence must not latch early; the
   camera established afterwards is the one captured.
6. Two consecutive world passes: `GetPreviousCamera()` returns the first while
   `GetCapturedCamera()` returns the second.
7. `DecomposeCamera` recovers a known eye position and basis from a matrix
   built by real `glRotatef`/`glTranslatef`.

### In-game

New config key `cameraLogInterval` (frames; `0` = off, default `0`). When set,
prints the reconstructed world position and basis every N frames to
`opengl32_enhancer.log` (see `debug_log.h` — stdout redirection, so a plain
`printf`). A one-shot "captured first camera" line mirrors
`projection_capture.cpp`'s first-frustum line.

Acceptance: run Anachronox with `cameraLogInterval` set, walk a straight line
and turn on the spot, and confirm the logged position and basis track the
movement. This is the only check that can confirm the heuristic picked the
right matrix; the unit tests confirm only that it behaves as designed.

## Files

New: `modelview_capture.h`, `modelview_capture.cpp`,
`modelview_capture_test.cpp`.

Modified: `generators/gen_wrapper_cpp.py` and the regenerated `wrapper.cpp`;
`CMakeLists.txt` (the DLL's source list — it links the generated `wrapper.cpp`,
which is what calls the hooks — plus the new test target; no existing test
target needs it, since no stage consumes the capture yet); `config.h`, `config.cpp`,
`config_writer.cpp`, `config_editor.cpp`, `opengl32_enhancer.ini`, `README.md`
for `cameraLogInterval`; `docs/enhancement-opportunities.md` to record the
outcome.

`config_writer.cpp` keeps an explicit key allow-list. Omitting the new key
there means the editor silently drops it on save — the hazard
`docs/enhancement-opportunities.md` calls out by name.
