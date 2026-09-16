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
};

// Plenty of headroom for the 11 real stages, even if a config lists some of them twice.
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
    float scale = 1.0f;                        // Bilinear, NVScaler
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
