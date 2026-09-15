# NIS-based post-process effects (BilinearUpscale, NVScaler, NVSharpen)

## Goal

Add three selectable full-screen post-process effects to the 32-bit OpenGL proxy
(`opengl32_enh32.dll`), alongside the existing pixel-invert effect, ported from
NVIDIA's Image Scaling SDK (NIS) sample sources at
`E:\Gits\NVIDIAImageScaling\samples\DX12\` and the portable core at
`E:\Gits\NVIDIAImageScaling\NIS\`:

- **BilinearUpscale** — simple bilinear resample (scale factor configurable, default 1.0/no-op resize).
- **NVScaler** — NIS's edge-adaptive resample + sharpen compute shader (`NIS_SCALER=1`).
- **NVSharpen** — NIS's lighter sharpen-only compute shader (`NIS_SCALER=0`).

All three run at native resolution by default (no real reduced-resolution
pipeline exists yet — see "Non-goals").

## Non-goals

- No control over the host app's actual render resolution. "Upscale" here
  means running NVScaler's shader (which is valid at scale 1.0, per
  `NVScalerUpdateConfig`'s `kScaleX/kScaleY` range check of `[0.5, 1.0]`), not
  a real performance-motivated upscale pipeline. Real reduced-resolution
  rendering is future work once the wrapper can intercept the app's actual
  render-target setup.
- No HDR support (`NISHDRMode::None` only).
- No fragment-shader fallback for pre-4.3 GL contexts — see "GL version
  requirement".

## Config file

`anax_enhancer.ini`, next to the DLL, INI-style `key=value` lines, read once
on the first `wglSwapBuffers` call and cached for the process lifetime:

```ini
effect=nvsharpen      ; none | invert | bilinear | nvscaler | nvsharpen
sharpness=0.5         ; 0..1, used by nvscaler/nvsharpen
scale=1.0             ; used by bilinear; nvscaler's internal resample also
                       ; reads this, clamped to [0.5, 1.0] per NIS's own limits
```

Missing file, missing keys, or out-of-range values fall back to
`effect=none` for that field and log once via the existing
`printf("[opengl32_enh_cpp] ...")` convention (see `wrapper.cpp`'s existing
resolve-failure messages for the established style).

## GL version requirement and loader

NIS's official shaders (and the bilinear resample here) are GLSL **compute**
shaders, requiring GL 4.3+ core. None of the needed entry points
(`glCreateShader`, `glDispatchCompute`, `glBindImageTexture`,
`glBlitFramebuffer`, buffer/UBO calls, etc.) are part of opengl32.dll's
static export table, so they aren't covered by the generated `wrapper.cpp`
proxy forwarding.

A new `gl_loader.h/.cpp` resolves exactly the entry points these effects
need via `wglGetProcAddress`, lazily on first use inside the
`wglSwapBuffers` hook (a context is guaranteed current at that point). If
any pointer is null, or `glGetString(GL_VERSION)` parses to below 4.3, the
whole compute-effects subsystem is disabled for the process (logged once)
and every configured effect other than `invert`/`none` behaves as `none`
(the real `wglSwapBuffers` is called untouched — never crashes or hangs the
host app).

## GPU pipeline

Per swap, for `bilinear`/`nvscaler`/`nvsharpen` (not `invert`, which keeps
its existing CPU-readback implementation in `pixel_invert.cpp` unchanged):

1. Lazily create (or resize, if the viewport size changed since last frame)
   an input texture and an output texture sized to the current viewport,
   plus one FBO per texture used as blit source/destination.
2. `glCopyTexSubImage2D` from the current back buffer directly into the
   input texture — GPU-to-GPU, no CPU readback, no `glReadPixels`/
   `glDrawPixels`/raster-position state dependency (the class of bug just
   fixed in `pixel_invert.cpp`'s invert path is structurally avoided here).
3. Bind the compute program:
   - NVScaler/NVSharpen: input texture + `coef_scale`/`coef_usm` coefficient
     textures (NVScaler only) + a UBO holding `NISConfig` (built via
     `NVScalerUpdateConfig`/`NVSharpenUpdateConfig` from vendored
     `nis_config.h`).
   - Bilinear: input texture + a small constant buffer (scale factor).
   Bind the output texture as a write image, `glDispatchCompute`,
   `glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT)`.
4. `glBlitFramebuffer` from the output-texture FBO onto `GL_BACK`.
5. Restore whatever FBO/program/active-texture-unit/texture bindings were
   current before step 1 (saved via `glGetIntegerv` at the top of the
   function), so the host app's own GL state is undisturbed.

## Shader source and NIS reuse

- `nis_config.h`: vendored, effectively unmodified copy of
  `NIS/NIS_Config.h` (the `NISConfig` struct, `NVScalerUpdateConfig`/
  `NVSharpenUpdateConfig`, and the float coefficient tables `coef_scale`/
  `coef_usm`) — pure portable C++, no DX/Vulkan dependency. Keep NVIDIA's
  MIT license header and a comment noting the source path/version
  (`NVIDIA Image Scaling SDK v1.0.3`, per the header comment in the source
  files).
- `NIS/NIS_Main.glsl` + `NIS/NIS_Scaler.h` are Vulkan-flavored GLSL
  (`layout(set=0,binding=N)`, separate `texture2D`+`sampler` opaque types,
  `GL_EXT_shader_16bit_storage`/`GL_EXT_shader_explicit_arithmetic_types`
  extensions that are unused here since `NIS_USE_HALF_PRECISION` defaults
  to 0). These are hand-ported into two self-contained compute shader
  string literals embedded in `nis_effect.cpp` (`NIS_SCALER=1` and `=0`
  variants, with `NIS_Scaler.h`'s body pre-expanded in place of the GLSL
  `#include`, since desktop GLSL has no include mechanism), targeting
  `#version 430`, plain `layout(binding=N)` (dropping Vulkan's `set=`), and
  combined `sampler2D` types.
- Bilinear is a small compute shader written from scratch (NVIDIA's sample
  only has an HLSL version) — a standard bilinear resample; scale defaults
  to 1.0 so it's a correctness/perf-neutral no-op resize by default.

## File/module layout

- `config.h/.cpp` — reads/caches `anax_enhancer.ini`
- `gl_loader.h/.cpp` — resolves the GL 4.3 function pointers via
  `wglGetProcAddress`
- `nis_config.h` — vendored `NISConfig`/coefficient tables
- `nis_effect.h/.cpp` — NVScaler + NVSharpen (shared GL resource logic,
  same shader family with a compile-time-equivalent macro toggle baked into
  which of the two embedded shader strings is compiled)
- `bilinear_upscale.h/.cpp` — Bilinear effect
- `post_effects.h/.cpp` — new `ApplySelectedEffect()` entry point,
  replacing the current unconditional `InvertBackBufferColors()` call site
  in the generated `wrapper.cpp`; reads the cached config and dispatches to
  `invert` (existing `pixel_invert.cpp`, unchanged) / `bilinear` /
  `nvscaler` / `nvsharpen` / `none`
- `generators/gen_wrapper_cpp.py` updated so the generated `wrapper.cpp`
  calls `ApplySelectedEffect()` instead of `InvertBackBufferColors()`
- `CMakeLists.txt`: new `.cpp` files added to the `WRAPPER_NAME_CXX` target
  sources (both x64 and x86 builds — see the existing arch split; per
  [[project-32bit-prototype-first]] the near-term priority is verifying
  this on the `x86` preset)

## Error handling

Every failure mode — missing/bad config, GL < 4.3, shader compile/link
error (checked via `glGetShaderiv`/`glGetProgramiv` +
`glGetShaderInfoLog`/`glGetProgramInfoLog`), or `glGetError()` after
dispatch — logs once via the existing `printf` convention and falls back to
`none` (real `wglSwapBuffers` called untouched). Never crashes or hangs the
host app.

## Testing

No unit-test framework exists in this repo; the existing validation
pattern is `wrapper_test.cpp`, a small executable that loads the built DLL
and calls a representative subset of its exports against a real context.
Extend it to also exercise `ApplySelectedEffect()` for each effect value
against a GL 4.3 context it creates itself, asserting no GL errors and no
crash. Real visual/quality validation ("does it look right") happens
manually in an actual game, per the project's established practice for UI
work.
