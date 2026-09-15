#pragma once

// Selects and configures the post-process effect applied in wglSwapBuffers (see
// post_effects.h). Read from an INI-style anax_enhancer.ini file next to this DLL - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md for the file format.

enum class EffectKind {
    None,
    Invert,
    Bilinear,
    NVScaler,
    NVSharpen,
    TAA,
    HdrLook,
};

struct AnaxConfig {
    EffectKind effect = EffectKind::None;
    float sharpness = 0.5f;
    float scale = 1.0f;
    float taaBlend = 0.5f;
    float hdrStrength = 0.5f;
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
