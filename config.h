#pragma once

// Selects and configures the post-process effect applied in wglSwapBuffers (see
// post_effects.h). Read from an INI-style anax_enhancer.ini file next to this DLL - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md for the file format.

enum class EffectKind {
    None,
    Invert,
    Bilinear,
    NVScaler,
};

struct AnaxConfig {
    // effect: the primary/base effect - upscale or misc, runs first against the raw capture.
    EffectKind effect = EffectKind::None;
    float scale = 1.0f;

    // Everything below is an addon pass layered after `effect` above (see post_effects.cpp),
    // each independently optional, always applied in this fixed order:
    // effect -> AcesToneMap -> Bloom -> Sharpen -> LutGrading -> Taa -> Dither -> (real swap).
    bool enableAcesToneMap = false;
    float acesStrength = 0.5f;

    bool enableBloom = false;
    float bloomThreshold = 0.8f;
    float bloomIntensity = 0.5f;

    bool enableSharpen = false;
    float sharpness = 0.5f;

    bool enableLutGrading = false;
    char lutPath[256] = "";
    float lutStrength = 1.0f;

    bool enableTaa = false;
    float taaBlend = 0.5f;

    bool enableDither = false;
    float ditherStrength = 1.0f;
};

// Parses an INI-style config file at the given path. A missing file, a missing key, or an
// unparseable/out-of-range value falls back to the default for just that field and logs
// once via printf. Never fails outright - always returns a usable AnaxConfig. Exposed
// separately from GetAnaxConfig() below so it can be unit-tested without depending on this
// code actually being loaded as a DLL next to a real config file.
AnaxConfig ParseConfigFile(const char* path);

// Reads anax_enhancer.ini from the same directory as this DLL, once, and caches the result
// for the rest of the process lifetime (later calls are cheap and return the same object).
const AnaxConfig& GetAnaxConfig();
