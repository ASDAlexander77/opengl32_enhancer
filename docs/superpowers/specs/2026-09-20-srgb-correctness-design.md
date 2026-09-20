# sRGB correctness for the post chain — design

**Date:** 2026-09-20
**Status:** approved, ready for an implementation plan
**Feature name in config:** `srgbCorrect`

## What this builds

The post-effect chain currently does all of its arithmetic on sRGB-encoded
values while treating them as though they were light. This feature decodes the
captured frame to linear light once, runs the chain there, and encodes back to
sRGB once on present — so that every stage that averages, blurs, thresholds or
tone-maps is weighting light rather than weighting an encoding of it.

It is Tier 4's "sRGB correctness" item in `docs/enhancement-opportunities.md`.

## Why this is wrong today, concretely

sRGB is a roughly 2.2-power encoding. Averaging two encoded values does not give
the encoding of their average. The error is largest exactly where these stages
do their work:

- `bloom` thresholds and blurs. A blur is an average, so bright highlights
  bleed too weakly and the threshold does not mean the luminance it claims.
- `acestonemap` is a curve fitted to **linear scene light**. Fed sRGB-encoded
  values it is not a tone-mapping operator at all, just an arbitrary curve.
- `lightshafts` accumulates radially — again an average.
- `nr`, `localcontrast`, `dof`, `motionblur`, `taa`, `smaa` all filter, and a
  filter is a weighted average.
- The supersample resolve. Its halvings average pixels, so black against white
  resolves to 128 where correct linear averaging gives **188**. This is the
  largest single visible instance of the bug and it sits in the feature that
  exists specifically to average pixels well.

## Why now

Two things that would have made this expensive are already paid for:

**The pipeline is already `RGBA16F`.** `CreatePipelineTexture` in
`post_effects.cpp` allocates half-float, so linear intermediates need no format
change and there is no banding risk in the darks — the usual reason this
conversion is avoided in an 8-bit pipeline does not apply here.

**`renderFloatBuffer` already exists.** The supersampling work added an
`RGBA16F` colour attachment option, so the game's own blending can already
accumulate at float precision when asked.

## Decisions

Four were taken deliberately; each had a viable alternative.

**1. Bracket the whole chain rather than fixing stages individually.** Decode
once after capture, encode once at present. The alternative — converting inside
only the stages where the error is largest (`bloom`, `acestonemap`,
`lightshafts`) — keeps the blast radius small, but it duplicates the conversion
per stage, leaves stages handing each other gamma-space values, and does
nothing for the resolve. Bracketing makes the whole chain correct for two small
passes.

**2. A per-stage colour-space table, not a positional rule.** Each `EffectKind`
declares the space it wants to be handed, in one table beside `StageNeedsDepth`.
The chain tracks the image's current space and inserts a conversion only when
the next stage disagrees.

The rejected alternative was to let `acestonemap` be the transition point —
everything before it linear, everything after display-referred — which is how a
real renderer is built. It fails here because `acestonemap` is optional and
user-ordered: a chain without it, or with it listed first, has no defined
transition and the rule would silently do nothing.

**3. `acestonemap` is linear-in, linear-out.** This was the decision most likely
to go the other way, and getting it right collapses the design. The Narkowicz
fit maps linear light into a display-referred *range* whose values still have to
be encoded afterwards — the canonical use is `color = ACESFitted(linear); color
= encode(color);`. So its output is still "linear, not yet encoded".

The consequence is that **no stage has a different input and output space**, so
the table is a plain one-entry-per-stage lookup rather than a pair. Had
`acestonemap` genuinely changed the space, every stage would have needed two
entries and the chain would have needed to reason about both.

**4. The resolve always averages linear, and the encode runs after it.** The
present path resolves by halving (see `ResolveHalvingSteps`), and those halvings
are exactly the averages this feature exists to correct. Encoding first would
leave the downsample averaging in gamma space — fixing the chain while leaving
the largest instance of the bug in place.

This means the resolve's space cannot be left to whatever the chain happened to
end in. The shipped `effect=` line ends `… dither, gamma`, both of which are
`Display` stages, so "encode only if still linear" would have put the common
configuration straight back into the bug. A chain that ends in display space is
decoded once more before the resolve.

The cost is one extra conversion for supersampled chains ending in a
display-space stage, and that the encode pass runs on the resolved-size image
rather than at a fixed point in the chain. That extra decode is skipped when
there is no resolve to feed — see the table in the data-flow section.

A note on `dither` specifically: run before the resolve, its noise is averaged
by the downsample and largely destroyed. That is **already true today** —
`dither` has always run at render resolution with the resolve after it — so it
is a pre-existing property of combining `dither` with supersampling, not
something this feature introduces. It is called out here because the extra
decode makes it look like a new consequence when it is not.

## Which stages want display space

Three of the twenty-five:

| Stage | Space | Why |
|---|---|---|
| `lutgrading` | Display | `.cube` LUTs are authored against display-referred input. Feeding a linear value into a LUT indexed by encoded values reads the wrong cell. |
| `dither` | Display | It exists to hide the quantisation of the final 8-bit write. Dithering linear values perturbs the wrong quantity. |
| `gamma` | Display | An output correction — the "too dark on my monitor" control. It should act on what is about to be shown. |

Every other stage is `Linear`, including `acestonemap` (decision 3 above).

## Architecture and data flow

A new module, `srgb_convert.h`, owns two single-pass compute stages modelled on
`pixel_invert.h` — the simplest existing stage, and the right shape to copy
since these are pure per-pixel transforms with no neighbourhood and no config:

```
ApplySrgbDecode(src, dst, width, height)   // sRGB-encoded -> linear light
ApplySrgbEncode(src, dst, width, height)   // linear light -> sRGB-encoded
```

Both use the **exact piecewise sRGB transfer function**, not a `pow(2.2)`
approximation. The whole feature is a correctness claim, and the linear segment
near black is where a `pow` approximation is most wrong.

Per frame, with `srgbCorrect=1`:

```
capture into pair[0]          (sRGB-encoded, from the game's framebuffer)
decode pair[0]                -> space = Linear
decode captureTex             (see below)
for each stage in effect=:
    if ColorSpaceFor(stage) != space:
        insert a conversion pass; space = ColorSpaceFor(stage)
    run the stage
if halvings > 0 and space == Display: decode   -> the resolve averages linear
resolve by halving
if space == Linear: encode
final blit to framebuffer 0
```

The decode before the resolve is conditional on there actually being halvings to
do. With no resolve nothing is averaged, so the space no longer matters and a
chain ending in a display-space stage is already in the space the 8-bit write
wants — forcing a decode there would just be an encode's inverse, two passes
that cancel. The four combinations:

| supersampled | chain ends | what happens |
|---|---|---|
| yes | `Linear` | resolve, then encode |
| yes | `Display` | decode, resolve, then encode |
| no | `Linear` | encode |
| no | `Display` | nothing — already display-referred |

With `srgbCorrect=0` not one of those conversions is generated and the frame
takes exactly the path it takes today.

### `captureTex` is decoded too

`captureTex` holds the pristine, unmodified frame and is filled directly from
the game's framebuffer, so it arrives sRGB-encoded. Its two consumers — `taa`
and `motionblur` — are both `Linear` stages and receive it alongside pipeline
textures. Without the same decode they would compare a linear image against an
encoded one, which is a worse error than the one this feature removes. It gets
the same decode pass, at the same point.

### TAA's private history

`taa.cpp` keeps its own history texture holding its previous output, which is in
whatever space the chain ran in. That is self-consistent frame to frame, so no
conversion is needed. On the single frame where `srgbCorrect` is toggled the
history is stale in the wrong space; TAA's existing `historyValid` and
`rawDiff` rejection already handle a discontinuous change, and one frame at a
config change is the same class of event as a video-mode change.

### Depth is untouched

`depthTex` is not colour. `ssao`, `ssr`, `dof`, `fog` and `depthvignette` read
it unchanged.

## Config

One new key, `srgbCorrect`, `0`/`1`, default `0`.

Default off because turning it on changes every image the chain produces:
`bloomThreshold`, `acesStrength` and every tuned intensity stop meaning what
they meant. That matches the rule the rest of this project already follows —
`renderFloatBuffer` is off by default for exactly this reason, and every stage
default is a documented no-op.

Left **unmanaged** in `config_writer.cpp`'s `kManagedKeys`, the same precedent
`renderWidth`/`renderHeight` and `windowWidth`/`windowHeight` set: the config
editor does not expose it, so the writer has no business writing it back.

## Testing

### CPU-only, no GL context

- `ColorSpaceFor` pinned as a table over **all twenty-five** `EffectKind`
  values, not just the three interesting ones. A stage added later without a
  considered space is the failure this catches.

### GPU, real context, `gpu` label

- **Known-value round trip.** sRGB `0.5` decodes to linear `0.2140`, and that
  encodes back to `0.5`. Pins the transfer function itself against a value a
  `pow(2.2)` approximation gets visibly wrong.

- **Identity through the bracket — the safety property.** A chain of
  `invert, invert` (a `Linear` no-op pair) with `srgbCorrect=1` must return the
  input image. This proves decode-then-encode is an identity around a real
  stage. Note it must *not* be tested with an empty chain: `ShouldSkipEffectChain`
  returns early there, so no conversion runs and the test would pass trivially
  without proving anything.

- **Identity through a mid-chain conversion.** `gamma` at `gamma=1`,
  `brightness=1` is documented to reproduce its input exactly. As a `Display`
  stage it forces an encode before it and leaves the image in display space, so
  this exercises the conversion the table inserts rather than the bracket's.
  Unsupersampled, it is also the fourth row of the data-flow table — the case
  where the present path correctly does nothing at all.

  Both identity tests assert to **±1 per channel, not byte-exact**. The round
  trip passes through `RGBA16F`, whose ten-bit mantissa can land a value on the
  wrong side of an 8-bit rounding boundary. Claiming byte-exactness would be
  claiming something the format cannot deliver.

- **The resolve averages linear.** A black/white pattern supersampled 2x with
  `srgbCorrect=1` comes back at **188**, not 128. This is the one test that
  proves the ordering decision (decision 4) and the only one that would survive
  someone moving the encode before the resolve.

### Mutation checks

Each of the above must be shown to fail against a deliberate break, per this
project's standard: passing alone proves nothing.

- Replace the piecewise transfer function with `pow(x, 2.2)` — the known-value
  test must fail.
- Move the encode before the resolve — the 188 test must fail and nothing else
  should.
- Drop the decode that precedes the resolve for display-ending chains — the 188
  test run with `dither` or `gamma` last must fail. Without a display-ending
  variant of that test this mutation survives, which is precisely the shipped
  `effect=` line's shape.
- Give `lutgrading` the `Linear` space — the table test must fail.
- Skip the `captureTex` decode — needs a `motionblur`/`taa` case to be lethal;
  if no assertion catches it, say so rather than claiming coverage.

### What testing cannot establish

That the result looks better. Every assertion here is arithmetic. Whether a
linear chain is an improvement on a game whose own art was authored against the
gamma-space look is a human judgement, and this project still owes that
validation for supersampling and real TAA as well.

## Out of scope

**The game's own rendering stays gamma-space.** The game uploads gamma-encoded
textures and its fixed-function blending combines them in gamma space; this
feature corrects the post chain only, from the captured frame onward. Making the
game's own blending linear would mean sRGB texture views plus
`GL_FRAMEBUFFER_SRGB` on a framebuffer the game believes is ordinary, which
double-corrects everything the game composites itself — including the HUD — and
is a different and much riskier feature.

**Texture upscaling at load** remains Tier 4's other open item and is untouched
here.

**Flipping the default.** Whether `srgbCorrect` should eventually default on is
a real question, but it is gated on a human comparing screenshots, which has not
happened for any feature in this project yet.
