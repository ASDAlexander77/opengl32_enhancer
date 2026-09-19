# Real TAA — Design

**Status:** approved 2026-09-19
**Supersedes:** nothing. Replaces the internals of the existing `taa` stage; keeps its name, its config keys, and its current behaviour as a fallback.

## Goal

Turn the `taa` stage from a temporal *stabiliser* into real temporal anti-aliasing: sub-pixel jitter so successive frames carry genuinely new samples, and camera reprojection so history is fetched from where each pixel actually was.

## What exists today, and why it is not TAA

`taa.h` describes the current stage accurately and calls it "TAA-lite": it blends the current frame with a history texture, clamping history into the current frame's 3x3 neighbourhood min/max to bound ghosting. It has no motion vectors and no jitter.

Without jitter, every frame samples the scene at the same sub-pixel position, so history holds no information the current frame lacks. Averaging a value with itself cannot anti-alias. The stage therefore stabilises flicker but leaves edges exactly as aliased as it found them. Two heuristics — an adaptive blend keyed on raw difference from history, and `shimmerSuppression` — exist to compensate for the missing reprojection.

## Decisions

Three questions were settled before design:

1. **Scope: both halves.** Reprojection *and* jitter. Reprojection alone is stabilisation, not anti-aliasing; jitter alone would be rejected by the clamp on every camera movement.
2. **HUD: reuse the world-capture mask.** `taa` opts into `world_capture.h` alongside `motionblur` rather than inventing a second mechanism.
3. **Fallback: keep TAA-lite.** When the capture heuristics do not fire, `taa` runs exactly today's code and behaves exactly as it does now.

A fourth followed from these: **the existing heuristics stay on the fallback path and are absent from the real path.** They are not merely redundant there — the adaptive blend reduces a pixel's history weight in proportion to its raw difference from history, and jitter makes every pixel differ from history every frame by design. Left in place, it would read the anti-aliasing signal as motion and suppress it.

## Architecture

### New unit: `taa_jitter.h` / `taa_jitter.cpp`

Owns the per-frame sub-pixel offset and the only write this project makes into the game's render state. It is a separate unit for three reasons: it runs on a different clock from every effect (`glFrustum`, not swap), it is the sole place the proxy stops being passive, and its core is pure arithmetic testable without a GL context.

```c
// Pure. Halton(2,3), 8-sample cycle, mapped to +/-0.5 pixels.
void TaaJitterOffset(unsigned int index, float& jx, float& jy);

// Called once per frame at swap, beside InvalidateWorldFrame().
void AdvanceTaaJitter();

// Called from the generated glFrustum wrapper. No-op unless armed; otherwise shifts the
// frustum window in place. Records what it applied so the resolve can undo it.
void ApplyTaaJitterToFrustum(double& left, double& right, double& bottom, double& top);

// The offsets actually applied, in FRUSTUM UNITS (not pixels), for this frame and the last.
// False when that frame was not jittered.
bool GetTaaJitterApplied(float& dx, float& dy);
bool GetPreviousTaaJitterApplied(float& dx, float& dy);

// Set by the resolve each frame: did TAA's real path actually run? Arms the next frame.
void NotifyTaaRealPathRan(bool ran);
```

### Injection point

One line in `generators/gen_wrapper_cpp.py`'s `glFrustum` block, and its **order is the whole design**:

```c
ApplyTaaJitterToFrustum(&left, &right, &bottom, &top);            // NEW - may modify in place
CaptureProjectionFrustum(left, right, bottom, top, zNear, zFar);  // now records what GL gets
NotifyWorldProjection();
NotifyWorldPassBegan();
__proc_glFrustum(left, right, bottom, top, zNear, zFar);
```

Because the jitter lands *before* the capture, `GetCapturedProjection()` returns the frustum the depth buffer was genuinely rendered with. **SSAO, SSR, DOF, fog and motion blur need no changes and remain self-consistent for free.**

This changes an invariant that `projection_capture.h` states explicitly today — "Recording only - never changes what the game asked for". It becomes "records what GL was actually given, which is what the game asked for unless TAA jitter is armed". That header comment and the generator's comment must both be updated; leaving either stale is a defect, not a nicety.

### The jitter formula, and the one place a sign can be wrong

Given `glFrustum(l, r, b, t, n, f)`, viewport `W x H`, and pixel offset `(jx, jy)`:

```text
dx = jx * (r - l) / W        l += dx;  r += dx
dy = jy * (t - b) / H        b += dy;  t += dy
```

Shifting both edges leaves `r-l` unchanged and moves `r+l` by `2*dx`, so `x_ndc' = x_ndc - 2*dx/(r-l) = x_ndc - 2*jx/W`, i.e. the rendered image translates by exactly **`-jx` pixels** horizontally and `-jy` vertically. That is the convention; a test pins it, and if the implementation lands on the opposite sign the *formula* is corrected, not the test.

The resolve unprojects with the jittered frustum that produced the depth, and projects into history with the unjittered previous frustum. Two corrections then have to be applied on top, and **an earlier draft of this spec claimed they were unnecessary — that claim was wrong and cost a Critical review finding**, so it is spelled out here:

- Reconstructing the unjittered previous frustum subtracts the **previous** frame's recorded `(dx, dy)`.
- The **current** frame's jitter must be subtracted from the resulting history UV.

The second is the one the earlier draft missed, on the reasoning that consuming matrices rather than offsets made the composition self-consistent. It does not. Unprojecting with the jittered current frustum and projecting with the unjittered previous one produces a **scene-space motion vector** — where this scene point was last frame — but history is indexed on the **pixel grid**. The two differ by exactly the current jitter: measured on the built shader at `l=-1, r=1, W=1600, jx=0.375`, the history UV landed 0.375 px from the pixel being resolved.

Left uncorrected it is not a rounding detail. The Halton `jx` cycle has mean `-0.0547`, so under the history recursion `D <- blend*(D + j)` the image settles at a systematic displacement of about `-0.36` px with a period-8 oscillation — the "permanent wobble" this section warns about, produced by single-counting the offset rather than double-counting it. It also blunts the feature itself, since fetching history at the current sample's position re-averages the same scene point instead of accumulating new sub-pixel samples.

Standard TAA computes velocity with **both** ends unjittered, so a static camera yields exactly zero. With both corrections applied, a jitter sign error can still only originate in `JitterFrustumBounds` and these two subtractions.

`W` and `H` come from `glGetIntegerv(GL_VIEWPORT)` at `glFrustum` time. Quake II's `R_SetupGL` sets the viewport before the frustum, and `post_effects.cpp` (around line 415) already reads it the same way. One call per frame — the precedent `modelview_capture.h` set with its single `glGetFloatv`.

### Extending `projection_capture.h`

The resolve needs the **previous** frame's frustum, not only the current one, because a game may change FOV between frames and because the previous frame's jitter must be subtracted from the frustum it was actually applied to. Add, mirroring the existing `GetPreviousCamera()` in `modelview_capture.h` exactly:

```c
// The frustum captured on the previous frame, or false before two frames have been seen.
bool GetPreviousProjection(ProjectionParams& out);
```

"Previous frame" advances on the same swap-time tick as `AdvanceTaaJitter()` and the existing camera history, so `GetPreviousProjection()` and `GetPreviousTaaJitterApplied()` always describe the same frame. They are read as a pair and would be meaningless if they drifted apart.

### Arming

Jitter with nothing resolving it is strictly worse than no jitter: it is pure added shimmer. It is armed only when all of:

- `taa` is present in the effect chain (`HasEffectStage`), and
- `taaJitter` is true, and
- **last frame's TAA actually ran its real path** (`NotifyTaaRealPathRan`).

The one-frame lag is harmless: the first frames have no history and output the current frame unchanged regardless. This condition also prevents the class of defect world capture shipped with, where a subsystem ran every frame for users who had not asked for it.

**This cannot deadlock**, and the reason should be checked rather than assumed: the real path's preconditions are depth, both projections, both cameras and the world capture — *not* jitter. So the first world frame runs the real path un-jittered (reprojection only), which arms jitter for the second. Jitter is never a precondition of the signal that enables it.

## The resolve

`taa.cpp` holds two compute programs. The **lite** program is today's shader, byte-identical — not "mostly unchanged" — so the fallback path cannot regress. The **real** program is new. Selection happens at dispatch: real when depth, both projections, both cameras and the world capture are all present; lite otherwise.

Real path, per pixel:

1. **HUD test first.** `any(abs(capture(p) - world(p)) > 1/128)`, the predicate `motion_blur.cpp` already uses. A HUD pixel takes the current frame verbatim and seeds history — no reprojection, no clip. Subtitles neither ghost nor soften.
2. **Unproject** this pixel's depth with the **jittered** current frustum.
3. **Reproject** by `V_prev * V_cur^-1`, from `MotionBlurReprojection()`, reused unchanged.
4. **Project** with the **unjittered** previous frustum, then **subtract the current frame's jitter** in UV units to get the history UV. Both subtractions are required — see "The jitter formula, and the one place a sign can be wrong" above for why omitting the second displaces the whole image by about a third of a pixel. A static camera must yield a history UV exactly equal to the pixel's own UV; that is the property to test.
5. **Reject** if that UV is outside `[0,1]`: the pixel was off screen last frame, so there is no history. Take current.
6. **Variance clip**, not min/max: compute the mean and standard deviation of the 3x3 current neighbourhood (including the centre, 9 samples) and clip history to `mean +/- gamma*sigma` with **`gamma = 1.0`**. `taa.h` already documents stale history surviving inside the looser min/max box on busy content. `gamma` is a compile-time constant, not a config key — it is not exposed until there is evidence a user needs to tune it.
7. **Blend** by `taaBlend` and **dual-store** to output and history, keeping the existing single-dispatch trick.

`shimmerSuppression` is inert on the real path — jitter plus reprojection is what it was approximating. It keeps its exact current meaning on the fallback path. `PrintConfig`, the ini and the README must say which path it applies to rather than implying it always works.

The resolve calls `NotifyTaaRealPathRan()` with whether the real path ran, on every frame `taa` executes, including the frames it declines — that signal is what arms or disarms the next frame's jitter.

## Required cross-file registrations

These are listed explicitly because this project has already shipped one bug of exactly this shape — `Ssr` missing from the depth-stage set, silently disabling it.

- `StageNeedsDepth()` in `post_effects.cpp` gains `Taa`.
- `projection_capture.h` / `.cpp` gain `GetPreviousProjection()` and the swap-time tick that advances it.
- The world-capture gate is currently **duplicated**: `post_effects.cpp` (around line 512) and `world_capture.cpp` (around line 71) each test `HasEffectStage(config, EffectKind::MotionBlur)` by hand. Collapse both into one `StageNeedsWorldCapture()` beside `StageNeedsDepth()`, returning true for `MotionBlur` **or** `Taa`. Removing the duplication is part of this work, not a follow-up.
- `config.cpp`: parse `taaJitter`, and add it to `PrintConfig`.
- `config_writer.cpp`: **both** `kManagedKeys` and `FormatValueFor`. The motion blur work established that adding to only the first is insufficient to rewrite an existing line.
- `config_editor.cpp`: a checkbox beside the existing `taaBlend` slider.
- `opengl32_enhancer.ini` and `README.md`.

## Config

One new key.

```ini
; taaJitter is true/false (default true), used by `taa`. Sub-pixel jitter is what lets TAA
; accumulate genuinely new samples; without it `taa` only stabilises. Turn it off to keep
; reprojection but leave the game's projection matrix untouched.
taaJitter=true
```

`taaBlend` and `shimmerSuppression` keep their names, ranges and defaults. The shipped ini already sets `taaBlend=0.85`, which suits the real path; no retuning or migration note is needed.

## Testing

Every test below is mutation-checked: the named change is made to the production code and the test is proven to fail. A passing test alone proves nothing.

| Test | Mutation that must break it |
|---|---|
| Halton offsets over one 8-cycle are distinct, mean ~0, all within +/-0.5 | return a constant |
| At 1600x1200, `jx=0.5` shifts `left`/`right` by exactly half a pixel of frustum width | drop the `/ W` |
| Disarmed, `ApplyTaaJitterToFrustum` leaves all four bounds bit-identical | remove the arming gate |
| Jitter sign: a view-space point projecting to `(cx, cy)` unjittered projects to `(cx-jx, cy-jy)` jittered | negate `dx` |
| Unjittered previous frustum round-trips: jitter, then reconstruct, returns the original bounds | use the current offset instead of the previous |
| Static camera reprojects a pixel to its own UV within 1e-5 | swap current/previous |
| A HUD-masked pixel takes the current frame exactly | invert the mask test |
| `gpu`-labelled: edge-pixel variance over N frames of a static jittered scene falls below the un-jittered baseline | force jitter to zero |
| `taaJitter` parses, defaults true, and round-trips through `config_writer` | remove it from `FormatValueFor` |

## Limits, stated rather than discovered

- **No per-object motion vectors.** Camera reprojection handles the camera. A moving NPC under a still camera will ghost, because nothing in the GL stream says that pixel moved. An engine's TAA gets this from its renderer; a proxy cannot. This is inherent, not a defect to file.
- **Engines that do not call `glFrustum`** capture nothing and get the lite path, unchanged.
- **TAA softens.** Sampling history bilinearly at a reprojected UV re-filters it every frame. The conventional remedy is a mild sharpener *after* the stage, so `effect=..., taa, cas, ...` is the right pairing. Note that this is the **opposite** of the motion blur rule, where a following sharpener is harmful; both belong in their own headers.
- **Jitter changes what the game renders.** A game that reads back its own framebuffer sees jittered content. `taaJitter=false` is the escape hatch.

## Out of scope

- Per-object motion vectors (impossible from a proxy).
- Catmull-Rom history sampling to counter TAA softness. Bilinear first; revisit only if the softness is judged unacceptable in game.
- Dynamic resolution or upscaling integration (TAAU).
