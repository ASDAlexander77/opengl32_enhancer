# Opengl32 Enhancer

A drop-in `opengl32.dll` proxy that adds a configurable GPU post-processing
pipeline to any OpenGL game or application, with no source access or game
modification required.

It works the same way ReShade/ENB-style wrappers do: the DLL exports every
real `opengl32.dll` function (forwarding each call to the system OpenGL
library), and hooks `wglSwapBuffers` to run a chain of post-process effects
on the rendered frame right before it's presented.

## Effects

The pipeline is a comma-separated list of stages in `opengl32_enhancer.ini`,
run left to right in the order listed. Available stages:

| Stage | Effect |
| --- | --- |
| `invert` | debug/demo - inverts the image |
| `bilinear` | bilinear rescale |
| `nvscaler` | NVIDIA Image Scaling upscaler |
| `bloom` | glow on bright highlights |
| `acestonemap` | ACES filmic tone-mapping curve |
| `lutgrading` | 3D-LUT color grading from a `.cube` file |
| `vignette` | darkens the corners |
| `chromaticaberration` | lens-style edge fringing |
| `taa` | temporal anti-aliasing |
| `sharpen` | NVIDIA Image Scaling adaptive sharpen |
| `dither` | ordered dither, masks 8-bit banding |

Each stage has its own tunable parameters (thresholds, intensity, strength,
etc.) documented inline in `opengl32_enhancer.ini`. A stage only runs if it's
named in the `effect=` line; leaving it out (or setting `effect=none`) turns
it off.

A `cyberpunk.cube` LUT (teal-tinted shadows, magenta/pink highlights, boosted
contrast/saturation) ships as the default `lutgrading` look.

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
