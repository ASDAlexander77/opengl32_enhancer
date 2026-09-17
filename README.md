# Opengl32 Enhancer

A drop-in `opengl32.dll` proxy that adds a configurable GPU post-processing
pipeline to any OpenGL game or application, with no source access or game
modification required.

It works the same way ReShade/ENB-style wrappers do: the DLL exports every
real `opengl32.dll` function (forwarding each call to the system OpenGL
library), and hooks `wglSwapBuffers` to run a chain of post-process effects
on the rendered frame right before it's presented.

| Enhancement off | Enhancement on |
| --- | --- |
| ![Enhancement off](docs/imgs/enh_off.jpg) | ![Enhancement on](docs/imgs/enh_on.jpg) |

## Effects

The pipeline is a comma-separated list of stages in `opengl32_enhancer.ini`,
run left to right in the order listed. Available stages:

| Stage | Effect |
| --- | --- |
| `invert` | debug/demo - inverts the image |
| `bilinear` | bilinear rescale (honors `scale`) |
| `nvscaler` | NVIDIA Image Scaling upscaler (honors `scale`) |
| `bloom` | glow on bright highlights |
| `acestonemap` | ACES filmic tone-mapping curve |
| `lutgrading` | 3D-LUT color grading from a `.cube` file |
| `vignette` | darkens the corners |
| `chromaticaberration` | lens-style edge fringing |
| `taa` | temporal anti-aliasing |
| `smaa` | spatial anti-aliasing (SMAA) |
| `fsr` | AMD FidelityFX Super Resolution 1, EASU + RCAS (honors `scale`) |
| `cas` | AMD FidelityFX Contrast Adaptive Sharpening (sharpen-only) |
| `sharpen` | NVIDIA Image Scaling adaptive sharpen |
| `dither` | ordered dither, masks 8-bit banding |

Each stage has its own tunable parameters (thresholds, intensity, strength,
etc.) documented inline in `opengl32_enhancer.ini`. A stage only runs if it's
named in the `effect=` line; leaving it out (or setting `effect=none`) turns
it off.

A `cyberpunk.cube` LUT (teal-tinted shadows, magenta/pink highlights, boosted
contrast/saturation) ships as the default `lutgrading` look.

### `taa` vs `smaa`

Both are anti-aliasing, but they fail in opposite situations, so pick based on
which failure mode matters more for a given game - don't list both together,
since they solve the same problem and stacking them is wasted work, not
better antialiasing:

- `taa` blends the current frame with history, with no access to the game's
  own motion vectors (this proxy has no way to get them) - it works well on
  static or slow-moving scenes, but can ghost or fail to smooth edges during
  camera movement or fast motion, exactly when anti-aliasing matters most.
- `smaa` looks at a single frame only (edge detection + morphological
  reconstruction, hand-ported from the
  [reference SMAA implementation](https://github.com/iryoku/smaa)), so it has
  no motion-related artifacts at all and stays consistent during camera
  movement - at the cost of being a purely spatial technique, generally
  blurrier than a true motion-vector TAA would be on fine static detail.

### Picking a sharpener

Three stages sharpen, and stacking them just double-sharpens - pick one:

- `sharpen` (NVIDIA Image Scaling's NVSharpen) is the heaviest: a full
  USM-style filter with its own coefficient tables and a large neighborhood.
- `fsr`'s RCAS is limiter-driven - it refuses to sharpen anywhere doing so
  would clip, which is very safe but means it declines to touch
  already-saturated edges at all.
- `cas` is the lightest: one 3x3 neighborhood and a simple cross filter. It
  sharpens more uniformly across the image than RCAS and costs less than
  NVSharpen.

## Texture effects

Independent of the `effect=` pipeline above (which only touches the final
composited frame), `textureEffect=` in `opengl32_enhancer.ini` runs the
game's own textures in place through one stage as they're uploaded. Only
small, uncompressed `GL_RGBA`/`GL_UNSIGNED_BYTE` uploads are handled (no
S3TC/BC decode), and dimensions are never changed - only pixel content is.

- `sharpen` reuses the same NVIDIA Image Scaling adaptive-sharpen pass as
  the `sharpen` stage, at the `sharpness` value. Enabled by default.
- `cas` reuses the `cas` stage's Contrast Adaptive Sharpening, also at the
  `sharpness` value. Lighter than `sharpen`, which matters here because this
  runs on every eligible texture at load time.
- `invert` reuses the same debug/demo invert pass as the `invert` stage -
  handy for spotting which draws touch which textures.
- `none` disables texture-level processing.

`fsr` is deliberately not accepted here: it keeps a scratch texture sized to
its input and uploads arrive at many different sizes, so it would reallocate
on nearly every texture, and its EASU pass is a near no-op at identical
dimensions. Use `cas` instead.

(Older config files may still say `textureSharpen=1`/`0` - it's read as an
alias for `textureEffect=sharpen`/`none`.)

## Installing into a game

1. Grab `opengl32.dll`, `opengl32_enhancer.ini`, and `cyberpunk.cube` from the
   [Actions build artifact](../../actions) (or build them yourself - see
   below).
2. Copy all three files into the game's folder - the same directory as the
   game's `.exe`, **not** `system32`.
3. Edit `opengl32_enhancer.ini` to enable/tune the effects you want.
4. Launch the game normally. It will load your `opengl32.dll` instead of the
   system one, which loads the real system OpenGL library (via
   `GetSystemDirectoryA`) underneath and layers the effects on top.

Only works with games that render via OpenGL (`opengl32.dll`) - it has no
effect on Direct3D or Vulkan titles. To uninstall, just delete the three
files.

## Building

Windows + MSVC only, x86 (32-bit) target - `wglSwapBuffers` interception
targets 32-bit OpenGL games, which is what most of them are.

```sh
cmake --preset x86-release
cmake --build --preset x86-release --target opengl32_enh32
```

The resulting `build-x86-release/opengl32_enh32.dll` is what gets renamed to
`opengl32.dll` for installation. See `.github/workflows/build.yml` for a
from-scratch build (no local VS install assumptions) that produces the same
three-file install archive as a CI artifact.

### Tests

Most stages have a GPU-backed test (`*_test.cpp`) that creates a real GL
context and exercises the effect's compute/shader pipeline directly - build
and run the corresponding `*_test` target. `post_effects_test` is an
integration test that runs the actual `opengl32_enhancer.ini` shipped in this
repo end-to-end and dumps before/after `.ppm` images for a human to eyeball.
