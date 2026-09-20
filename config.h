#pragma once

// Selects and configures the post-process effects applied in wglSwapBuffers (see
// post_effects.h). Read from an INI-style opengl32_enhancer.ini file next to this DLL - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md for the file format.

// One stage of the post-effect pipeline. Every one of these is a peer: there is no longer a
// distinction between a "primary" effect and an "addon", they are just stages the config
// lists in the order it wants them run.
enum class EffectKind {
    None,
    Invert,
    Bilinear,
    NVScaler,
    AcesToneMap,
    Bloom,
    Sharpen,
    LutGrading,
    Vignette,
    ChromaticAberration,
    Taa,
    Dither,
    Smaa,
    Fsr,
    Cas,
    Nr,
    LocalContrast,
    DepthVignette,
    Ssao,
    Dof,
    Fog,
    LightShafts,
    Ssr,
    MotionBlur,
    Gamma,
};

// Plenty of headroom for the real stages, even if a config lists some of them twice.
const int kMaxEffectStages = 24;

struct AnaxConfig {
    // The pipeline, in the order the config asked for. A stage runs if and only if it appears
    // here, and stages run in this order - so `effect=bloom, acestonemap` and
    // `effect=acestonemap, bloom` are both valid and produce different images. Populated from
    // the comma-separated `effect` key; see EffectNameFor() for the accepted names.
    EffectKind stages[kMaxEffectStages] = {};
    int stageCount = 0;

    // Per-stage parameters. These are read only by the stages that use them, so leaving a
    // parameter set for a stage that isn't listed is harmless.
    float scale = 1.0f;                        // Bilinear, NVScaler, Fsr
    float acesStrength = 0.5f;                 // AcesToneMap
    float bloomThreshold = 0.8f;               // Bloom
    float bloomIntensity = 0.5f;               // Bloom
    float sharpness = 0.5f;                    // Sharpen, NVScaler
    char lutPath[256] = "";                    // LutGrading
    float lutStrength = 1.0f;                  // LutGrading
    float vignetteIntensity = 0.3f;            // Vignette
    float vignetteRadius = 0.7f;               // Vignette
    float chromaticAberrationStrength = 0.3f;  // ChromaticAberration
    float taaBlend = 0.5f;                     // Taa
    bool taaJitter = true;                     // Taa
    float ditherStrength = 1.0f;               // Dither
    bool fsrDenoise = false;                   // Fsr
    float fsrFilmGrain = 0.0f;                 // Fsr
    float nrIntensity = 0.5f;                  // Nr
    int nrPasses = 1;                          // Nr
    float nrColorStrength = 1.0f;              // Nr
    float nrTonePreservation = 0.0f;           // Nr
    float nrGrainPreservation = 0.0f;          // Nr
    float localStructureStrength = 0.3f;       // LocalContrast
    float localToneStrength = 0.3f;            // LocalContrast
    float shimmerSuppression = 0.0f;           // Taa
    float depthVignetteIntensity = 0.3f;       // DepthVignette
    float depthVignetteThreshold = 0.3f;       // DepthVignette
    // Ssao. ssaoRadius is in the GAME's world units, not a 0..1 fraction like most values here -
    // Quake II units are roughly an inch, so the useful range is tens of units. See ssao.h.
    float ssaoRadius = 24.0f;                  // Ssao
    float ssaoIntensity = 0.5f;                // Ssao
    float ssaoBias = 0.5f;                     // Ssao

    // Dof. focusDistance and focusRange are in the GAME'S OWN WORLD UNITS, like ssaoRadius -
    // see dof.h for why depth-denominated settings have to be. focusDistance 0 means "focus on
    // whatever is at the centre of the screen", which is what makes a single setting work as
    // the player walks around instead of only in the room it was tuned in.
    float dofFocusDistance = 0.0f;             // Dof, 0 = auto-focus on screen centre
    float dofFocusRange = 64.0f;               // Dof
    float dofBlurStrength = 0.0f;              // Dof, 0 = exact no-op

    // Fog. fogStart/fogEnd are in the GAME'S OWN WORLD UNITS, like ssaoRadius and dof's
    // distances - see fog.h. The defaults are sized for Quake II scale (roughly 1 unit = 1 inch),
    // so 200..2000 is about 17 to 170 feet. fogIntensity defaults to 0 because fog is a strong
    // stylistic choice, not a correction: listing the stage must change nothing until asked.
    float fogStart = 200.0f;                   // Fog
    float fogEnd = 2000.0f;                    // Fog
    float fogIntensity = 0.0f;                 // Fog, 0 = exact no-op
    float fogColorR = 0.55f;                   // Fog
    float fogColorG = 0.62f;                   // Fog
    float fogColorB = 0.70f;                   // Fog

    // LightShafts. Unlike the other depth-era stages these are screen-space fractions, not world
    // units - the light's position is found in the frame itself (see light_shafts.h), so there is
    // nothing here denominated in the game's scale.
    float shaftsIntensity = 0.0f;              // LightShafts, 0 = exact no-op
    float shaftsDensity = 0.6f;                // LightShafts
    float shaftsDecay = 0.96f;                 // LightShafts
    float shaftsThreshold = 0.75f;             // LightShafts

    // Ssr. ssrMaxDistance and ssrThickness are in the GAME'S OWN WORLD UNITS, like ssaoRadius
    // and dof's distances. ssrUpThreshold is the heuristic that stands in for the material
    // information a GL 1.1 game never supplies: nothing in the frame says "this is polished
    // marble and that is carpet", so instead only surfaces facing far enough UP reflect at all.
    // See ssr.h for what that costs. ssrIntensity defaults to 0 for the same reason fog's does -
    // reflections are a strong stylistic change, so listing the stage must alter nothing until
    // it is asked to.
    float ssrIntensity = 0.0f;                 // Ssr, 0 = exact no-op
    float ssrMaxDistance = 512.0f;             // Ssr
    float ssrThickness = 16.0f;                // Ssr
    float ssrUpThreshold = 0.7f;               // Ssr

    // Which world axis points up in the game, as 0=X, 1=Y, 2=Z, written in the ini as x/y/z.
    // `ssrUpThreshold` above measures how far a surface points up, and "up" is a fact about the
    // game's world that a captured view matrix cannot supply - it is an engine convention. Quake
    // II-family engines (Anachronox included) use +Z, hence the default. Get it wrong and
    // reflections appear on walls instead of floors. See ssr.h.
    int ssrWorldUpAxis = 2;                    // Ssr

    // MotionBlur. motionBlurMaxRadius is a fraction of the screen, not a world unit: it is the
    // longest smear allowed, and it exists because a scene cut or a teleport produces an
    // enormous inter-frame camera delta that would otherwise smear the whole frame. Clamping
    // is cheaper and more predictable than trying to detect a cut. See motion_blur.h.
    //
    // motionBlurStrength is the first intensity setting on this page to default to something
    // other than 0/1 (an exact no-op). Every other stylistic stage (fog/shafts/ssr) defaults to
    // an inert 0 because listing the stage is not itself the opt-in - it costs a dispatch even
    // at 0. motionblur is different: it is not in the shipped `effect=` line at all, so adding
    // it to your own `effect=` list already IS the opt-in, and a non-zero default means it does
    // something visible the moment it's added instead of requiring a second value to be found
    // and changed too.
    // The range is 0..4, not the 0..1 every other intensity setting uses, and that is
    // deliberate rather than sloppy. 1.0 means "smear over exactly one frame of camera motion",
    // which at 60fps is about 16ms - film looks blurry at 24fps with a 180-degree shutter, so
    // one frame at 60fps is genuinely a small smear. Values above 1 are what read as motion
    // blur rather than as a shimmer. Measured in Anachronox: 0.5 came back as "a little bit of
    // blur but not strong".
    float motionBlurStrength = 1.5f;           // MotionBlur, 0 = exact no-op
    float motionBlurMaxRadius = 0.05f;         // MotionBlur

    // Gamma. `gamma` is the display exponent (1.0 = no-op, >1 brightens the midtones, <1
    // darkens them) and `brightness` a linear gain applied BEFORE it. Both default to an exact
    // passthrough. See gamma.h for why brightness is a gain and not an additive offset.
    float gamma = 1.0f;                        // Gamma
    float brightness = 1.0f;                   // Gamma

    // Writes the current frame (color + depth + the captured projection) to frameDumpPath when
    // this virtual-key code is pressed, for the standalone config editor to load and tune
    // against - see frame_dump.h. 0 disables the feature and the per-frame key poll with it.
    // Independent of the effect= pipeline: a dump captures the game's frame BEFORE any stage
    // runs, so it works with effect=none and always yields unprocessed source.
    int frameDumpKey = 0x7B;                   // VK_F12
    char frameDumpPath[256] = "opengl32_enhancer_frame.dump";

    // Prints the camera position and orientation that modelview_capture.h reconstructs to the
    // debug log every N frames. 0 (default) is off. This exists because the capture's heuristic
    // can only be CONFIRMED inside a real game - the unit tests show it behaves as designed,
    // not that the engine does what the design assumes. Independent of the effect= pipeline,
    // and of the capture itself, which always runs and costs one glGetFloatv per frame.
    int cameraLogInterval = 0;

    // Forces trilinear + anisotropic filtering on the game's own mipmapped world/model
    // textures - see texture_filter.h. 0 (default) leaves every glTexParameter call the game
    // makes completely untouched; 1..16 is the anisotropy level to request (capped at runtime
    // to whatever the GPU/driver actually supports). Independent of the effect= pipeline.
    float anisotropy = 0.0f;

    // Generates a mip chain for textures the game uploaded WITHOUT one, lazily, and only for
    // the ones actually drawn in the world pass - see texture_mipmap.h, which explains why the
    // world pass is the only signal that can tell those apart from HUD artwork. false
    // (default) leaves every upload and every draw completely untouched. Independent of the
    // effect= pipeline. Complements `anisotropy` above, which covers the textures the game
    // mipped itself.
    bool autoMipmap = false;

    // Overrides the game window's size at the moment it creates its GL context (see
    // window_override.h) - independent of the effect= pipeline, since it's a Win32 window
    // property, not a rendered stage. Both default to 0, meaning "leave the game's own window
    // size alone"; the override only applies when BOTH are set to a positive value, since a
    // window size is a single (width, height) pair, and honoring just one would mean guessing
    // the other instead of using what's actually configured.
    int windowWidth = 0;
    int windowHeight = 0;

    // Renders the game into an offscreen framebuffer of this size and downsamples it to the
    // window on present - see render_target.h. Both default to 0, meaning off; the override
    // only applies when BOTH are positive, the same both-or-nothing rule windowWidth/
    // windowHeight uses, since a render size is a single (width, height) pair and honoring
    // just one would mean guessing the other.
    int renderWidth = 0;
    int renderHeight = 0;

    // RGBA16F instead of RGBA8 for that framebuffer's colour attachment, which makes the
    // game's OWN fixed-function blending accumulate at 16-bit float. Off by default because
    // it genuinely changes how the game's blending accumulates, and supersampling on its own
    // should not change anything except sample count.
    bool renderFloatBuffer = false;

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

    // Draws a small "FX" badge in a corner of the frame whenever the effect= pipeline actually
    // runs, so you can tell "the pipeline ran but nothing looked different" apart from "the
    // proxy isn't loaded/configured at all" without checking the log - see fx_indicator.h.
    // Independent of the effect= pipeline's own stages; on by default precisely because its
    // job is to catch the case where you forgot to check whether it's working.
    bool fxIndicator = true;

    // Runs small GL_RGBA/GL_UNSIGNED_BYTE texture uploads in place (same dimensions - no
    // resize) at load time through a single stage (see texture_effect.h). `sharpen`/`cas` use
    // the `sharpness` value above; `invert` is a debug/demo aid for spotting which draws touch
    // which textures. Independent of the effect= pipeline. EffectKind::None disables it
    // (default). Only EffectKind::Sharpen, EffectKind::Cas and EffectKind::Invert are accepted
    // here - anything else (including Nr and LocalContrast, both multi-pass) falls back to
    // None. See texture_effect.h for why.
    EffectKind textureEffect = EffectKind::None;
};

// True if `stage` appears anywhere in config.stages. Order-insensitive, so this answers "is
// it enabled", not "when does it run" - post_effects.cpp walks config.stages directly for
// the latter.
bool HasEffectStage(const AnaxConfig& config, EffectKind stage);

// Which stages need the pre-HUD world-only colour capture (see world_capture.h). Latching it
// costs a full-resolution copy and a persistent texture every frame, so it is gated on a stage
// that actually consumes it being in the chain.
//
// This lives here rather than beside StageNeedsDepth in post_effects.h because it has TWO
// consumers in two different translation units - post_effects.cpp's own texture allocation and
// world_capture.cpp's latch gate - so it belongs in the vocabulary both already share (config.h),
// not in a header that would force one of them to depend on the other. StageNeedsDepth stays in
// post_effects.h because it has exactly one consumer, post_effects.cpp itself. The asymmetry is
// deliberate, not an oversight: previously the same question was answered by two hand-written
// copies of `HasEffectStage(config, MotionBlur)`, one in post_effects.cpp and one in
// world_capture.cpp, and adding a second consumer to only one of them would have silently
// starved the other.
bool StageNeedsWorldCapture(EffectKind stage);

// Does any stage in this config's chain need the world capture? The two callers - the
// pipeline's texture allocation and world_capture.cpp's own latch gate - must agree exactly, or
// one allocates a texture the other never fills, or worse, the latch runs for a chain that has
// no consumer and every user pays a full-resolution copy per frame for nothing.
bool AnyStageNeedsWorldCapture(const AnaxConfig& config);

// The canonical config-file name for a stage ("acestonemap", "bloom", ...), suitable for
// writing back into an `effect=` line. Returns "none" for EffectKind::None.
const char* EffectNameFor(EffectKind stage);

// Parses an INI-style config file at the given path. A missing file, a missing key, or an
// unparseable/out-of-range value falls back to the default for just that field and logs
// once via printf. Never fails outright - always returns a usable AnaxConfig. Exposed
// separately from GetAnaxConfig() below so it can be unit-tested without depending on this
// code actually being loaded as a DLL next to a real config file.
AnaxConfig ParseConfigFile(const char* path);

// Reads opengl32_enhancer.ini from the same directory as this DLL, once, and caches the result
// for the rest of the process lifetime (later calls are cheap and return the same object).
const AnaxConfig& GetAnaxConfig();

// The same cached object, writable. This exists for the standalone config editor (see
// config_editor.cpp), which edits settings live and re-runs the pipeline to show the result:
// post_effects.cpp re-reads the config at the top of every ApplySelectedEffect() call, so an
// edit here is visible on the very next frame with no reload step. Inside the DLL nothing writes
// through this - the game's config is read once from the ini and left alone - so callers there
// should keep using GetAnaxConfig() and let the compiler enforce that.
AnaxConfig& GetMutableAnaxConfig();
