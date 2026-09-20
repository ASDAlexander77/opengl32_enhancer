# sRGB Correctness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the post-effect chain on linear light instead of sRGB-encoded values, so every stage that averages, blurs, thresholds or tone-maps weights light rather than an encoding of it.

**Architecture:** Decode the captured frame to linear once, run the chain there, encode back to sRGB once on present. A per-stage colour-space table tells the chain which of the three display-space stages need converting back mid-chain. The pipeline is already `RGBA16F`, so nothing changes format.

**Tech Stack:** C++ (MSVC, x86), OpenGL 4.3 compute shaders, CMake + Ninja, CTest. No new dependencies.

**Spec:** `docs/superpowers/specs/2026-09-20-srgb-correctness-design.md`

## Global Constraints

- **Build:** `cmake --build --preset x86`. Tests: `ctest --test-dir build-x86`. All 41 existing tests must stay green after every task.
- **Default off.** `srgbCorrect` defaults to `0`. With it off, the frame must take byte-for-byte the path it takes today — no conversion pass may be generated, and no existing test's output may change.
- **Exact piecewise sRGB transfer function**, never `pow(x, 2.2)`. Decode: `s <= 0.04045 ? s/12.92 : pow((s+0.055)/1.055, 2.4)`. Encode: `l <= 0.0031308 ? l*12.92 : 1.055*pow(l, 1.0/2.4) - 0.055`.
- **Mutation-check every test.** A passing test proves nothing until you have broken the code it claims to cover and watched it fail. Each task names its mutation explicitly; if a mutation survives, say so rather than claiming coverage.
- **GL objects are per-context.** Every module caching a program must compare `GetGlContextGeneration()` and rebuild when it differs — copy the pattern in `pixel_invert.cpp`.
- **Stage contract.** A stage reads `srcTexture`, writes `dstTexture`, both `width x height` `RGBA16F` owned by the caller. It does no capture, no blit and no state save/restore. It returns `true` only if it actually wrote `dstTexture`.
- **Test output goes to the debug log, not the console.** `ApplySelectedEffect` calls `RedirectStdoutToDebugLog()`, so any `printf` after the first call to it lands in `build-x86/opengl32_enhancer.log`. Read results from there, not from the terminal.

---

### Task 1: The `srgbCorrect` config key

**Files:**
- Modify: `config.h` (add the field beside `renderFloatBuffer`, ~line 205)
- Modify: `config.cpp` (add the parse branch beside `renderFloatBuffer`, ~line 566)
- Modify: `opengl32_enhancer.ini` (document the key)
- Test: `config_test.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `GetAnaxConfig().srgbCorrect` — a `bool`, default `false`. Every later task reads this.

- [ ] **Step 1: Write the failing test**

`config_test.cpp` has no test framework — it writes a fixture ini, parses it, and
calls a `void Check(bool, const char*)`. There is no existing `renderFloatBuffer`
coverage to copy, so this is written from the file's own idiom (see the
`effect=nvscaler` block around line 78). Add a new block in `main`:

```cpp
    // srgbCorrect is the opt-in for running the chain on linear light.
    {
        WriteFixture("config_test_srgb.ini", "srgbCorrect=1\n");
        AnaxConfig config = ParseConfigFile("config_test_srgb.ini");
        Check(config.srgbCorrect, "srgbCorrect=1 turns linear-light processing on");

        WriteFixture("config_test_srgb_off.ini", "srgbCorrect=0\n");
        AnaxConfig off = ParseConfigFile("config_test_srgb_off.ini");
        Check(!off.srgbCorrect, "srgbCorrect=0 turns it off");
    }
```

And add one line to the existing "missing file falls back to defaults" block near
line 60, which is where this file pins every default:

```cpp
        Check(!config.srgbCorrect, "missing file falls back to default srgbCorrect (false)");
```

That default assertion is the entire backwards-compatibility guarantee for this
feature, which is why it belongs with the others rather than in the new block.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build --preset x86 --target config_test`
Expected: FAIL to compile — `srgbCorrect` is not a member of `AnaxConfig`.

- [ ] **Step 3: Add the field**

In `config.h`, directly after `renderFloatBuffer`:

```cpp
    // Runs the whole effect= chain on linear light instead of sRGB-encoded values: the frame
    // is decoded once after capture and encoded once on present. Every stage that averages,
    // blurs, thresholds or tone-maps is weighting light rather than an encoding of it -
    // including the supersample resolve, where black against white resolves to 188 rather
    // than 128. See docs/superpowers/specs/2026-09-20-srgb-correctness-design.md.
    //
    // Off by default because turning it on changes every image the chain produces:
    // bloomThreshold, acesStrength and every tuned intensity stop meaning what they meant.
    // Same reasoning as renderFloatBuffer above.
    bool srgbCorrect = false;
```

- [ ] **Step 4: Add the parse branch**

In `config.cpp`, directly after the `renderFloatBuffer` branch:

```cpp
        } else if (strcmp(key, "srgbCorrect") == 0) {
            config.srgbCorrect = ParseBool(value, config.srgbCorrect, "srgbCorrect");
```

- [ ] **Step 5: Add it to the config summary line**

`config.cpp` prints a one-line summary of every key when the ini loads — the
`effect=…; scale=…; acesStrength=…` line visible in `opengl32_enhancer.log`. Find
`renderFloatBuffer` in that `printf`'s format string and argument list and add
`srgbCorrect=%s` beside it, formatted the same way the other booleans there are.

This is not cosmetic: the in-game verification in Task 6 reads that line to confirm
the feature is actually on, and a feature that cannot be seen in the log cannot be
diagnosed from a user's log either.

- [ ] **Step 6: Document the key in the ini**

In `opengl32_enhancer.ini`, after the `renderFloatBuffer` block:

```ini
; srgbCorrect is 0/1 (0=off, the default). Runs the whole effect= chain on LINEAR LIGHT rather
; than on sRGB-encoded values. sRGB is roughly a 2.2-power encoding, and averaging two encoded
; values does not give the encoding of their average - so with this off, every stage that
; averages is weighting the encoding instead of the light:
;
;   - bloom's threshold does not mean the luminance it claims, and its blur bleeds too weakly
;   - acestonemap is a curve fitted to LINEAR scene light; fed encoded values it is just an
;     arbitrary curve, not a tone-mapping operator
;   - lightshafts accumulates radially, and nr/localcontrast/dof/motionblur/taa/smaa all filter
;   - the renderWidth/renderHeight downsample averages pixels, so black against white resolves
;     to 128 where correct linear averaging gives 188
;
; Off by default because it changes every image: bloomThreshold, acesStrength and every tuned
; intensity above stop meaning what they meant, so an existing ini would silently look
; different. Turning it on costs two extra full-screen passes per frame.
;
; NOT total sRGB correctness. The game still uploads gamma-encoded textures and its own
; fixed-function blending still combines them in gamma space; this corrects the post chain
; only, from the captured frame onward. See the design doc for why going further is a
; different and much riskier feature.
srgbCorrect=0
```

Leave it OUT of `config_writer.cpp`'s `kManagedKeys` — the same precedent
`renderWidth`/`windowWidth` set. The config editor does not expose it, so the
writer has no business writing it back.

- [ ] **Step 7: Run the tests to verify they pass**

Run: `cmake --build --preset x86 && ctest --test-dir build-x86`
Expected: 41/41 pass, including the three new assertions.

- [ ] **Step 8: Mutation-check the default**

Change the field's initialiser to `= true`. Rebuild and run `config_test`.
Expected: the "defaults to off" assertion FAILS. Restore `= false`.

This matters more than it looks: the default is the entire backwards-compatibility
guarantee for this feature.

- [ ] **Step 9: Commit**

```bash
git add config.h config.cpp config_test.cpp opengl32_enhancer.ini
git commit -m "Add the srgbCorrect config key, default off"
```

---

### Task 2: The `srgb_convert` conversion passes

**Files:**
- Create: `srgb_convert.h`, `srgb_convert.cpp`
- Create: `srgb_convert_test.cpp`
- Modify: `CMakeLists.txt` (three places — see Step 6)

**Interfaces:**
- Consumes: `GetGlComputeApi()`, `GetGlContextGeneration()` from `gl_loader.h`.
- Produces:
  ```cpp
  bool ApplySrgbDecode(unsigned int srcTexture, unsigned int dstTexture, int width, int height);
  bool ApplySrgbEncode(unsigned int srcTexture, unsigned int dstTexture, int width, int height);
  ```
  Task 4 and Task 5 both call these.

- [ ] **Step 1: Write the header**

Create `srgb_convert.h`:

```cpp
#pragma once

// The two colour-space conversion passes that bracket the post-effect chain when
// srgbCorrect=1 - one stage each in the shared pipeline (see post_effects.cpp): reads
// srcTexture, writes dstTexture. No capture/blit/state-save of its own - the caller owns the
// pipeline's shared textures and the app's GL state around the whole chain.
//
// These exist because sRGB is roughly a 2.2-power ENCODING, and averaging two encoded values
// does not give the encoding of their average. Every stage that averages, blurs, thresholds or
// tone-maps is therefore weighting the wrong quantity until the frame is decoded to linear
// light. See docs/superpowers/specs/2026-09-20-srgb-correctness-design.md.
//
// Both use the EXACT piecewise sRGB transfer function, never a pow(2.2) approximation. The
// whole feature is a correctness claim, and the linear segment near black - below 0.04045 in,
// 0.0031308 out - is exactly where a pow approximation is most wrong. Approximating here would
// be introducing a new error while removing an old one.

// sRGB-encoded -> linear light. srcTexture and dstTexture are both width x height RGBA16F 2D
// textures owned by the caller. Alpha passes through untouched - it is a coverage value, not a
// light measurement, and encoding never applied to it. Negative inputs are clamped to 0 before
// the curve, since pow() of a negative is NaN and one NaN propagates through every later
// stage. Returns true if dstTexture was actually written (the caller must only treat
// dstTexture as the pipeline's new source when this returns true); returns false if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplySrgbDecode(unsigned int srcTexture, unsigned int dstTexture, int width, int height);

// Linear light -> sRGB-encoded. Same contract as ApplySrgbDecode in every respect. Values
// above 1.0 are left to the curve rather than clamped: the pipeline is RGBA16F and a stage
// upstream of tone mapping can legitimately produce them, the same reasoning gamma.h gives.
bool ApplySrgbEncode(unsigned int srcTexture, unsigned int dstTexture, int width, int height);
```

- [ ] **Step 2: Write the failing test**

Create `srgb_convert_test.cpp`. Copy the window/context/pixel-format preamble from
`gamma_test.cpp` verbatim — it is the closest existing single-pass stage test — then:

```cpp
// The transfer function itself, pinned at the value that separates the real curve from the
// pow(2.2) approximation people reach for. sRGB 0.5 is linear 0.21404; pow(0.5, 2.2) is
// 0.21764. Those differ by ~0.9 of an 8-bit step, so a +-1/255 tolerance would NOT catch the
// approximation - this asserts against the float value directly, which does.
const float kMidSrgb   = 0.5f;
const float kMidLinear = 0.21404f;

// A four-step wedge covering both segments of the piecewise curve. 0.02 is BELOW the 0.04045
// knee, so it exercises the linear segment that a single pow() gets wrong by a wide margin;
// the rest are on the power segment. 0 and 1 are fixed points of both directions.
const float kWedge[4] = {0.0f, 0.02f, 0.5f, 1.0f};
```

Then three cases:

```cpp
// --- decode maps sRGB to linear at the pinned midpoint ---
{
    UploadFloat(srcTex, kMidSrgb);                    // fills the source with 0.5 in rgb
    bool wrote = ApplySrgbDecode(srcTex, dstTex, width, height);
    Check(wrote, "ApplySrgbDecode reported that it wrote");
    float got = ReadBackFloat(dstTex);
    printf("decode: sRGB %.4f -> linear %.5f (exact curve gives %.5f, pow(2.2) gives %.5f)\n",
           kMidSrgb, got, kMidLinear, 0.21764f);
    Check(fabsf(got - kMidLinear) < 0.001f,
          "decode uses the exact piecewise curve, not a pow(2.2) approximation");
}

// --- encode is decode's inverse across both segments of the curve ---
{
    for (int i = 0; i < 4; ++i) {
        UploadFloat(srcTex, kWedge[i]);
        ApplySrgbDecode(srcTex, midTex, width, height);
        ApplySrgbEncode(midTex, dstTex, width, height);
        float got = ReadBackFloat(dstTex);
        char what[160];
        snprintf(what, sizeof(what),
                 "encode(decode(%.4f)) == %.4f, round-tripping through both segments",
                 kWedge[i], kWedge[i]);
        Check(fabsf(got - kWedge[i]) < 0.002f, what);
    }
}

// --- the linear segment below the knee is a straight 12.92 gain, not a power curve ---
// This is the case a pow(2.2) approximation fails hardest and the one the round trip above
// cannot see on its own, since an approximation that is its own inverse still round-trips.
{
    UploadFloat(srcTex, 0.02f);
    ApplySrgbDecode(srcTex, dstTex, width, height);
    float got = ReadBackFloat(dstTex);
    float expected = 0.02f / 12.92f;                  // 0.001548
    printf("decode below the knee: sRGB 0.02 -> linear %.6f (expected %.6f, "
           "pow(2.2) would give %.6f)\n", got, expected, powf(0.02f, 2.2f));
    Check(fabsf(got - expected) < 0.0002f,
          "below the 0.04045 knee decode is a straight 1/12.92 gain, not a power curve");
}
```

Three `RGBA16F` textures are needed — `srcTex`, `midTex` and `dstTex` — because the
round-trip case chains decode into encode and a stage must never read the texture it
is writing. Any small size works; 16x16 is plenty since these are per-pixel passes
with no neighbourhood.

Write `UploadFloat(tex, v)` as a `glTexSubImage2D` of `GL_RGBA`/`GL_FLOAT` filling
every pixel with `(v, v, v, 1)`, and `ReadBackFloat(tex)` as an FBO attach plus
`glReadPixels` of one pixel as `GL_RGBA`/`GL_FLOAT` returning the red channel.
`RGBA16F` plus float readback is what keeps this test off the 8-bit grid, which the
0.02 case needs: on the 8-bit grid the expected value 0.001548 and the `pow(2.2)`
value 0.000158 both quantise to 0, and the mutation would survive.

Include `<cmath>` for `fabsf` and `powf`.

- [ ] **Step 3: Run the test to verify it fails**

Run: `cmake --build --preset x86 --target srgb_convert_test`
Expected: FAIL to compile — `srgb_convert.cpp` does not exist yet.

- [ ] **Step 4: Write the implementation**

Create `srgb_convert.cpp` by copying `pixel_invert.cpp` wholesale and changing three
things: the two shader sources, a `PipelineState` per direction, and the entry points.

The shaders:

```cpp
const char* kDecodeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    // The exact piecewise sRGB EOTF. mix() with a bvec selects per channel, so all three
    // channels take their own branch without any actual branching.
    "vec3 SrgbToLinear(vec3 s) {\n"
    "    vec3 lo = s / 12.92;\n"
    "    vec3 hi = pow((s + 0.055) / 1.055, vec3(2.4));\n"
    "    return mix(hi, lo, lessThanEqual(s, vec3(0.04045)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    // max() before the curve: pow() of a negative is NaN, and one NaN here propagates
    // through every later stage and out to the screen.
    "    imageStore(outputImage, outCoord, vec4(SrgbToLinear(max(color.rgb, vec3(0.0))), color.a));\n"
    "}\n";

const char* kEncodeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    "vec3 LinearToSrgb(vec3 l) {\n"
    "    vec3 lo = l * 12.92;\n"
    "    vec3 hi = 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055;\n"
    "    return mix(hi, lo, lessThanEqual(l, vec3(0.0031308)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    imageStore(outputImage, outCoord, vec4(LinearToSrgb(max(color.rgb, vec3(0.0))), color.a));\n"
    "}\n";
```

Two `PipelineState` globals, `g_decode` and `g_encode`, each with its own
`initTried`/`initOk`/`generation`/`program`. Factor the body into one static helper
taking the state, the shader source and a name for the log, then:

```cpp
bool ApplySrgbDecode(unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    return RunConvertPass(g_decode, kDecodeShaderSource, "srgb_convert(decode)",
                          srcTexture, dstTexture, width, height);
}

bool ApplySrgbEncode(unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    return RunConvertPass(g_encode, kEncodeShaderSource, "srgb_convert(encode)",
                          srcTexture, dstTexture, width, height);
}
```

Keep `pixel_invert.cpp`'s context-generation reset, its `width <= 0 || height <= 0`
guard, its `(width + 7) / 8` group maths, its
`glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT)` and its
`glGetError()` report, all unchanged.

- [ ] **Step 5: Run the test to verify it passes**

Run: `cmake --build --preset x86 --target srgb_convert_test && ./build-x86/srgb_convert_test.exe`
Expected: every assertion PASS. Note the printed decode value should read `0.21404`.

- [ ] **Step 6: Wire it into the build**

Three edits to `CMakeLists.txt`:

1. Append `srgb_convert.cpp` to the `add_library(${WRAPPER_NAME_CXX} SHARED ...)` source list (line 43).
2. Add the test target beside `gamma_test`:
   ```cmake
   # Creates a real GL context and checks the two sRGB conversion passes against the EXACT
   # piecewise transfer function - including the linear segment below the 0.04045 knee, which
   # is where the pow(2.2) approximation people reach for is most wrong - and that encode is
   # decode's inverse across both segments. See srgb_convert_test.cpp.
   add_executable(srgb_convert_test srgb_convert_test.cpp srgb_convert.cpp gl_loader.cpp)
   target_link_libraries(srgb_convert_test opengl32 gdi32 user32)
   ```
3. Add `srgb_convert_test` to the `gpu`-labelled `foreach(test_target ...)` list (~line 475).

Also append `srgb_convert.cpp` to `post_effects_test`'s source list (line 369) and
`config_editor`'s (line 397) — Task 4 makes `post_effects.cpp` depend on it, and both
of those targets link `post_effects.cpp`.

- [ ] **Step 7: Run the full suite**

Run: `cmake --build --preset x86 && ctest --test-dir build-x86`
Expected: 42/42 pass.

- [ ] **Step 8: Mutation-check the transfer function**

Replace `SrgbToLinear`'s body with `return pow(s, vec3(2.2));`. Rebuild and run
`srgb_convert_test`.
Expected: the midpoint assertion FAILS (0.21764 vs 0.21404) **and** the below-the-knee
assertion FAILS by a wide margin (0.000158 vs 0.001548). Restore the exact curve.

If only one of the two fails, the other assertion is not pulling its weight — say so.

- [ ] **Step 9: Commit**

```bash
git add srgb_convert.h srgb_convert.cpp srgb_convert_test.cpp CMakeLists.txt
git commit -m "Add the exact piecewise sRGB decode and encode passes"
```

---

### Task 3: The per-stage colour-space table

**Files:**
- Modify: `post_effects.h` (declare `ColorSpace` and `ColorSpaceFor`)
- Modify: `post_effects.cpp` (define `ColorSpaceFor` beside `StageNeedsDepth`)
- Test: `post_effects_test.cpp`

**Interfaces:**
- Consumes: `EffectKind` from `config.h`.
- Produces:
  ```cpp
  enum class ColorSpace { Linear, Display };
  ColorSpace ColorSpaceFor(EffectKind stage);
  ```
  Task 4 drives the chain from this.

- [ ] **Step 1: Write the failing test**

In `post_effects_test.cpp`, beside `CheckResolveHalvingSteps`:

```cpp
// The colour space each stage wants to be handed - see ColorSpaceFor in post_effects.h.
// Every one of the twenty-five EffectKind values is listed, not just the three interesting
// ones: a stage added later without a considered space is exactly the failure this catches,
// and a test that only pinned the Display ones would not catch it.
bool CheckColorSpaceFor() {
    struct Row { EffectKind stage; ColorSpace expected; const char* why; };
    const Row rows[] = {
        // The three that want display-referred values.
        {EffectKind::LutGrading, ColorSpace::Display,
         ".cube LUTs are authored against encoded input; a linear value reads the wrong cell"},
        {EffectKind::Dither, ColorSpace::Display,
         "it exists to hide the quantisation of the final 8-bit write"},
        {EffectKind::Gamma, ColorSpace::Display,
         "an output correction - it should act on what is about to be shown"},

        // acestonemap is the one most likely to be got wrong, so it is pinned with its
        // reasoning: the Narkowicz fit maps linear light into a display-referred RANGE whose
        // values still have to be encoded afterwards, so its output is still 'linear, not yet
        // encoded'. If this were Display, every stage would need separate in and out spaces.
        {EffectKind::AcesToneMap, ColorSpace::Linear,
         "the Narkowicz fit takes linear light and its output still needs encoding"},

        // Everything else averages, filters or blends, and all of those want light.
        {EffectKind::None, ColorSpace::Linear, "no stage, no opinion"},
        {EffectKind::Invert, ColorSpace::Linear, "a per-pixel transform on light"},
        {EffectKind::Bilinear, ColorSpace::Linear, "reconstruction is an average"},
        {EffectKind::NVScaler, ColorSpace::Linear, "reconstruction is an average"},
        {EffectKind::Bloom, ColorSpace::Linear, "threshold and blur both weight luminance"},
        {EffectKind::Sharpen, ColorSpace::Linear, "a neighbourhood weighting"},
        {EffectKind::Vignette, ColorSpace::Linear, "a multiply on light"},
        {EffectKind::ChromaticAberration, ColorSpace::Linear, "resamples per channel"},
        {EffectKind::Taa, ColorSpace::Linear, "blends against history"},
        {EffectKind::Smaa, ColorSpace::Linear, "blends along detected edges"},
        {EffectKind::Fsr, ColorSpace::Linear, "reconstruction is an average"},
        {EffectKind::Cas, ColorSpace::Linear, "a neighbourhood weighting"},
        {EffectKind::Nr, ColorSpace::Linear, "denoising is an average"},
        {EffectKind::LocalContrast, ColorSpace::Linear, "a local mean"},
        {EffectKind::DepthVignette, ColorSpace::Linear, "a multiply on light"},
        {EffectKind::Ssao, ColorSpace::Linear, "an occlusion multiply on light"},
        {EffectKind::Dof, ColorSpace::Linear, "a blur"},
        {EffectKind::Fog, ColorSpace::Linear, "a blend towards a fog colour"},
        {EffectKind::LightShafts, ColorSpace::Linear, "radial accumulation is an average"},
        {EffectKind::Ssr, ColorSpace::Linear, "a blend of reflected light"},
        {EffectKind::MotionBlur, ColorSpace::Linear, "a directional average"},
    };

    bool ok = true;
    for (const Row& row : rows) {
        char what[256];
        snprintf(what, sizeof(what), "%s wants %s (%s)", EffectNameFor(row.stage),
                 row.expected == ColorSpace::Display ? "display" : "linear", row.why);
        ok = Check(ColorSpaceFor(row.stage) == row.expected, what) && ok;
    }

    // The count is asserted, not assumed. Adding an EffectKind without adding a row here
    // would otherwise leave the new stage's space silently unpinned, which is the whole
    // failure this table exists to prevent.
    ok = Check((int)(sizeof(rows) / sizeof(rows[0])) == 25,
               "all 25 EffectKind values have a pinned colour space") && ok;
    return ok;
}
```

Call it from `main` beside `CheckResolveHalvingSteps`:

```cpp
    ok = CheckColorSpaceFor() && ok;
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build --preset x86 --target post_effects_test`
Expected: FAIL to compile — `ColorSpace` is not declared.

- [ ] **Step 3: Declare the type and the function**

Append to `post_effects.h`:

```cpp
// The colour space a value in the pipeline is currently in.
//
// `Linear` is light: the quantity that may legitimately be averaged, blurred, thresholded or
// tone-mapped. `Display` is sRGB-encoded - what the 8-bit back buffer holds and what the game
// handed us. The distinction matters because sRGB is roughly a 2.2-power encoding, so the
// average of two encoded values is NOT the encoding of their average.
enum class ColorSpace {
    Linear,
    Display,
};

// Which space a stage wants to be handed. Only consulted when srgbCorrect=1; with it off the
// chain never asks, and no conversion is ever generated.
//
// Three of the twenty-five stages want `Display`, and each for the same underlying reason -
// they are about the OUTPUT rather than about the light. Everything else averages, filters or
// blends, and all of those are only meaningful on light.
//
// `acestonemap` is deliberately `Linear` and this is the subtle one: the Narkowicz fit takes
// linear scene light and produces a display-referred range whose values must still be encoded
// afterwards (`color = ACESFitted(linear); color = encode(color);`). Its output is therefore
// still 'linear, not yet encoded'. That is what lets this be a single per-stage lookup rather
// than a separate input and output space for every stage.
ColorSpace ColorSpaceFor(EffectKind stage);
```

- [ ] **Step 4: Define it**

In `post_effects.cpp`, directly after `StageNeedsDepth`, matching that function's shape:

```cpp
// See post_effects.h.
ColorSpace ColorSpaceFor(EffectKind stage) {
    switch (stage) {
        case EffectKind::LutGrading:
        case EffectKind::Dither:
        case EffectKind::Gamma:
            return ColorSpace::Display;
        default:
            return ColorSpace::Linear;
    }
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build --preset x86 && ctest --test-dir build-x86`
Expected: 42/42 pass. Read the new assertions from `build-x86/opengl32_enhancer.log`
if they are printed after the first `ApplySelectedEffect` call — but
`CheckColorSpaceFor` runs before it, so these appear on the console.

- [ ] **Step 6: Mutation-check the table**

Move `EffectKind::LutGrading` from the `Display` group to fall through to `default`.
Rebuild and run `post_effects_test`.
Expected: the `lutgrading` row FAILS. Restore it.

Then delete a row from the test's `rows[]` array and rebuild.
Expected: the count assertion FAILS. Restore it.

- [ ] **Step 7: Commit**

```bash
git add post_effects.h post_effects.cpp post_effects_test.cpp
git commit -m "Add the per-stage colour space table"
```

---

### Task 4: Bracket the chain and convert mid-chain

**Files:**
- Modify: `post_effects.cpp` — the capture block (~line 707-716), the stage loop (~line 841 onward)
- Test: `post_effects_test.cpp`

**Interfaces:**
- Consumes: `GetAnaxConfig().srgbCorrect` (Task 1), `ApplySrgbDecode`/`ApplySrgbEncode` (Task 2), `ColorSpaceFor` (Task 3).
- Produces: a `ColorSpace space` local in `ApplySelectedEffect`, live at the point the present path begins. Task 5 reads it.

- [ ] **Step 1: Write the failing test**

In `post_effects_test.cpp`, after the 4x resolve block. Two cases, because they
exercise different conversions:

```cpp
// --- the sRGB bracket is an identity around a no-op chain ---
//
// The safety property: with srgbCorrect=1 and a chain that does nothing, decode-then-encode
// must give the image back. If this drifts, every frame drifts.
//
// `invert, invert` and NOT an empty chain, deliberately. ShouldSkipEffectChain returns early
// on an empty chain, so no conversion would run at all and the test would pass while proving
// nothing. Two inverts is a real pair of Linear stages with a known composite identity - and
// note inverting in linear space is NOT the same operation as inverting in gamma space, so
// this also confirms the stages really did see linear values.
{
    AnaxConfig& mutableConfig = GetMutableAnaxConfig();
    int savedStageCount = mutableConfig.stageCount;
    bool savedSrgb = mutableConfig.srgbCorrect;
    EffectKind savedStage0 = mutableConfig.stages[0];
    EffectKind savedStage1 = mutableConfig.stages[1];

    mutableConfig.stageCount = 2;
    mutableConfig.stages[0] = EffectKind::Invert;
    mutableConfig.stages[1] = EffectKind::Invert;
    mutableConfig.srgbCorrect = true;
    ResetRenderTargetState();                 // no supersampling: isolate the bracket

    BlitPatternToBackBuffer();                // the same known pattern the primary run uses
    ApplySelectedEffect(hdc);
    std::vector<unsigned char> out((size_t)width * height * 4);
    ReadBackBuffer(out);

    int worst = 0;
    for (size_t i = 0; i < inputPixels.size(); ++i) {
        int d = abs((int)inputPixels[i] - (int)out[i]);
        worst = d > worst ? d : worst;
    }
    printf("srgb bracket identity: worst per-channel difference = %d\n", worst);
    // +-1, not byte-exact, and that is honest rather than lax: the round trip passes through
    // RGBA16F, whose ten-bit mantissa can land a value on the wrong side of an 8-bit
    // rounding boundary. Claiming byte-exactness would claim something the format cannot
    // deliver. A broken conversion is off by far more than one step.
    ok = Check(worst <= 1,
               "srgb bracket: decode-then-encode returns the image around a no-op chain") && ok;

    mutableConfig.stageCount = savedStageCount;
    mutableConfig.stages[0] = savedStage0;
    mutableConfig.stages[1] = savedStage1;
    mutableConfig.srgbCorrect = savedSrgb;
}

// --- a display-space stage gets handed display-referred values ---
//
// `gamma` at gamma=1, brightness=1 is documented to reproduce its input EXACTLY (gamma.h:
// the pow() is skipped outright at gamma=1). As a Display stage it forces an encode before
// it and leaves the image in display space, so the present path must then do nothing - the
// fourth row of the design's data-flow table. Net effect: still an identity, but reached
// through the mid-chain conversion rather than the bracket's own.
{
    AnaxConfig& mutableConfig = GetMutableAnaxConfig();
    int savedStageCount = mutableConfig.stageCount;
    bool savedSrgb = mutableConfig.srgbCorrect;
    EffectKind savedStage0 = mutableConfig.stages[0];
    float savedGamma = mutableConfig.gamma;
    float savedBrightness = mutableConfig.brightness;

    mutableConfig.stageCount = 1;
    mutableConfig.stages[0] = EffectKind::Gamma;
    mutableConfig.gamma = 1.0f;
    mutableConfig.brightness = 1.0f;
    mutableConfig.srgbCorrect = true;
    ResetRenderTargetState();

    BlitPatternToBackBuffer();
    ApplySelectedEffect(hdc);
    std::vector<unsigned char> out((size_t)width * height * 4);
    ReadBackBuffer(out);

    int worst = 0;
    for (size_t i = 0; i < inputPixels.size(); ++i) {
        int d = abs((int)inputPixels[i] - (int)out[i]);
        worst = d > worst ? d : worst;
    }
    printf("srgb mid-chain identity: worst per-channel difference = %d\n", worst);
    ok = Check(worst <= 1,
               "srgb mid-chain: a Display stage is handed encoded values and the present "
               "path then correctly does nothing") && ok;

    mutableConfig.stageCount = savedStageCount;
    mutableConfig.stages[0] = savedStage0;
    mutableConfig.gamma = savedGamma;
    mutableConfig.brightness = savedBrightness;
    mutableConfig.srgbCorrect = savedSrgb;
}
```

Extract `BlitPatternToBackBuffer()` and `ReadBackBuffer(out)` as small lambdas from
the code the primary integration run already uses (the `patternFbo` blit and the
`glReadBuffer(GL_BACK)` + `glReadPixels` pair) rather than duplicating them.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build --preset x86 --target post_effects_test && ./build-x86/post_effects_test.exe`
Then: `grep "srgb bracket\|srgb mid-chain" build-x86/opengl32_enhancer.log`
Expected: both FAIL — `srgbCorrect` is read by nothing yet, so `invert, invert` in
gamma space is an identity too, but `gamma` after a missing encode is not... in fact
**expect the bracket case to PASS and the mid-chain case to PASS**, because nothing
converts and both chains are identities on their own.

This is the important subtlety in this task: **neither test fails before the
implementation**, so neither is a red test in the ordinary sense. Their value is
entirely as regression guards, and they only earn it through Step 7's mutations.
Record this honestly rather than pretending a red-green cycle happened.

- [ ] **Step 3: Decode after capture**

In `post_effects.cpp`, immediately after the capture's `glGetError()` check
(~line 717-724), before the chain:

```cpp
    // The sRGB bracket opens here - see srgb_convert.h and the design doc. The frame the game
    // drew is sRGB-encoded; every stage below wants light. Decoding once here and encoding
    // once on present is what makes the whole chain correct, rather than each stage having to
    // know about the encoding.
    //
    // Decoded into pair[1] and the ping-pong advanced, exactly like a stage - the conversion
    // IS a stage in every respect except that the user did not list it.
    ColorSpace space = ColorSpace::Display;
    if (config.srgbCorrect) {
        if (ApplySrgbDecode(pair[cur], pair[1 - cur], curWidth, curHeight)) {
            cur = 1 - cur;
            space = ColorSpace::Linear;
        }
        // captureTex holds the pristine frame and arrives encoded like pair[0] did. Its two
        // consumers - taa and motionblur - are both Linear stages and receive it ALONGSIDE
        // pipeline textures, so leaving it encoded would have them compare a linear image
        // against an encoded one. That is a worse error than the one this feature removes.
        //
        // It has no ping-pong partner, so the decoded result goes to the free half of the
        // pair and is copied back. The copy goes through presentFbo and glCopyTexSubImage2D
        // rather than glCopyImageSubData, which is NOT resolved in GlComputeApi - and this
        // is the same FBO-and-copy route captureTex was filled by twenty lines above, so it
        // needs no new entry point at all.
        if (space == ColorSpace::Linear && needCapture && g_pipeline.captureTex != 0) {
            if (ApplySrgbDecode(g_pipeline.captureTex, pair[1 - cur], curWidth, curHeight)) {
                gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.presentFbo);
                gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                          pair[1 - cur], 0);
                gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, g_pipeline.presentFbo);
                gl.glReadBuffer(GL_COLOR_ATTACHMENT0);
                gl.glBindTexture(GL_TEXTURE_2D, g_pipeline.captureTex);
                gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, curWidth, curHeight);
            }
        }
    }
```

Do **not** add `glCopyImageSubData` to `GlComputeApi` — it is absent today and the
route above makes it unnecessary. Adding an entry point that one call site uses,
when an already-resolved one does the same job, is the kind of surface this proxy
does not need.

Note `space` is declared here and must stay in scope through to the present path —
Task 5 reads it.

- [ ] **Step 4: Convert at the stage boundary**

In the stage loop, immediately after the `src`/`dst`/`dstW`/`dstH` block and before
`bool wrote = false;`:

```cpp
        // The image is in whatever space the last stage left it in; this stage may want the
        // other one. Only generated when they actually disagree, so a chain that is entirely
        // Linear (the common case) pays for the bracket and nothing more.
        if (config.srgbCorrect && ColorSpaceFor(stage) != space) {
            bool converted = (space == ColorSpace::Display)
                                 ? ApplySrgbDecode(pair[cur], pair[1 - cur], curWidth, curHeight)
                                 : ApplySrgbEncode(pair[cur], pair[1 - cur], curWidth, curHeight);
            if (converted) {
                cur = 1 - cur;
                space = ColorSpaceFor(stage);
                src = pair[cur];
                if (!doRealUpscale) {
                    dst = pair[1 - cur];
                }
            }
        }
```

`src` and `dst` are recomputed because the conversion advanced the ping-pong after
they were taken. Getting this wrong makes the stage read its own output — a
feedback loop that is obvious on screen but silent in a suite that never lists a
display-space stage.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build --preset x86 && ctest --test-dir build-x86`
Then: `grep "srgb bracket\|srgb mid-chain" build-x86/opengl32_enhancer.log`
Expected: 42/42 pass; both new assertions report a worst difference of 0 or 1.

- [ ] **Step 6: Verify the feature-off path is untouched**

Run the full suite and confirm the primary integration run's
"Average per-channel |output - input|" line is unchanged from before this task.
`srgbCorrect` defaults off, so it must be identical, not merely close.

- [ ] **Step 7: Mutation-check both new tests**

These are the mutations that give the two tests from Step 1 their value:

1. Delete the `ApplySrgbEncode` branch from Step 4's conversion (make it always
   decode). Expected: the **mid-chain** test FAILS — `gamma` is handed linear values
   and the image comes back visibly wrong. Restore.
2. Delete the `src = pair[cur];` line from Step 4. Expected: the **mid-chain** test
   FAILS — the stage reads the pre-conversion texture. Restore.
3. Delete the `space = ColorSpace::Linear;` assignment in Step 3. Expected: the
   **bracket** test FAILS — nothing ever encodes on present, so a linear image is
   written to an 8-bit buffer and comes back washed out. Restore.

If any mutation survives, the corresponding test is not pinning what it claims —
report that rather than moving on.

- [ ] **Step 8: Mutation-check the captureTex decode**

Delete the `captureTex` decode from Step 3 and run the suite.
Expected: **probably nothing fails**, because no test in this suite runs `motionblur`
or `taa` with `srgbCorrect=1` against a moving frame. If so, say so plainly in the
commit message: the `captureTex` decode is reasoned, not covered. Do not invent a
test that only appears to cover it.

- [ ] **Step 9: Commit**

```bash
git add post_effects.cpp post_effects_test.cpp
git commit -m "Run the effect chain on linear light when srgbCorrect is on"
```

---

### Task 5: Resolve in linear, encode after

**Files:**
- Modify: `post_effects.cpp` — the present path (the `ResolveHalvingSteps` block and the final blit)
- Test: `post_effects_test.cpp`

**Interfaces:**
- Consumes: `space` from Task 4, `ResolveHalvingSteps` (already shipped), `ApplySrgbDecode`/`ApplySrgbEncode`.
- Produces: nothing later tasks depend on.

- [ ] **Step 1: Write the failing test**

In `post_effects_test.cpp`, after Task 4's cases. Two variants of the same
measurement, because the second is the one that catches the mutation the first
cannot:

```cpp
// --- the resolve averages LINEAR light, not encoded values ---
//
// The whole ordering decision in one number. Black against white, resolved 2:1:
//   averaging encoded values -> 128
//   averaging light          -> decode to 0 and 1, average to 0.5, encode -> 188
// A 60-point gap, far outside any rounding argument.
//
// Run twice: once with a chain ending in a Linear stage, once ending in a Display stage.
// The second is the shipped effect= line's shape (it ends '... dither, gamma') and is the
// only one that catches a missing decode-before-resolve.
struct ResolveCase { EffectKind lastStage; const char* label; };
const ResolveCase resolveCases[] = {
    {EffectKind::Invert,  "chain ending Linear"},
    {EffectKind::Gamma,   "chain ending Display"},
};

for (const ResolveCase& rc : resolveCases) {
    AnaxConfig& mutableConfig = GetMutableAnaxConfig();
    int savedStageCount = mutableConfig.stageCount;
    bool savedSrgb = mutableConfig.srgbCorrect;
    EffectKind savedStage0 = mutableConfig.stages[0];
    EffectKind savedStage1 = mutableConfig.stages[1];
    int savedRenderWidth = mutableConfig.renderWidth;
    int savedRenderHeight = mutableConfig.renderHeight;
    float savedGamma = mutableConfig.gamma;
    float savedBrightness = mutableConfig.brightness;

    // Two stages, both no-ops, so the only thing shaping the result is the resolve.
    // invert+invert composes to identity; gamma at 1/1 is documented to be exact.
    mutableConfig.stageCount = 2;
    mutableConfig.stages[0] = EffectKind::Invert;
    mutableConfig.stages[1] = rc.lastStage == EffectKind::Invert ? EffectKind::Invert
                                                                 : EffectKind::Gamma;
    mutableConfig.gamma = 1.0f;
    mutableConfig.brightness = 1.0f;
    mutableConfig.srgbCorrect = true;
    mutableConfig.renderWidth = width * 2;
    mutableConfig.renderHeight = height * 2;

    ResetRenderTargetState();
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, width, height);
    ArmSupersampleForFrame(EnsureRenderTarget());
    if (Check(IsSupersampleActive(), "linear resolve: supersampling armed")) {
        BindRenderTarget();
        pGlViewport(0, 0, mutableConfig.renderWidth, mutableConfig.renderHeight);

        // One-pixel-wide alternating black and white columns in the render target. At 2:1
        // every destination pixel covers exactly one black and one white texel.
        pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        pGlClear(GL_COLOR_BUFFER_BIT);
        pGlEnable(GL_SCISSOR_TEST);
        pGlClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        for (int x = 1; x < mutableConfig.renderWidth; x += 2) {
            pGlScissor(x, 0, 1, mutableConfig.renderHeight);
            pGlClear(GL_COLOR_BUFFER_BIT);
        }
        pGlDisable(GL_SCISSOR_TEST);

        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        pGlClearColor(40.0f / 255.0f, 40.0f / 255.0f, 180.0f / 255.0f, 1.0f);
        pGlClear(GL_COLOR_BUFFER_BIT);

        ApplySelectedEffect(hdc);

        std::vector<unsigned char> out((size_t)width * height * 4);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.glReadBuffer(GL_BACK);
        gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out.data());

        long long total = 0;
        int counted = 0;
        for (int x = 4; x < width - 4; ++x) {
            total += out[((size_t)(height / 2) * width + x) * 4];
            ++counted;
        }
        int mean = (int)(total / counted);
        char what[200];
        snprintf(what, sizeof(what),
                 "linear resolve (%s): black against white resolves to ~188 (light), "
                 "not ~128 (encoded values)", rc.label);
        printf("linear resolve (%s): middle row mean = %d\n", rc.label, mean);
        ok = Check(mean >= 170 && mean <= 205, what) && ok;
    }

    mutableConfig.stageCount = savedStageCount;
    mutableConfig.stages[0] = savedStage0;
    mutableConfig.stages[1] = savedStage1;
    mutableConfig.renderWidth = savedRenderWidth;
    mutableConfig.renderHeight = savedRenderHeight;
    mutableConfig.gamma = savedGamma;
    mutableConfig.brightness = savedBrightness;
    mutableConfig.srgbCorrect = savedSrgb;
    ResetRenderTargetState();
    pGlViewport(0, 0, width, height);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build --preset x86 --target post_effects_test && ./build-x86/post_effects_test.exe`
Then: `grep "linear resolve" build-x86/opengl32_enhancer.log`
Expected: **both cases FAIL, reporting a mean near 128.** Task 4 encodes nothing on
present yet, so the image reaching the resolve is linear but the final blit writes it
raw — read back through the 8-bit buffer it lands near the encoded average. Record
the actual numbers; they are the before-half of the evidence.

- [ ] **Step 3: Decode before the resolve when there is one**

In the present path, immediately before the `int steps = ResolveHalvingSteps(...)`
block:

```cpp
    // The resolve's halvings are averages, and averages are the whole reason this feature
    // exists - so the resolve must see light. A chain ending in a display-space stage is
    // decoded once more first.
    //
    // Conditional on there BEING halvings, and that is not an optimisation for its own sake:
    // with no resolve nothing is averaged, so the space no longer matters and a chain ending
    // display-referred is already in the space the 8-bit write wants. Forcing a decode there
    // would just be an encode's inverse, two passes that cancel.
    //
    // Without this, the SHIPPED effect= line - which ends '... dither, gamma', both
    // display-space stages - would have gone straight back into the bug this feature fixes.
    int steps = ResolveHalvingSteps(curWidth, curHeight, presentWidth, presentHeight);
    if (config.srgbCorrect && steps > 0 && space == ColorSpace::Display) {
        if (ApplySrgbDecode(pair[cur], pair[1 - cur], curWidth, curHeight)) {
            cur = 1 - cur;
            space = ColorSpace::Linear;
        }
    }
```

Delete the existing `int steps = ...` line that this replaces.

- [ ] **Step 4: Encode after the resolve**

After the halving loop closes and before
`gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.presentFbo);`:

```cpp
    // The bracket closes here. After the resolve, so the halvings above averaged light -
    // encoding first would have fixed the chain while leaving the largest single instance of
    // the bug (the downsample) in place.
    if (config.srgbCorrect && space == ColorSpace::Linear) {
        if (ApplySrgbEncode(pair[cur], pair[1 - cur], curWidth, curHeight)) {
            cur = 1 - cur;
            space = ColorSpace::Display;
        }
    }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build --preset x86 && ctest --test-dir build-x86`
Then: `grep "linear resolve\|srgb bracket\|srgb mid-chain\|4x resolve" build-x86/opengl32_enhancer.log`
Expected: 42/42 pass. Both `linear resolve` cases report a mean near **188**. The
`4x resolve` case from the previous wave must still report 64 — it runs with
`srgbCorrect` off and must be untouched.

- [ ] **Step 6: Mutation-check the ordering**

1. Move the Step 4 encode to before the `steps` block. Expected: **both**
   `linear resolve` cases FAIL, back near 128. Restore.
2. Delete the Step 3 decode entirely. Expected: the **`chain ending Display`** case
   FAILS and the `chain ending Linear` case still passes. This is the mutation the
   single-variant version of this test could not catch, and the reason there are two.
   Restore.
3. Change `steps > 0` to `true` in Step 3. Expected: nothing fails — it is a
   redundant-work guard, not a correctness one. Confirm that and restore; do not
   claim coverage it does not have.

- [ ] **Step 7: Commit**

```bash
git add post_effects.cpp post_effects_test.cpp
git commit -m "Resolve in linear light and encode after the downsample"
```

---

### Task 6: Documentation and in-game verification

**Files:**
- Modify: `README.md`
- Modify: `docs/enhancement-opportunities.md` (the Tier 4 sRGB bullet)
- Modify: `docs/superpowers/specs/2026-09-20-srgb-correctness-design.md` (status line)

**Interfaces:**
- Consumes: the finished feature.
- Produces: nothing.

- [ ] **Step 1: Document the key in the README**

Add a section beside the supersampling one covering: what `srgbCorrect` does, why
the chain is wrong without it, the 128-vs-188 example as the concrete illustration,
that it is off by default and why, that tuned values will need revisiting, and the
explicit non-claim that this does **not** make the game's own rendering linear.

- [ ] **Step 2: Strike the Tier 4 bullet**

In `docs/enhancement-opportunities.md`, mark the "sRGB correctness" bullet shipped in
the same style the `autoMipmap` bullet uses, pointing at `srgb_convert.h` and the
design doc. Record the two findings that came out of the design rather than just the
outcome: that `acestonemap` is linear-in/linear-out (which collapsed the space table
to a single lookup), and that leaving the resolve's space to whatever the chain ended
in would have put the shipped `effect=` line straight back into the bug.

Also update the "Suggested order" list — this was the last of Tier 4's open items
apart from texture upscaling.

- [ ] **Step 3: Run the game**

Launch Anachronox with `srgbCorrect=1` and the full shipped `effect=` chain, at
`renderWidth`/`renderHeight` 1280x960 over a 640x480 game.

Confirm from `opengl32_enhancer.log`: the config summary reports `srgbCorrect=1`, no
GL errors in steady state, and no `srgb_convert` shader-init failures.

**Beware the testbed's third-party GL wrappers** — this install has carried them
before, and they will confound any judgement about what the proxy is doing. Confirm
the log's entry points resolved through this DLL before trusting anything you see.

- [ ] **Step 4: Record the validation status honestly**

Write what the run established and what it did not, in the same shape the
supersampling section uses. In particular: running without error is not the same as
looking right, and a linear chain on art authored against the gamma-space look is
exactly the kind of change that needs a human's eye. If no screenshot comparison was
made, say so.

Set the spec's `**Status:**` line to `shipped 2026-09-20` plus the validation state.

- [ ] **Step 5: Commit**

```bash
git add README.md docs/enhancement-opportunities.md docs/superpowers/specs/2026-09-20-srgb-correctness-design.md
git commit -m "Document sRGB correctness and record its validation status"
```
