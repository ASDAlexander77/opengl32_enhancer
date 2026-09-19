# Enhancement opportunities: what else GL 4.3 can do for a GL 1.1/2.0 game

Status: exploration, not an approved design. Nothing here has been through
brainstorming or planning. Written 2026-09-18.

## The governing constraint

What this proxy can *see* is the ceiling on what it can improve. Everything
shipped so far is built on two observation points:

- the finished frame at `wglSwapBuffers` — colour and depth off framebuffer 0
- texture uploads at `glTexImage2D`

That seam has been mined hard: nineteen post-process stages, texture effects,
anisotropic filtering, a window-size override, and a frame dump. The remaining
wins mostly require **widening what is intercepted**, not adding more stages
over the same inputs.

### Interception surface today

Verified against `generators/gen_wrapper_cpp.py`:

| Entry point | What is done with it |
|---|---|
| `wglCreateContext` | window size override (`window_override.h`) |
| `wglSwapBuffers` | the whole post-effect chain (`post_effects.cpp`) |
| `glFrustum` | **recorded only**, never modified (`projection_capture.h`) |
| `glTexImage2D` | texture effects (`texture_effect.h`) |
| `glTexParameteri/f` | trilinear + anisotropy forcing (`texture_filter.h`) |
| `wglChoosePixelFormat` | exported and forwarded, **unmodified** |

Everything else in the generated wrapper is a straight pass-through.

## Tier 1 — free: colour + depth already in hand

These need no new interception and drop into the existing stage pattern
(`depth_vignette.cpp` is the closest template — it already samples the depth
attachment the pipeline blits for it).

- ~~**Depth of field.**~~ **Done** — `dof.h`, shipped 2026-09-18.
- ~~**Per-pixel distance fog.**~~ **Done** — `fog.h`, shipped 2026-09-19.
- ~~**Volumetric light shafts.**~~ **Done** — `light_shafts.h`, shipped
  2026-09-19. The predicted risk was real and is now a documented limitation
  rather than a surprise: the light is located by taking the brightness-weighted
  centroid of the frame, so a large pale surface can drag the rays off the actual
  lamp (raise `shaftsThreshold`), and a light that is off-screen cannot be found
  at all. Notably this stage needs neither depth nor a projection.
- ~~**SSR (screen-space reflections).**~~ **Done** — `ssr.h`, shipped
  2026-09-19. The heuristic gate this entry predicted would be needed is
  `ssrUpThreshold`: only surfaces whose view-space normal points far enough
  upward reflect. The design risk was real and did not go away, it was only
  bounded and written down. It is a guess about *orientation* standing in for a
  fact about *material*, so it is wrong in both directions — carpet reflects as
  readily as marble, and a wall mirror does not reflect at all. That half needs
  material information the game does not have, and still stands.

  The other half — "up" measured in view space, so pitching the camera swung the
  gate off true — was fixed on 2026-09-19, once Tier 2 landed. SSR was the first
  shipped stage capped by the missing capture and the first consumer of it; the
  gate now measures against world up. What the capture could *not* supply is
  which world axis is up, because that is an engine convention rather than
  anything a view matrix reveals, so it became `ssrWorldUpAxis` (default `z`,
  right for Quake II-family engines). A game whose view matrix is never captured
  falls back to the old view-space behaviour rather than losing reflections.

  Worth recording how the test was framed, since "the floor got brighter" would
  have passed with the gate ignored entirely: `ssr_test.cpp` changes *nothing
  but* `upThreshold` between two calls and requires the same floor pixel to go
  from reflecting to bit-exact.

**Tier 1 is now complete.** All four stages ship.

Risk: low. Cost: one stage each, same shape as the existing ones — each
completed stage took one new `.cpp`/`.h`/`_test.cpp` plus registration in
`config.h`, `config.cpp`, `config_writer.cpp`, `post_effects.cpp`,
`config_editor.cpp`, `CMakeLists.txt`, the `.ini` and the README. Note
`config_writer.cpp`: it keeps an explicit key allow-list, and forgetting it
means the editor silently drops the new settings on save.

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

What remains a guess is the "when" — and on 2026-09-19 that guess was checked in
Anachronox with `cameraLogInterval=60`, not only against Quake II's `R_SetupGL`
and a real GL driver in `modelview_capture_test.cpp`. **It holds.** Four things
in the log agree, and they are worth recording because each fails differently if
the latch picks the wrong matrix:

- `up` stays within 0.946..1.000 of world +Z across every sample, and is exactly
  `(0, 0, 1)` whenever the camera is level. Quake's world up is +Z; a transposed
  or sign-flipped basis would wander instead.
- A corridor walk logs ~570 units of travel along +Y at near-constant speed,
  decelerating to a stop, while the other two axes drift under 4 units.
- Through that walk `forward` reads `(0.05, 0.986, -0.155)` — along the
  direction of travel. Position and orientation are derived separately from the
  same matrix, so their agreement is a cross-check the log makes on itself.
- Standing still logs the position identical to the decimal for nineteen
  consecutive samples (~18 s). Jitter while stationary is the signature of
  latching something that is not the camera, and there is none.

Pitching down moved `forward.z` to -0.32 while `up.z` fell to 0.947, which only
a correct decomposition does.

That is one engine, not a proof. An engine that builds its view matrix without
`glFrustum`, or edits the modelview at depth 0 before its camera sequence, would
still defeat the heuristic. `GetCapturedCamera()` returns false in the first case
rather than guessing; the second fails silently, and is why `cameraLogInterval`
stays in the shipped config rather than being removed now the check has passed.

## Tier 3 — pixel format (blocked on a spike)

Modifying the pixel format the game receives would allow:

- **MSAA injection** — true geometry antialiasing, which `smaa` can only
  approximate from an already-aliased image.
- **Higher bit depth / float back buffer** — the `dither` stage exists to fight
  8-bit banding that a deeper buffer would not produce.

**Do not design this before running the experiment below.** Most 1.1-era games
select their pixel format through GDI's `ChoosePixelFormat`/`SetPixelFormat` in
`gdi32.dll`, not through the `wgl*` variants this proxy exports — in which case
the hook never fires and the whole tier is unreachable without hooking `gdi32`
as well.

*Experiment:* add a `printf` to the forwarded `wglChoosePixelFormat` and
`wglSetPixelFormat`, run Anachronox, and see whether either fires. Minutes of
work, and it decides whether this tier exists at all.

## Tier 4 — textures

- **Automatic mipmap generation.** `texture_filter.h` is explicit that it forces
  trilinear filtering on *the game's own mipmapped* textures. Textures uploaded
  without a mip chain are untouched and still shimmer at distance. Calling
  `glGenerateMipmap` on upload would cover them, and complements the anisotropy
  forcing already there. Probably the best effort-to-reward ratio on this list.
- **Texture upscaling at load** — 2x/4x EASU for world textures, or an xBRZ-style
  filter for sprites and UI. `textureEffect=cas` means the upload-time plumbing
  and its pitfalls (see `texture_effect.h` on mipmap-incomplete and stale-UV
  failures) are already understood.
- **sRGB correctness.** Old games upload gamma-space textures and blend in gamma
  space; the post chain then does its work on wrongly-weighted colour.

## Tier 5 — geometry and lighting

Recording `glLight*`/`glMaterial*` and the vertex submission path would allow
replacing per-vertex lighting with per-pixel lighting. Visually the largest
change available, and by far the riskiest: it means reproducing fixed-function
semantics faithfully enough that nothing regresses. Listed for completeness;
not recommended until Tier 2 exists, since it needs the same matrix capture.

## A caveat on supersampling (DSR)

Rendering above native and downsampling on present is the single biggest raw
image-quality win available to a wrapper, and it is **not** reachable through
`windowWidth`/`windowHeight`.

The game only renders larger if it believes its drawable is larger, and the
window-size override cannot help twice over: Windows clamps any decorated window
to roughly desktop size via `SM_CXMAXTRACK`/`SM_CYMAXTRACK` (see
`window_override_test.cpp`, which was changed on 2026-09-18 precisely because it
had assumed otherwise). Real DSR requires intercepting the size the game
*queries* — `GetClientRect` and `glViewport` — not the window itself. The
existing NIS spec reaches the same conclusion from the other direction: "Real
reduced-resolution rendering is future work once the wrapper can intercept the
app's actual render-target setup."

### DPI virtualisation comes first, though

Before any of that matters, check whether the game is DPI-aware. It usually is
not, and the proxy inherits whatever the host process has.

Measured on Anachronox, on a 3840x2160 display at 150% scaling: the game — and
therefore this DLL inside it — sees a virtualised 2560x1440 desktop. Every
coordinate the proxy handles is in that space, so `windowWidth`/`windowHeight`
are virtual pixels, and Windows then bitmap-stretches the finished frame by 1.5x
onto the panel. That stretch lands *after* every sharpening stage this project
runs, which makes it the single largest image-quality problem available to fix,
and no amount of FSR or CAS can compensate for it.

It cannot be fixed from inside the DLL: neither `anox.exe` nor `ref_gl.dll`
statically imports `opengl32.dll`, so the proxy is `LoadLibrary`'d long after the
game's window exists, and an existing window cannot be un-virtualised. An
external `.manifest` is ignored on modern Windows without `PreferExternalManifest`.
What does work is the per-user AppCompat layer — `HIGHDPIAWARE` under
`HKCU\...\AppCompatFlags\Layers`, the same thing the "Override high DPI scaling
behaviour: Application" checkbox writes. With it set, a requested 1920x1440
client area arrives as exactly 1920x1440 *physical* pixels.

So this is documentation and a setup step, not code — but it should be the first
thing checked whenever "the resolution setting isn't working".

## Suggested order

1. **Auto mipmap generation** (Tier 4) — smallest change, immediate benefit,
   and it completes a feature that already exists.
2. ~~**Modelview capture** (Tier 2).~~ Done — see above. The consumers it
   unlocks (SSR's world-space `up`, real TAA, motion blur, temporally
   accumulated SSAO) are now each a separate, smaller piece of work.
3. **Pixel-format spike** (Tier 3) — cheap, and answers a question that gates a
   whole tier either way.
4. ~~Tier 1 stages as appetite allows.~~ Done — all four shipped. Note that the
   last of them (`ssr`) ended up *blocked in quality* on the Tier 2 capture,
   which strengthens the case for item 2 rather than weakening it.

Tier 5 only after Tier 2 lands.
