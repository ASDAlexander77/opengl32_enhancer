# Camera motion blur: reprojecting the frame through two cameras

Status: approved design, not yet implemented. Brainstormed 2026-09-19.
Consumes `modelview_capture.h` (Tier 2, shipped 2026-09-19) and introduces
one new capture point of its own.

## Goal

A `motionblur` stage in the shared post-effect pipeline that smears the
frame along the camera's own movement, so that spinning, strafing and
running produce a visible cinematic blur.

The blur is **per-pixel and parallax-correct**: a pixel's velocity comes
from unprojecting its depth, carrying that position through the previous
frame's camera, and measuring how far it moved on screen. Near geometry
smears more than distant geometry when the camera translates, and only
rotation moves the sky.

Success is the same two-part criterion Tier 2 used: unit tests that fail
when the code they cover is broken, and a look at the real game.

## Non-goals

**Per-object motion blur.** A character walking across a stationary frame
will not smear. The proxy sees `glVertex`-era fixed-function geometry, not
per-object transforms, so there is nowhere to get an object's velocity
from. This is a permanent limitation of the interception surface, not a
deferred feature — it is the reason the stage is named for the camera.

**Shutter-angle simulation.** No frame timer, no exposure model. The
inter-frame camera delta already encodes frame time: more time between
frames means more camera movement, so a fixed strength multiplier is
frame-rate-adaptive in the right direction for free. Measuring wall-clock
would add a dependency to buy nothing.

**Fixing the HUD problem for other stages.** The world-only capture this
design introduces would also let `ssao` and `ssr` stop treating HUD pixels
as world geometry. That is a real improvement and explicitly out of scope
here; this design adds the capture and one consumer.

## The HUD problem, and why it shapes the design

Quake II's `R_SetGL2D` disables depth testing before drawing the HUD.
Depth writes require depth testing, so **HUD pixels never write depth** —
they keep whatever the world wrote behind them. Depth therefore cannot
distinguish a subtitle from the wall it covers.

Left alone, a camera blur would smear Anachronox's HUD, subtitles and
dialogue boxes along with the world. Unlike `ssao`'s quiet darkening of
those same pixels, smeared text reads immediately as broken.

The fix is to capture the frame **before** the HUD is drawn, and use the
difference between that and the finished frame as a HUD mask.

## The new capture point

`glFrustum` arms; the first `glOrtho` of a frame latches. This is the same
discrimination `modelview_capture.h` and `projection_capture.h` already
make, at the same two hook points, and it is deliberately not a second
heuristic — a world pass begins at `glFrustum`, and the 2D pass begins at
`glOrtho`, so at the first `glOrtho` the world is complete and the HUD has
not started.

The generator already emits `NotifyTwoDProjection()` at `glOrtho` **before**
the passthrough (`generators/gen_wrapper_cpp.py`), so the hook fires at
exactly the right instant.

On latching, the module does one `glCopyTexSubImage2D` of the back buffer
into a persistent texture at the game's render resolution. That is the same
call `post_effects.cpp` already makes at swap time, so the copy path is
proven; the difference is that this one runs **inside the game's own GL
state** rather than in the pipeline's save/restore window, so it saves and
restores the texture binding and read buffer around itself. `texture_effect.cpp`
establishes that discipline at `glTexImage2D`.

### Staleness

The capture reports false unless it latched during the current frame. A
frame that issues no `glOrtho` leaves no world capture, and the stage
declines to run rather than blurring against the previous frame's world.
This is the one place where being wrong would be invisible, so it fails
closed.

Invalidation happens at swap **after the effect chain has run**, not
before it — the chain is the consumer, so invalidating on entry would
destroy the very frame the stage needs. This is the same ordering
`modelview_capture` already uses, where `FinalizeCameraForFrame()` runs
before the chain and `AdvanceCameraHistory()` after it, and
`InvalidateWorldFrame()` belongs beside the latter.

### Where it breaks

- **3D drawn after the first `glOrtho`** — a rendered portrait inside a
  dialogue box, an inset viewport. Those pixels are absent from the world
  capture, so they differ from the finished frame and read as HUD: they
  simply do not blur. Degrades to "less blur", never to corruption.
- **A frame with no `glOrtho`** — no capture, no blur that frame.
- **A game that draws its HUD without `glOrtho`** — no capture ever, so
  the stage never runs. `GetWorldOnlyFrame()` returns false rather than
  guessing.

## Velocity

Per frame, on the CPU:

    M = V_prev · V_current⁻¹

A view matrix is rigid, so the inverse is `Rᵀ` and `−Rᵀt` — no general
4×4 inversion. `M` is uploaded once per frame in the stage's UBO.

Per pixel, in the shader:

1. Unproject the hardware depth at `p` to a view-space position, the same
   way `ssao.cpp` and `ssr.cpp` already do it, using the frustum from
   `projection_capture.h`.
2. Multiply by `M` to get that same world point in the **previous**
   frame's view space.
3. Project with the same captured frustum to get the previous screen
   position.
4. `velocity = (p − p_prev) · strength`, clamped to `maxRadius`.
5. Average 8 taps along that vector.

The same projection is used for both frames. The frustum is captured once
and a game that changes FOV mid-frame is already outside what
`projection_capture.h` models.

**Sky needs no special case.** Cleared depth unprojects to the far plane,
where the translation term vanishes and only rotation survives — which is
exactly how sky should behave. This falls out of the math rather than
being branched for.

**`maxRadius` is not a tuning nicety.** On a scene cut, a teleport or a
camera snap, the inter-frame delta is enormous and would smear the whole
screen. The clamp bounds that without needing cut detection.

## HUD compositing

    hud(p) = any(|captureTex(p) − worldTex(p)| > ε)

`ε = 1/128`. Both textures are copies of the same 8-bit back buffer held
in RGBA16F, so identical pixels differ by nothing at all and the only
noise floor is 8-bit quantisation at 1/255. `1/128` sits just clear of
that, which makes the test "did the game draw something here" rather than
a tolerance to tune. A HUD element whose blend is fainter than one part in
128 is not visible anyway, so a false negative there costs nothing.

Two uses:

- A **tap** landing on a HUD pixel is rejected, so dialogue text never
  bleeds into the world behind it.
- A **centre** pixel that is HUD returns `src(p)` **bit-exact** — not
  nearly unchanged, exactly unchanged, the same guarantee `ssr` and
  `lut_grading` make, so that an overlay the stage decided not to touch is
  indistinguishable from the stage never having run.

`captureTex` is the pristine back-buffer capture. By the time the stage
runs, the ping-pong buffers have been through `ssao` and whatever else
precedes it, so the unmodified frame has to be kept separately: a second
`glCopyTexSubImage2D` immediately after the existing one, gated on a stage
that needs it being listed — the same pattern, and the same reasoning, as
`needDepth`.

## Interface

    // world_capture.h
    void NotifyWorldPassBegan();     // glFrustum
    void NotifyTwoDPassBegan();      // glOrtho
    void InvalidateWorldFrame();     // swap
    bool GetWorldOnlyFrame(unsigned int& texture, int& width, int& height);

    // motion_blur.h
    void MotionBlurReprojection(const CameraMatrix& current,
                                const CameraMatrix& previous,
                                float out[16]);   // pure, no GL

    bool ApplyMotionBlur(unsigned int srcTexture, unsigned int dstTexture,
                         unsigned int depthTexture, unsigned int worldTexture,
                         unsigned int captureTexture,
                         int width, int height,
                         const ProjectionParams& projection,
                         const float reprojection[16],
                         float strength, float maxRadius);

`MotionBlurReprojection` is pure and separately testable, mirroring
`SsrViewSpaceWorldUp`. `ApplyMotionBlur` takes the finished matrix rather
than two cameras for the same reason `ApplySsr` takes a pre-computed up
vector: no stage header includes `config.h`.

`GetWorldOnlyFrame()` reports the game's native render resolution, which
is also the only resolution the depth buffer exists at. The stage's own
`width`/`height` can only differ from it after a real upscale, and
`motionblur` reads depth, so `StageNeedsDepth()` already skips it there
with the "list it before the upscaler" warning. The caller passes the
world texture through unconditionally and the dimensions agree by
construction; a mismatch means the registration below was not done.

## Configuration

Two keys, deliberately:

- `motionBlurStrength` — 0..1, default 0.5. 0 reproduces the input
  bit-exact.
- `motionBlurMaxRadius` — screen fraction, default 0.05.

Sample count is fixed at 8 in the shader, the way `ssr` fixes its 32 steps
rather than exposing them.

`motionblur` is **not** added to the shipped `effect=` line. It goes in a
commented preset, so enabling it is a deliberate act.

## Registration

`motionblur` reads depth, so it must be added to `StageNeedsDepth()` in
`post_effects.h`/`post_effects.cpp`. That function and its test exist as of
2026-09-19 precisely because `ssr` was missing from the two hand-maintained
lists it replaced; the `post_effects_test.cpp` table gets a row.

## No-op conditions

All return false and leave the frame bit-exact:

- GL 4.3 compute unavailable, or shader init failed
- no depth captured this frame
- no projection captured (menu-only frames never call `glFrustum`)
- no current camera, or no previous camera — `GetPreviousCamera()` is
  false until one frame has completed
- no world capture this frame (see **Staleness**)
- `strength == 0`, or a zero camera delta

## Verification

### Unit tests

`motion_blur_test.cpp`, against a real GL context. The load-bearing case is
framed the way `ssr_test.cpp`'s was, and for the same reason — "the frame
got blurrier" would pass with the velocity math ignored entirely:

- Run twice changing **only the previous camera** — once equal to the
  current camera, once rotated — and require the same pixel to go from
  **bit-exact to changed**.
- `strength = 0` is bit-exact.
- A region where `captureTex` differs from `worldTex` is bit-exact.
- `MotionBlurReprojection`, with no GL at all: identity cameras give an
  identity matrix; a known translation gives a known matrix.

`world_capture_test.cpp`, mirroring `modelview_capture_test.cpp`:

- `glFrustum` then `glOrtho` latches.
- `glOrtho` with no preceding `glFrustum` does not latch.
- A second `glOrtho` in the same frame does not re-latch.
- After a swap, it reports false until it latches again.

Every test is mutation-checked: the code it claims to cover is broken, and
the test must fail. A passing test proves nothing on its own.

### In-game

The unproven assumption is *"the first `glOrtho` means the world is
finished"* — the same class of claim Tier 2's latch was, and settleable
only in a real game. `cameraLogInterval`, which exists for exactly this
purpose, also reports whether the world capture latched this frame. No new
config surface.

### What the tests cannot prove

Whether it looks good, and whether that `glOrtho` assumption holds in
Anachronox. Both need a human with the game running.

## Cost

One extra full-res copy at `glOrtho`, one more at swap for `captureTex`,
one compute dispatch — all paid only when `motionblur` is listed.

## Files

Create:

- `world_capture.h` / `world_capture.cpp` / `world_capture_test.cpp`
- `motion_blur.h` / `motion_blur.cpp` / `motion_blur_test.cpp`

Modify:

- `generators/gen_wrapper_cpp.py` — hooks at `glFrustum`, `glOrtho`, swap
- `post_effects.cpp` / `post_effects.h` — `captureTex`, `StageNeedsDepth`,
  the stage call
- `post_effects_test.cpp` — a row in the depth-stage table
- `config.h` / `config.cpp` / `config_writer.cpp` / `config_editor.cpp`
- `CMakeLists.txt`, `opengl32_enhancer.ini`, `README.md`
- `docs/enhancement-opportunities.md` — record the Tier 2 consumer
