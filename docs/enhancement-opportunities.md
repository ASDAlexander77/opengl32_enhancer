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

The capture is recording-only. That "nothing consumes it yet" is now stale
twice over: SSR's `up` gate was rewired onto it (see Tier 1 above, the
`ssrWorldUpAxis` work), and camera motion blur (`motionblur`, shipped
2026-09-19) is the second consumer — it reprojects each pixel between this
frame's and the previous frame's captured camera. TAA still uses its raw
frame-to-frame difference heuristic rather than the capture; that rewire
remains open.

Motion blur needed one more capture beyond the camera: `world_capture.h`,
which latches a copy of the frame from before the HUD is drawn (at the first
`glOrtho` of the frame, the same discrimination `projection_capture.h` and
`modelview_capture.h` already make) so HUD/dialogue pixels can be excluded
from the blur rather than smeared into the world behind them. That latch
timing is the same kind of heuristic as the modelview capture's "when," and it
carries the same caveat: confirmed by unit test and by reading the generated
wrapper, **not yet confirmed in a running game**. Unlike the modelview
capture's Anachronox validation recorded below, nobody has yet set
`cameraLogInterval` and `effect=motionblur`, walked around, and looked at
whether the HUD stays sharp while the world smears. Doing that is what would
turn this from "unit-tested" into "confirmed," the same way the modelview
capture went from a design assumption to the four corroborating log
observations below.

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

## Tier 3 — pixel format (spike run 2026-09-20: the hook fires)

Modifying the pixel format the game receives would allow:

- **MSAA injection** — true geometry antialiasing, which `smaa` can only
  approximate from an already-aliased image.
- **Higher bit depth / float back buffer** — the `dither` stage exists to fight
  8-bit banding that a deeper buffer would not produce.

### The spike, and what it found

The worry was that most 1.1-era games select their pixel format through GDI's
`ChoosePixelFormat`/`SetPixelFormat` in `gdi32.dll`, not through the `wgl*`
variants this proxy exports, leaving the tier unreachable without hooking
`gdi32` as well. The experiment was to log the four forwarded `wgl*PixelFormat`
entry points and run Anachronox.

**Run on 2026-09-20. All four fire.** Verbatim, from the game's
`opengl32_enhancer.log`:

```
[TIER3-SPIKE] wglChoosePixelFormat #1 hdc=7F01064A
[TIER3-SPIKE]   pfd: flags=0x00000025 color=24 depth=32 stencil=0 alpha=0 accum=0 type=0
[TIER3-SPIKE] wglDescribePixelFormat #1..#16 hdc=7F01064A      (enumeration, capped by the probe)
[TIER3-SPIKE] wglSetPixelFormat #1 hdc=7F01064A index=9
[TIER3-SPIKE] wglGetPixelFormat #1..#16 hdc=7F01064A
```

So `gdi32` forwards to `opengl32` for ICD-provided formats, which is what those
exports are for, and the proxy sits in the path. What this means concretely:

- The game asks for `PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL`
  (`0x25`), 24-bit colour, 32-bit depth, **no stencil, no alpha, no accum**,
  `PFD_TYPE_RGBA`. It takes format index 9.
- The whole enumeration passes through `wglDescribePixelFormat`, so the proxy
  can both see and answer the question of what formats exist.
- The sequence happens **twice** — the engine sets a pixel format, then does it
  again on a second context. Anything that overrides the choice has to be
  idempotent across both, and a window can only have its pixel format set once,
  so the second pass is a re-created window.

**The tier is reachable. It is not, however, cheap — see below.**

### The real cost is the collision with the depth stages

`PIXELFORMATDESCRIPTOR` has no sample-count field, so MSAA injection is not a
matter of editing the struct the game passed. It means creating a dummy context,
resolving `wglChoosePixelFormatARB`, asking it for a `WGL_SAMPLES_ARB` format,
and returning that index from `wglChoosePixelFormat` instead of the one the
driver picked. Standard, but it is a context-bootstrap inside a proxy that is
itself loaded late.

The larger problem is downstream. `post_effects.cpp` captures colour and depth
from the default framebuffer with `glCopyTexSubImage2D`. Against a multisampled
default framebuffer that call is an `INVALID_OPERATION`, and the depth half has
no correct cheap fix at all: a resolved multisample depth buffer is meaningless
to `ssao`, `ssr`, `dof`, `fog`, `motionblur`, `depthvignette` and the real TAA
resolve, all of which unproject it as if each pixel were one surface. So MSAA
injection is either:

- **gated** — allowed only when no depth-reading stage is in the chain, which
  makes MSAA and TAA mutually exclusive and removes most of the reason to want
  it; or
- **plumbed** — render into our own multisample FBO, resolve colour for the
  chain, and keep a separate single-sample depth pass. That touches the one code
  path every other stage depends on.

Note also that the bit-depth half of this tier is worth less than it looks: the
post chain is already `RGBA16F` end to end. What a deeper buffer would buy is
the game's *own* 8-bit rendering, before capture, plus the final present — real,
but narrower than "the `dither` stage becomes unnecessary".

## Tier 4 — textures

- ~~**Automatic mipmap generation.**~~ Shipped on 2026-09-20 as `autoMipmap` —
  see `texture_mipmap.h`. Two things about it are worth carrying forward.

  **It is not done on upload.** The original sketch here said "calling
  `glGenerateMipmap` on upload would cover them", and that is wrong: "uploaded
  without a mip chain" is also exactly how 2D/UI/HUD/font artwork is uploaded,
  which is the category `texture_filter.h` takes pains to leave alone. Nothing
  about an upload distinguishes a wall from a health bar. The signal that does
  is the projection in force when the texture is *drawn*, so generation is lazy:
  recorded at upload, acted on at the first draw under `glFrustum`.

  **The size of the prize was measured, not assumed.** A census of `baltown`,
  tagging every texture by the pass it was drawn in:

  | mip chain | drawn in | count |
  |---|---|---|
  | has chain | never drawn (PVS-culled) | 242 |
  | has chain | world only | 31 |
  | has chain | world + 2D | 6 |
  | has chain | 2D only | 39 |
  | level 0 only | world only | 11 |
  | level 0 only | world + 2D | 1 |
  | level 0 only | 2D only | 5 |
  | level 0 only | never drawn | 191 |

  So 12 of the 49 textures drawn in the world pass had no mip chain — a real
  target set, but a small one. Running the game with the shipped feature on
  generated 15 chains, including the exact texture names the census predicted.
  Expect a subtle reduction in distant shimmer, not a dramatic change.

  Two traps, both of which caught this work in progress and will catch the next
  person:

  - **Tag on draw, never on bind.** The first census tagged at `glBindTexture`
    and reported *zero* world-only textures, because `glTexImage2D` binds its
    texture to upload it and uploads happen under the loading screen's ortho
    projection — so all 526 textures looked like 2D.
  - **A draw uses whatever is still bound.** At the top of a frame's world pass
    the leftover binding is the previous frame's HUD texture, so the rule also
    requires that the binding was established since the last projection change.

  A side finding: Anachronox sets `GL_LINEAR_MIPMAP_LINEAR` itself, so
  `texture_filter.h`'s trilinear upgrade is a no-op in this game and only its
  anisotropy half does any work.
- **Texture upscaling at load** — 2x/4x EASU for world textures, or an xBRZ-style
  filter for sprites and UI. `textureEffect=cas` means the upload-time plumbing
  and its pitfalls (see `texture_effect.h` on mipmap-incomplete and stale-UV
  failures) are already understood.
- ~~**sRGB correctness.**~~ Shipped on 2026-09-20 as `srgbCorrect` — see
  `srgb_convert.h` and
  `docs/superpowers/specs/2026-09-20-srgb-correctness-design.md`. Two things
  about the design are worth carrying forward.

  **`acestonemap` is linear-in AND linear-out.** The Narkowicz fit's output
  still needs encoding — the canonical use is `color = ACESFitted(linear);
  color = encode(color)`. That means no stage in the chain has a *different*
  input and output space, so the per-stage colour-space table collapsed to a
  single lookup rather than a pair of input/output entries. Had `acestonemap`
  genuinely changed the space, every stage would have needed two table
  entries and the chain would have needed to reason about both.

  **The resolve can't default to "whatever the chain ended in."** It always
  averages linear. The shipped `effect=` line ends `…, dither, gamma`, both
  display-space stages, so "encode only if still linear" would have put that
  exact configuration straight back into the bug this feature exists to fix.
  A chain that ends in display space is decoded once more before the resolve.

  A defect the work itself uncovered, worth flagging for the next person:
  decoding the pristine capture texture alone silently disabled `taa` and
  `motionblur`. Both compute a HUD mask as `any(abs(captureTex - worldTex) >
  1/128)`, and `worldTex` was left undecoded — so with one operand converted
  and the other not, nearly every non-black pixel read as HUD and both stages
  became frame-wide no-ops. The fix was to convert neither texture: they are
  only ever compared against each other, never read as light.

  Not yet validated in-game — see the design doc's status line and
  `README.md`'s sRGB correctness section.

## Tier 5 — geometry and lighting

Recording `glLight*`/`glMaterial*` and the vertex submission path would allow
replacing per-vertex lighting with per-pixel lighting. Visually the largest
change available, and by far the riskiest: it means reproducing fixed-function
semantics faithfully enough that nothing regresses. Listed for completeness;
not recommended until Tier 2 exists, since it needs the same matrix capture.

## Supersampling (DSR) — shipped 2026-09-20

Rendering above native and downsampling on present is the single biggest raw
image-quality win available to a wrapper, and it is **not** reachable through
`windowWidth`/`windowHeight`: that setting makes the *window* bigger, which
makes the game's own image get stretched, not resampled.

It ships as `renderWidth`/`renderHeight` (both 0 = off), with `renderFloatBuffer`
for an RGBA16F colour attachment. See `render_target.h` and
`docs/superpowers/specs/2026-09-20-supersampling-design.md`.

**One thing this document got wrong, corrected here.** It said real DSR requires
intercepting the size the game *queries* — naming `GetClientRect` and
`glViewport`. `GetClientRect` is a `user32` export a proxy `opengl32.dll` cannot
reach, and it does not matter: id Tech 2-family engines take their resolution
from their own mode table and hand it to `glViewport`, never asking the window.
`glViewport` alone was the lever, and the whole feature stayed inside the
`opengl32` surface.

**What the in-game run established** (Anachronox, `baltown`, game at 640x480,
`renderWidth`/`renderHeight` 1280x960, the full twelve-stage `effect=` chain
including `fsr`):

- The target allocates and the log says so.
- The world pass runs: `glFrustum` entered, sixteen auto-mipmap chains generated.
- No GL errors in steady state, and none of the failure modes the design feared.
- The upscaler-superseded notice fires, confirming `fsr` and supersampling
  coexist without either silently breaking.
- Exactly **one** frame goes wrong, at the moment the game creates its second GL
  context: the capture runs once against a framebuffer belonging to the dead
  context and returns `GL_INVALID_OPERATION`. This was read at the time as the
  fail-safe behaving correctly. It was not — see the correction below.
- It is noticeably slower to reach gameplay, which is expected: at 2x on each
  axis every stage in the chain does four times the work, and the loading screen
  renders frames too.

**What it did NOT establish: that the image looks right.** No screenshot
comparison was made and no human confirmed the result by eye. The feature is
verified to run without error, not verified to look correct.

### Corrections found by review, fixed 2026-09-20

A review of the merged branch found five defects. Three are worth carrying
forward, because each is a case of a test that looked like it covered something
and did not.

**The resolve was a single bilinear tap, correct only at exactly 2x.** A
shrinking `GL_LINEAR` `glBlitFramebuffer` reads four source pixels regardless of
how many were rendered. At 2x that is the whole box and the frame resolves
correctly; at 4x it is 4 of 16 and at 5x, 4 of 25 — so above 2x the image still
aliased while costing the full fill rate. Every test in the suite used a 2x
ratio, which is the one ratio at which the bug is invisible, and the GPU test
named "the downsample averages rather than point-samples" used a two-tone seam
at the centre — the exact place where a tap and a box agree. The fix halves
exactly as many times as it can before the final blit (each exact halving *is* a
2x2 box), and the new test paints every fourth column white at 4x, where a box
average returns 64 and a single tap returns 0. Measured before the fix: 0.
After: 64, flat across the row.

**The supersample latch outlived the GL context that took it.** The latch is a
statement about a framebuffer name, and names are per-context. On a
`vid_restart` the frame *after* the change was still armed, so its viewports
were scaled up while the binding — belonging to the dead context — had not
travelled, leaving the game drawing a blown-up corner into the new context's
framebuffer 0. The in-game run above saw this as "one dropped frame" and the
`GL_INVALID_OPERATION` it logged; that reading was wrong. The latch now expires
with the context generation, which is the same rule every other cached
GL-derived value in this DLL already follows.

**Two diagnostics could never fire.** The aspect-ratio warning and the
below-native refusal both lived in `EnsureRenderTarget`, which runs immediately
after `NotifyFrameBoundary` on the first frame — before any viewport has been
recorded — and returns at its fast path on every frame after. Neither had the
game's size available at the only moment it executed. They have moved to the
arming path, which is the one place that holds both numbers. The refusal in
particular was a promise the ini and README both made and the code kept
silently: a `renderWidth` below the game's own viewport allocated a target,
announced "supersampling into 320x240", then declined to arm forever without
saying why.

**Not fixed: no stencil attachment.** The target carries colour and depth only
and never consults the game's pixel format, so in a game that uses stencil every
test passes — shadow masks and stencil-masked mirrors draw over everything.
Fixing it means moving the whole depth-consuming half of the post chain to a
packed `DEPTH24_STENCIL8`, because a depth blit between mismatched formats is
`GL_INVALID_OPERATION`. That is a real regression risk across every depth stage
traded against a fault no available testbed can reproduce — Anachronox requests
`stencil=0`, which is why this shipped unnoticed. Documented in `render_target.h`
and the ini rather than guessed at.

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

1. ~~**Auto mipmap generation** (Tier 4).~~ Shipped on 2026-09-20 as
   `autoMipmap` — see above, including why the "generate on upload" shape this
   list originally proposed would have blurred the HUD.
2. ~~**Modelview capture** (Tier 2).~~ Done — see above. Two of the consumers
   it unlocks now ship (SSR's world-space `up`, camera `motionblur`); real
   TAA and temporally accumulated SSAO remain separate, smaller pieces of
   work.
3. ~~**Pixel-format spike** (Tier 3).~~ Done on 2026-09-20 — the hook fires, so
   the tier exists. It is still not the next thing to build: MSAA injection
   fights every depth-reading stage (see Tier 3 above), so it costs far more
   than its position on this list suggests.
4. ~~Tier 1 stages as appetite allows.~~ Done — all four shipped. Note that the
   last of them (`ssr`) ended up *blocked in quality* on the Tier 2 capture,
   which strengthens the case for item 2 rather than weakening it.
5. ~~**sRGB correctness** (Tier 4).~~ Shipped on 2026-09-20 as `srgbCorrect` —
   see above. The last of Tier 4's open items apart from texture upscaling,
   which remains open.

Tier 5 only after Tier 2 lands.
