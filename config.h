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

    // Forces trilinear + anisotropic filtering on the game's own mipmapped world/model
    // textures - see texture_filter.h. 0 (default) leaves every glTexParameter call the game
    // makes completely untouched; 1..16 is the anisotropy level to request (capped at runtime
    // to whatever the GPU/driver actually supports). Independent of the effect= pipeline.
    float anisotropy = 0.0f;

    // Overrides the game window's size at the moment it creates its GL context (see
    // window_override.h) - independent of the effect= pipeline, since it's a Win32 window
    // property, not a rendered stage. Both default to 0, meaning "leave the game's own window
    // size alone"; the override only applies when BOTH are set to a positive value, since a
    // window size is a single (width, height) pair, and honoring just one would mean guessing
    // the other instead of using what's actually configured.
    int windowWidth = 0;
    int windowHeight = 0;

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
