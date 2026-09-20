# Supersampling (DSR) — design

**Date:** 2026-09-20
**Status:** approved, ready for an implementation plan
**Feature name in config:** `renderWidth` / `renderHeight`

## What this builds

The proxy renders the game into an offscreen framebuffer larger than the window,
then downsamples that image to the window on present. The game believes nothing
has changed; every pixel it draws is simply made of more samples.

This is the single biggest raw image-quality win available to a wrapper, and it
is the one thing `windowWidth`/`windowHeight` cannot deliver: that setting makes
the *window* bigger, which makes the game's own image get stretched, not
resampled.

## Why now, and what the old plan got wrong

`docs/enhancement-opportunities.md` deferred this behind two blockers. Both have
been checked, and the second one was wrong.

**DPI virtualisation — cleared.** The doc is right that a non-DPI-aware game
gets a virtualised desktop and a bitmap stretch applied after every sharpening
stage this project runs. It is also right that this cannot be fixed from inside
the DLL. On the development machine it is already fixed the documented way:
`anox.exe` carries `HIGHDPIAWARE` in `HKCU\...\AppCompatFlags\Layers`. This
remains a setup step, not code, and it remains a prerequisite — supersampling
into a virtualised desktop would still end with Windows stretching the result.

**"Intercept `GetClientRect`" — wrong lever.** The doc says real DSR requires
intercepting the size the game *queries*, naming `GetClientRect` and
`glViewport`. `GetClientRect` is a `user32` export, which a proxy `opengl32.dll`
cannot intercept without IAT patching or a second proxy DLL — the same dead end
Tier 3 feared. It does not matter, because id Tech 2-family engines do not ask
the window how big it is. They take their resolution from their own mode table
and hand it to `glViewport`. Anachronox's `settings.cfg` carries
`u_gl_mode "9"`, and the renderer sizes itself from that.

So the only lever needed is `glViewport`, which this proxy already exports and
currently forwards untouched. The whole feature stays inside the `opengl32`
surface.

**The situation this fixes, measured.** With `windowWidth=2400`,
`windowHeight=1800` and the game at 640x480, the log reports:

```
post_effects: 'nvscaler' cannot upscale beyond 2x (window 2400x1800 is more
than double the game's 640x480), so it is being skipped.
```

640x480 stretched across 2400x1800. Supersampling inverts that relationship
instead of trying to rescue it afterwards.

## Decisions

Three were taken deliberately; each had a viable alternative.

**1. The render size is an absolute target, not a multiplier.** `renderWidth` /
`renderHeight` name the resolution directly. A multiplier of the game's viewport
would keep the aspect ratio correct by construction, but the game's own mode is
small and it *moves* — both 1600x1200 and 640x480 were observed from the same
install on the same day. An absolute target means a video-mode change moves only
the scale factor, never the allocation. The cost is that a mismatched aspect
ratio becomes possible and has to be detected rather than being unrepresentable.

**2. Everything is supersampled, including the HUD.** The alternative — world at
high resolution, HUD at native, composited at the `glOrtho` boundary — would
give pixel-exact UI, and the boundary detection already exists in
`world_capture.h`. It was rejected because it forces the effect chain to run
mid-frame instead of at swap, which is a far deeper change to how the pipeline
is sequenced than this feature is worth.

This is not a regression for the HUD. Today the game's 640x480 HUD is stretched
3.75x to fill a 2400x1800 window. Rendered at 3200x2400 and downsampled to
2400x1800 it comes out sharper than it is now. The source art is the limit in
both cases.

**3. The colour attachment is `RGBA8` by default, `RGBA16F` behind
`renderFloatBuffer=1`.** Because this framebuffer belongs to us, its format is
ours to choose, and a float attachment would make the game's own fixed-function
blending accumulate at 16-bit — the "deeper back buffer" half of Tier 3, for
free, without touching pixel formats at all. It is off by default because it
genuinely changes how the game's blending accumulates, and the first version of
this feature should be byte-for-byte identical to today when it is not asked
for.

## Architecture and data flow

A new module, `render_target.h`, owns one offscreen framebuffer sized
`renderWidth` x `renderHeight`, with a colour attachment (`RGBA8`, or `RGBA16F`
under `renderFloatBuffer`) and a `DEPTH_COMPONENT24` depth attachment — the same
depth format `post_effects.cpp` already allocates for `depthTex`, so the depth
the chain reads keeps exactly the precision it has today. It becomes
"the framebuffer the game renders into", and publishes that as a value the rest
of the code asks for rather than hardcoding `0`.

Per frame, inside the `wglSwapBuffers` hook:

1. Run the effect chain exactly as today. It reads `GL_VIEWPORT`, which is now
   the supersampled size, so `ssao`, `ssr`, `taa` and `motionblur` all receive
   more pixels with **no changes of their own**.
2. Bind draw framebuffer 0.
3. `glBlitFramebuffer` the finished image down to the window's client size with
   `GL_LINEAR`. **This blit is the supersampling resolve.**
4. Call the real `SwapBuffers`.
5. Rebind the render target, so the next frame's drawing lands in it.

Between frames, `glViewport` and `glScissor` calls from the game are scaled into
render-target space before being forwarded.

**Why the downsample is last.** The effect chain must run at the supersampled
resolution, not after the resolve, because `ssao`, `ssr`, `taa`, `dof`, `fog`
and `depthvignette` all unproject the depth buffer. Downsampling depth first
would hand them a buffer in which a pixel is no longer one surface, which is
exactly the error that makes MSAA hard for this project (see Tier 3). Resolving
colour last avoids the question entirely.

## Components and interfaces

```cpp
// render_target.h

bool IsSupersampleActive();           // configured, created, and usable this frame
unsigned int GetGameFramebuffer();    // our FBO when active, else 0
unsigned int GetGameReadBuffer();     // GL_COLOR_ATTACHMENT0 when active, else GL_BACK
bool EnsureRenderTarget();            // size/context-keyed create
void BindRenderTarget();              // after the real SwapBuffers returns
void NotifyGameViewport(int x, int y, int width, int height);
void ScaleGameRect(int& x, int& y, int& width, int& height);   // pure, no GL
void ResetRenderTargetState();
```

`GetGameReadBuffer()` is not cosmetic. `glReadBuffer(GL_BACK)` is invalid
against a framebuffer object, so every capture site would begin throwing
`GL_INVALID_OPERATION` if it were left as-is.

The destination of the present blit is the WINDOW's client size, which
`post_effects.cpp` already computes as `dstWidth`/`dstHeight` in the same
function as the blit. An earlier draft of this spec listed a `GetPresentSize()`
returning the game's own reference viewport instead; that was wrong, and would
have put a finished 3200x2400 frame into a 640x480 corner of a 2400x1800 window.

`ScaleGameRect` is deliberately pure and GL-free so the entire scaling rule is
testable without a context, following `taa_jitter.h` and `texture_mipmap.h`.

The two hooks use these differently, and the difference matters: the `glViewport`
hook calls `NotifyGameViewport` first — which latches the reference size if this
is the frame's first viewport — and then `ScaleGameRect`. The `glScissor` hook
calls only `ScaleGameRect`, because a scissor rectangle is never the frame's
full-frame viewport and must not be mistaken for one.

### Config

| Key | Default | Meaning |
|---|---|---|
| `renderWidth` | 0 | Offscreen render width. Both-or-nothing with the height, the same rule `windowWidth`/`windowHeight` uses. |
| `renderHeight` | 0 | Offscreen render height. |
| `renderFloatBuffer` | 0 | `RGBA16F` colour attachment instead of `RGBA8`. |

### Call sites changed

| Where | Change |
|---|---|
| `gen_wrapper_cpp.py`, `glViewport` | `ScaleGameRect` then forward the scaled values — the same in-place pattern `glFrustum` already uses for TAA jitter |
| `gen_wrapper_cpp.py`, `glScissor` | The same scaling. Without it the HUD is clipped to a fraction of the frame |
| `gen_wrapper_cpp.py`, after the real `wglSwapBuffers` | `BindRenderTarget()` |
| `post_effects.cpp:279`, `:557`, `:585` | `GetGameFramebuffer()` / `GetGameReadBuffer()` |
| `world_capture.cpp:133` | The same |
| `post_effects.cpp:1003` | Blit to 0 with the shrink, `GL_LINEAR` |
| `post_effects.cpp:508` | See below — the black-screen trap |

## Failure handling

**The governing rule: every failure falls back to rendering into framebuffer 0,
which is exactly today's behaviour.** This feature must never be able to produce
a black screen or a clipped image.

### The atomic arming invariant

Viewport scaling and the bound framebuffer must be armed and disarmed together.
If `glViewport` is scaled to 3200x2400 while the render target is not bound, the
game renders a 3200x2400 viewport into a 2400x1800 back buffer and the player
sees the top-left corner, blown up.

This is the same class of invariant as `taa_jitter.h`'s arming rule — two pieces
of state that must agree or the image is silently wrong — and it is solved the
same way: `IsSupersampleActive()` is latched in the `wglSwapBuffers` hook, at the
same moment the render target is bound for the frame that follows, and is read
unchanged by both the viewport hook and the capture path for the whole of that
frame. It is never re-evaluated mid-frame.

One consequence, stated so it is not mistaken for a bug: the frames before the
first `wglSwapBuffers` are drawn into framebuffer 0 unsupersampled, because
nothing has bound the render target yet. That is a fraction of a second at
startup and it is the fail-safe direction.

### The black-screen trap

`post_effects.cpp:508` currently reads:

```cpp
if (config.stageCount == 0 && !hasRealUpscale) {
    return;
}
```

With supersampling active and an empty `effect=` list, that early return means
nothing ever blits the render target to the screen, and the player sees black.
It must become `&& !IsSupersampleActive()`.

This is called out explicitly because an empty `effect=` list is the natural
way to first test this feature, so the trap sits directly on the path a
developer will take.

### Failure modes

| Failure | Response |
|---|---|
| GL 4.3 bundle unavailable | Feature off, logged once — the all-or-nothing gate every other stage uses |
| `glCheckFramebufferStatus` not complete | Off for this context, status code logged |
| Size above `GL_MAX_TEXTURE_SIZE` or `GL_MAX_VIEWPORT_DIMS` | Refuse and log, rather than silently clamping to a size that was not asked for |
| `renderWidth:renderHeight` aspect differs from the game's viewport aspect | Run, warn once. Precedent at `post_effects.cpp:495` |
| `renderWidth` below the game's own viewport | Refuse and log. That is a render-scale feature, not supersampling, and silently doing something other than the name is worse than doing nothing |
| Game changes video mode mid-run | Re-latch the reference viewport on the next frame. The allocation stays at the configured absolute size, so only the scale factor moves |
| GL context change | Destroy and recreate; texture names from a dead context mean nothing |

### Interaction with the upscalers

With supersampling active, the game's native size exceeds the window, so
`hasRealUpscale` in `post_effects.cpp` goes false and any `fsr`, `nvscaler` or
`bilinear` stage in `effect=` falls back to its same-size behaviour driven by
`scale`. Supersampling supersedes the upscaler rather than composing with it.
Log this once when both are configured, so it does not look like the upscaler
broke.

### Cost

The memory is unremarkable: 3200x2400 is roughly 30 MB of colour and 23 MB of
depth at 8 bits, doubling the colour under `renderFloatBuffer`. The real cost is
that every stage in the chain now runs over 4x the pixels at a 2x linear factor.
That is the price of the feature, and it is why this is opt-in and off by
default.

## Testing

### CPU-only, no GL context

Following the split `taa_jitter.h` and `texture_mipmap.h` use — the scaling rule
is arithmetic and needs no driver.

1. A full-frame viewport scales to exactly the configured size.
2. A sub-viewport scales proportionally: game 640x480 into 3200x2400 is x5, so
   `(100,50,200,100)` becomes `(500,250,1000,500)`.
3. `glScissor` uses the identical factor.
4. The reference latch takes the **first** viewport after a swap and ignores
   later ones within the same frame.
5. A video-mode change re-latches on the following frame.
6. Inactive is a true identity: `ScaleGameRect` must not modify its arguments.
7. `renderWidth` below the game's viewport refuses.
8. Both-or-nothing on `renderWidth`/`renderHeight`.
9. **Edge-based scaling.** The factor is fractional in general — the x5 above is
   a round number only because 3200 and 640 happen to divide; a 2400-wide target
   over the same 640-wide viewport is x3.75 — so rectangles must be scaled by
   their edges, not by origin and size
   independently. Scaling `x` and `width` separately and rounding each produces
   adjacent sub-viewports that disagree by a pixel, and the player sees seams.
   The test pins `right = round(x + width) - round(x)`.

### GPU, real context, `gpu` label

1. The framebuffer is created at the configured size and reports complete.
2. With it bound, drawing lands in it and **not** in the back buffer — verified
   by reading both.
3. **The downsample averages.** Render a known 2x2 checker at 2x, downsample,
   and assert the result is the mean rather than a point sample. A `GL_NEAREST`
   blit would pass every other test here and deliver none of the benefit, so
   this is the test that proves the feature does what its name says.
4. A size beyond `GL_MAX_TEXTURE_SIZE` leaves `IsSupersampleActive()` false and
   `GetGameFramebuffer()` at 0.

### Mutation checks

Every guard gets one, per the project's standing practice: break the code a test
claims to cover and prove the test fails. The arming rule especially — a test
that still passes with the feature permanently disabled proves nothing.

### What testing cannot establish

That "the first `glViewport` after a swap is the game's full-frame viewport"
holds for a real engine. This is an empirical claim about Anachronox, not a
theorem, and it is the single most likely source of bugs in this design. A game
that sets a sub-viewport first would render wrong. It is verified by running the
game and comparing a frame dump against a known-good one, and by nothing else.

## Out of scope

- Rendering *below* the game's resolution for performance. Refused explicitly
  rather than left undefined.
- Keeping the HUD at native resolution (decision 2).
- Any change to `windowWidth`/`windowHeight`, which remains an independent
  Win32 window property.
- Intercepting `GetClientRect` or any other non-`opengl32` export.
- MSAA, which remains Tier 3's problem and is a different mechanism entirely.
