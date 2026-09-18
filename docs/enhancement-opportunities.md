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

- **SSR (screen-space reflections).** Depth, colour, and normals reconstructed
  from depth — `ssao.cpp` already does that reconstruction. 1.1-era surfaces are
  uniformly flat-shaded, so this is a large, visible change.
- **Depth of field.** Focus distance sampled from depth near screen centre.
- **Volumetric light shafts.** Radial blur from bright pixels, masked by depth.
- **Per-pixel distance fog.** Old engines fog per vertex; real depth plus the
  captured `zNear`/`zFar` gives correct per-pixel fog, and can equally *remove*
  an engine's fog banding.

Risk: low. Cost: one stage each, same shape as the existing ones.

## Tier 2 — capture the modelview matrix (recommended first)

`taa.h` states the limitation directly: *"this proxy has no access to the app's
per-object motion vectors, unlike a real engine's TAA"*. That one missing input
is what caps several shipped stages at once — the neighbourhood-clamp and
raw-difference heuristics in `taa.cpp` exist entirely to work around it.

`glFrustum` is already recorded. Recording `glLoadMatrixf`, `glMultMatrixf`,
`glLoadIdentity`, `glRotatef`, `glTranslatef` and `glScalef` as well would let
the proxy reconstruct the **camera** matrix each frame, and camera motion
vectors follow by reprojection. That unlocks:

- real TAA instead of TAA-lite
- motion blur, currently impossible
- temporally accumulated (much quieter) SSAO
- the groundwork for frame generation

Why this one first: it is additive and recording-only, exactly like
`projection_capture.cpp`, so it cannot change what the game renders; and it
improves stages that already ship rather than adding new surface area.

Known hazard: the same one `projection_capture.h` documents for `glFrustum` —
id Tech 2-era engines draw the HUD last under an orthographic projection, so
"the most recent matrix at swap time" is the HUD's, not the world's. The
heuristic that solved it there (record at the point the engine establishes the
3D view, ignore the 2D pass) will need an equivalent here, and it is the part
most likely to be engine-specific.

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

## Suggested order

1. **Auto mipmap generation** (Tier 4) — smallest change, immediate benefit,
   and it completes a feature that already exists.
2. **Modelview capture** (Tier 2) — unlocks the most, changes nothing on its own.
3. **Pixel-format spike** (Tier 3) — cheap, and answers a question that gates a
   whole tier either way.
4. Tier 1 stages as appetite allows; they are independent of everything above.

Tier 5 only after Tier 2 lands.
