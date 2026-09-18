// See config.h. Reads opengl32_enhancer.ini (INI-style key=value lines, ';'/'#' comments,
// inline ';' comments) next to this DLL.
//
// The `effect` key is an ordered, comma-separated list of stage names - membership decides
// what runs and position decides when. The older `enableXxx=true/false` booleans are still
// honored for config files written before that (see ApplyEnableOverrides below), but they
// can only say whether a stage runs, not where in the chain it lands.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

// Same no-<windows.h> discipline as the rest of this DLL - see wrapper.cpp's header
// comment / generators/gen_wrapper_cpp.py: <windows.h> declares dllimport wgl*/gl* names
// that collide with wrapper.cpp's dllexport definitions of the same names.
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HMODULE;

extern "C" {
    __declspec(dllimport) BOOL __stdcall GetModuleHandleExA(DWORD dwFlags, const char* lpModuleName, HMODULE* phModule);
    __declspec(dllimport) DWORD __stdcall GetModuleFileNameA(HMODULE hModule, char* lpFilename, DWORD nSize);
}

namespace {

const DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 0x00000004;
const DWORD GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x00000002;

// Any function defined in this module works as the address anchor for
// GetModuleHandleExA(..._FROM_ADDRESS, ...) below - it identifies "the module this code
// is running as", which is this DLL when linked into the proxy, or the test .exe itself
// when linked into config_test.
void AddressAnchor() {}

bool GetIniPathNextToThisModule(char* outPath, size_t outPathSize) {
    HMODULE hModule = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<const char*>(&AddressAnchor), &hModule)) {
        return false;
    }
    char modulePath[512];
    DWORD len = GetModuleFileNameA(hModule, modulePath, sizeof(modulePath));
    if (len == 0 || len >= sizeof(modulePath)) {
        return false;
    }
    char* lastSlash = nullptr;
    for (char* p = modulePath; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            lastSlash = p;
        }
    }
    if (lastSlash == nullptr) {
        return false;
    }
    *(lastSlash + 1) = '\0';
    return snprintf(outPath, outPathSize, "%sopengl32_enhancer.ini", modulePath) > 0;
}

void Trim(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') {
        ++start;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

char LowerChar(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

// Stage names in the ini are matched case-insensitively so `effect=AcesToneMap` and
// `effect=acestonemap` both work - the canonical spelling (EffectNameFor) is the readable
// mixed-case-free lowercase form, but nobody should have to remember that.
bool EqualsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (LowerChar(*a) != LowerChar(*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

struct StageName {
    EffectKind kind;
    const char* name;
};

// The canonical name for each stage, in EffectKind order. EffectNameFor() indexes this
// directly, so it must stay in sync with the enum.
const StageName kStageNames[] = {
    {EffectKind::None,                "none"},
    {EffectKind::Invert,              "invert"},
    {EffectKind::Bilinear,            "bilinear"},
    {EffectKind::NVScaler,            "nvscaler"},
    {EffectKind::AcesToneMap,         "acestonemap"},
    {EffectKind::Bloom,               "bloom"},
    {EffectKind::Sharpen,             "sharpen"},
    {EffectKind::LutGrading,          "lutgrading"},
    {EffectKind::Vignette,            "vignette"},
    {EffectKind::ChromaticAberration, "chromaticaberration"},
    {EffectKind::Taa,                 "taa"},
    {EffectKind::Dither,              "dither"},
    {EffectKind::Smaa,                "smaa"},
    {EffectKind::Fsr,                 "fsr"},
    {EffectKind::Cas,                 "cas"},
    {EffectKind::Nr,                  "nr"},
    {EffectKind::LocalContrast,       "localcontrast"},
    {EffectKind::DepthVignette,       "depthvignette"},
    {EffectKind::Ssao,                "ssao"},
    {EffectKind::Gamma,               "gamma"},
};
const int kStageNameCount = (int)(sizeof(kStageNames) / sizeof(kStageNames[0]));

// Names that older config files used for stages that have since been renamed. Accepted on
// input, but never written back out by EffectNameFor().
const StageName kStageAliases[] = {
    {EffectKind::AcesToneMap, "hdrlook"},    // was a primary effect named "hdrlook"
    {EffectKind::Sharpen,     "nvsharpen"},  // was a primary effect named "nvsharpen"
};
const int kStageAliasCount = (int)(sizeof(kStageAliases) / sizeof(kStageAliases[0]));

bool ParseStageName(const char* value, EffectKind& outKind) {
    for (int i = 0; i < kStageNameCount; ++i) {
        if (EqualsIgnoreCase(value, kStageNames[i].name)) {
            outKind = kStageNames[i].kind;
            return true;
        }
    }
    for (int i = 0; i < kStageAliasCount; ++i) {
        if (EqualsIgnoreCase(value, kStageAliases[i].name)) {
            outKind = kStageAliases[i].kind;
            printf("[opengl32_enh_cpp] config: effect '%s' is now called '%s', treating as such\n",
                   value, EffectNameFor(outKind));
            return true;
        }
    }
    return false;
}

void AppendStage(AnaxConfig& config, EffectKind stage) {
    if (stage == EffectKind::None) {
        return;  // "none" is how a config says "no stages here", not a stage of its own.
    }
    if (config.stageCount >= kMaxEffectStages) {
        printf("[opengl32_enh_cpp] config: more than %d effects listed, ignoring '%s' and "
               "anything after it\n", kMaxEffectStages, EffectNameFor(stage));
        return;
    }
    config.stages[config.stageCount++] = stage;
}

void RemoveStage(AnaxConfig& config, EffectKind stage) {
    int write = 0;
    for (int read = 0; read < config.stageCount; ++read) {
        if (config.stages[read] != stage) {
            config.stages[write++] = config.stages[read];
        }
    }
    config.stageCount = write;
}

// Parses the comma-separated `effect` value into config.stages, replacing whatever was there
// before (the key is authoritative - a second `effect=` line wins over the first, and over
// any stages a legacy enableXxx flag had already contributed, which is why the enable flags
// are collected during parsing and applied only once the whole file has been read).
void ParseEffectList(AnaxConfig& config, char* value) {
    config.stageCount = 0;

    char* cursor = value;
    while (cursor != nullptr && *cursor != '\0') {
        char* comma = strchr(cursor, ',');
        if (comma != nullptr) {
            *comma = '\0';
        }
        Trim(cursor);
        if (*cursor != '\0') {
            EffectKind kind = EffectKind::None;
            if (ParseStageName(cursor, kind)) {
                AppendStage(config, kind);
            } else {
                printf("[opengl32_enh_cpp] config: unrecognized effect '%s', skipping it\n", cursor);
            }
        }
        cursor = (comma != nullptr) ? comma + 1 : nullptr;
    }
}

void CopyStringValue(char* dest, size_t destSize, const char* value, const char* key) {
    if (strlen(value) >= destSize) {
        printf("[opengl32_enh_cpp] config: '%s' value '%s' is too long (max %zu chars), ignoring\n",
               key, value, destSize - 1);
        return;
    }
    strcpy(dest, value);
}

bool ParseBool(const char* value, bool fallback, const char* key) {
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) return true;
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) return false;
    printf("[opengl32_enh_cpp] config: '%s' value '%s' is not a bool, using default %s\n",
           key, value, fallback ? "true" : "false");
    return fallback;
}

float ParseClampedFloat(const char* value, float minValue, float maxValue, float fallback, const char* key) {
    char* end = nullptr;
    float parsed = strtof(value, &end);
    if (end == value) {
        printf("[opengl32_enh_cpp] config: '%s' value '%s' is not a number, using default %.3f\n", key, value, fallback);
        return fallback;
    }
    if (parsed < minValue || parsed > maxValue) {
        float clamped = parsed < minValue ? minValue : maxValue;
        printf("[opengl32_enh_cpp] config: '%s' value %.3f out of range [%.3f, %.3f], clamping to %.3f\n",
               key, parsed, minValue, maxValue, clamped);
        return clamped;
    }
    return parsed;
}

// base is strtol's: 10 for ordinary counts, 0 for values conventionally written in hex (a
// virtual-key code), where it accepts a 0x prefix and plain decimal alike.
int ParseClampedInt(const char* value, int minValue, int maxValue, int fallback, const char* key,
                     int base = 10) {
    char* end = nullptr;
    long parsed = strtol(value, &end, base);
    if (end == value) {
        printf("[opengl32_enh_cpp] config: '%s' value '%s' is not an integer, using default %d\n", key, value, fallback);
        return fallback;
    }
    if (parsed < minValue || parsed > maxValue) {
        int clamped = parsed < minValue ? minValue : maxValue;
        printf("[opengl32_enh_cpp] config: '%s' value %ld out of range [%d, %d], clamping to %d\n",
               key, parsed, minValue, maxValue, clamped);
        return clamped;
    }
    return (int)parsed;
}

// One legacy enableXxx=true/false key that a config file set. Collected while parsing and
// applied afterwards (see ApplyEnableOverrides) so the result doesn't depend on whether the
// `effect` line happened to come before or after the enable flags in the file.
struct EnableOverride {
    bool seen = false;
    bool value = false;
};

struct EnableKey {
    EffectKind kind;
    const char* key;
};

// The legacy enableXxx keys, listed in the order the pipeline used to hard-code. A config
// file that only uses these keys therefore reproduces exactly the old fixed ordering, which
// is the point: such a file never got to express an order, so the order it used to get is
// the only correct thing to give it.
const EnableKey kEnableKeys[] = {
    {EffectKind::Bloom,               "enableBloom"},
    {EffectKind::AcesToneMap,         "enableAcesToneMap"},
    {EffectKind::LutGrading,          "enableLutGrading"},
    {EffectKind::Vignette,            "enableVignette"},
    {EffectKind::ChromaticAberration, "enableChromaticAberration"},
    {EffectKind::Taa,                 "enableTaa"},
    {EffectKind::Sharpen,             "enableSharpen"},
    {EffectKind::Dither,              "enableDither"},
};
const int kEnableKeyCount = (int)(sizeof(kEnableKeys) / sizeof(kEnableKeys[0]));

void ApplyEnableOverrides(AnaxConfig& config, const EnableOverride* overrides) {
    bool warned = false;
    for (int i = 0; i < kEnableKeyCount; ++i) {
        if (!overrides[i].seen) {
            continue;
        }
        if (!warned) {
            printf("[opengl32_enh_cpp] config: 'enableXxx' keys still work but can't express "
                   "ordering - prefer listing stages in 'effect=' instead\n");
            warned = true;
        }
        bool present = HasEffectStage(config, kEnableKeys[i].kind);
        if (overrides[i].value && !present) {
            AppendStage(config, kEnableKeys[i].kind);
        } else if (!overrides[i].value && present) {
            RemoveStage(config, kEnableKeys[i].kind);
        }
    }
}

// Renders config.stages as "bilinear, bloom, dither" for the summary log line.
void FormatStageList(const AnaxConfig& config, char* out, size_t outSize) {
    if (config.stageCount == 0) {
        snprintf(out, outSize, "none");
        return;
    }
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < config.stageCount && used + 1 < outSize; ++i) {
        int written = snprintf(out + used, outSize - used, "%s%s",
                               i == 0 ? "" : ", ", EffectNameFor(config.stages[i]));
        if (written <= 0) {
            break;
        }
        used += (size_t)written;
    }
}

AnaxConfig LoadConfig() {
    AnaxConfig config;

    char iniPath[512];
    if (!GetIniPathNextToThisModule(iniPath, sizeof(iniPath))) {
        printf("[opengl32_enh_cpp] config: FAILED to determine module directory, using defaults (effect=none)\n");
        return config;
    }
    return ParseConfigFile(iniPath);
}

}  // namespace

bool HasEffectStage(const AnaxConfig& config, EffectKind stage) {
    for (int i = 0; i < config.stageCount; ++i) {
        if (config.stages[i] == stage) {
            return true;
        }
    }
    return false;
}

const char* EffectNameFor(EffectKind stage) {
    int index = (int)stage;
    if (index < 0 || index >= kStageNameCount) {
        return "unknown";
    }
    return kStageNames[index].name;
}

AnaxConfig ParseConfigFile(const char* path) {
    AnaxConfig config;
    EnableOverride overrides[kEnableKeyCount];

    FILE* f = fopen(path, "r");
    if (f == nullptr) {
        printf("[opengl32_enh_cpp] config: no config file at '%s', using defaults (effect=none)\n", path);
        return config;
    }

    char line[256];
    while (fgets(line, sizeof(line), f) != nullptr) {
        Trim(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') {
            continue;
        }
        char* eq = strchr(line, '=');
        if (eq == nullptr) {
            continue;
        }
        *eq = '\0';
        char* key = line;
        char* value = eq + 1;
        char* comment = strchr(value, ';');
        if (comment != nullptr) {
            *comment = '\0';
        }
        Trim(key);
        Trim(value);

        // `effects` is accepted alongside `effect` purely because the value is a list now and
        // the plural is the spelling people reach for.
        if (strcmp(key, "effect") == 0 || strcmp(key, "effects") == 0) {
            ParseEffectList(config, value);
            continue;
        }

        bool handledAsEnableKey = false;
        for (int i = 0; i < kEnableKeyCount; ++i) {
            if (strcmp(key, kEnableKeys[i].key) == 0) {
                overrides[i].seen = true;
                overrides[i].value = ParseBool(value, true, kEnableKeys[i].key);
                handledAsEnableKey = true;
                break;
            }
        }
        if (handledAsEnableKey) {
            continue;
        }

        if (strcmp(key, "sharpness") == 0) {
            config.sharpness = ParseClampedFloat(value, 0.0f, 1.0f, config.sharpness, "sharpness");
        } else if (strcmp(key, "scale") == 0) {
            config.scale = ParseClampedFloat(value, 0.5f, 1.0f, config.scale, "scale");
        } else if (strcmp(key, "taaBlend") == 0) {
            config.taaBlend = ParseClampedFloat(value, 0.0f, 1.0f, config.taaBlend, "taaBlend");
        } else if (strcmp(key, "acesStrength") == 0) {
            config.acesStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.acesStrength, "acesStrength");
        } else if (strcmp(key, "bloomThreshold") == 0) {
            config.bloomThreshold = ParseClampedFloat(value, 0.0f, 1.0f, config.bloomThreshold, "bloomThreshold");
        } else if (strcmp(key, "bloomIntensity") == 0) {
            config.bloomIntensity = ParseClampedFloat(value, 0.0f, 2.0f, config.bloomIntensity, "bloomIntensity");
        } else if (strcmp(key, "enableHdrLook") == 0) {
            // Renamed to enableAcesToneMap, which is itself now legacy - fold it onto the
            // same override slot so both spellings behave identically.
            printf("[opengl32_enh_cpp] config: 'enableHdrLook' is now the 'acestonemap' effect, treating as such\n");
            for (int i = 0; i < kEnableKeyCount; ++i) {
                if (kEnableKeys[i].kind == EffectKind::AcesToneMap) {
                    overrides[i].seen = true;
                    overrides[i].value = ParseBool(value, true, "enableHdrLook");
                    break;
                }
            }
        } else if (strcmp(key, "hdrStrength") == 0) {
            printf("[opengl32_enh_cpp] config: 'hdrStrength' is now 'acesStrength', treating as such\n");
            config.acesStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.acesStrength, "hdrStrength");
        } else if (strcmp(key, "lutPath") == 0) {
            CopyStringValue(config.lutPath, sizeof(config.lutPath), value, "lutPath");
        } else if (strcmp(key, "lutStrength") == 0) {
            config.lutStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.lutStrength, "lutStrength");
        } else if (strcmp(key, "vignetteIntensity") == 0) {
            config.vignetteIntensity = ParseClampedFloat(value, 0.0f, 1.0f, config.vignetteIntensity, "vignetteIntensity");
        } else if (strcmp(key, "vignetteRadius") == 0) {
            config.vignetteRadius = ParseClampedFloat(value, 0.0f, 1.0f, config.vignetteRadius, "vignetteRadius");
        } else if (strcmp(key, "chromaticAberrationStrength") == 0) {
            config.chromaticAberrationStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.chromaticAberrationStrength, "chromaticAberrationStrength");
        } else if (strcmp(key, "fsrDenoise") == 0) {
            config.fsrDenoise = ParseBool(value, config.fsrDenoise, "fsrDenoise");
        } else if (strcmp(key, "fsrFilmGrain") == 0) {
            config.fsrFilmGrain = ParseClampedFloat(value, 0.0f, 1.0f, config.fsrFilmGrain, "fsrFilmGrain");
        } else if (strcmp(key, "gamma") == 0) {
            // The low clamp is 0.5, not 0: the shader divides by this, so 0 would be an
            // infinity sprayed across the whole frame. See gamma.h.
            config.gamma = ParseClampedFloat(value, 0.5f, 3.0f, config.gamma, "gamma");
        } else if (strcmp(key, "brightness") == 0) {
            config.brightness = ParseClampedFloat(value, 0.0f, 2.0f, config.brightness, "brightness");
        } else if (strcmp(key, "ditherStrength") == 0) {
            config.ditherStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.ditherStrength, "ditherStrength");
        } else if (strcmp(key, "nrIntensity") == 0) {
            config.nrIntensity = ParseClampedFloat(value, 0.0f, 1.0f, config.nrIntensity, "nrIntensity");
        } else if (strcmp(key, "nrPasses") == 0) {
            config.nrPasses = ParseClampedInt(value, 1, 4, config.nrPasses, "nrPasses");
        } else if (strcmp(key, "nrColorStrength") == 0) {
            config.nrColorStrength = ParseClampedFloat(value, 0.0f, 1.0f, config.nrColorStrength, "nrColorStrength");
        } else if (strcmp(key, "nrTonePreservation") == 0) {
            config.nrTonePreservation = ParseClampedFloat(value, 0.0f, 1.0f, config.nrTonePreservation, "nrTonePreservation");
        } else if (strcmp(key, "nrGrainPreservation") == 0) {
            config.nrGrainPreservation = ParseClampedFloat(value, 0.0f, 1.0f, config.nrGrainPreservation, "nrGrainPreservation");
        } else if (strcmp(key, "localStructureStrength") == 0) {
            config.localStructureStrength = ParseClampedFloat(value, 0.0f, 2.0f, config.localStructureStrength, "localStructureStrength");
        } else if (strcmp(key, "localToneStrength") == 0) {
            config.localToneStrength = ParseClampedFloat(value, 0.0f, 2.0f, config.localToneStrength, "localToneStrength");
        } else if (strcmp(key, "shimmerSuppression") == 0) {
            config.shimmerSuppression = ParseClampedFloat(value, 0.0f, 1.0f, config.shimmerSuppression, "shimmerSuppression");
        } else if (strcmp(key, "depthVignetteIntensity") == 0) {
            config.depthVignetteIntensity = ParseClampedFloat(value, 0.0f, 1.0f, config.depthVignetteIntensity, "depthVignetteIntensity");
        } else if (strcmp(key, "depthVignetteThreshold") == 0) {
            config.depthVignetteThreshold = ParseClampedFloat(value, 0.0f, 1.0f, config.depthVignetteThreshold, "depthVignetteThreshold");
        } else if (strcmp(key, "ssaoRadius") == 0) {
            // World units, so the upper bound is deliberately far above the 0..1 most values
            // here use - see config.h. 512 is well past useful for a Quake II-scale map and
            // exists only to catch a typo'd order of magnitude.
            config.ssaoRadius = ParseClampedFloat(value, 0.1f, 512.0f, config.ssaoRadius, "ssaoRadius");
        } else if (strcmp(key, "ssaoIntensity") == 0) {
            config.ssaoIntensity = ParseClampedFloat(value, 0.0f, 1.0f, config.ssaoIntensity, "ssaoIntensity");
        } else if (strcmp(key, "ssaoBias") == 0) {
            config.ssaoBias = ParseClampedFloat(value, 0.0f, 16.0f, config.ssaoBias, "ssaoBias");
        } else if (strcmp(key, "frameDumpKey") == 0) {
            // A Windows virtual-key code, so the sensible way to write it in an ini is hex
            // (0x7A = F11). strtol with base 0 accepts both that and plain decimal.
            config.frameDumpKey = ParseClampedInt(value, 0, 0xFE, config.frameDumpKey, "frameDumpKey", 0);
        } else if (strcmp(key, "frameDumpPath") == 0) {
            CopyStringValue(config.frameDumpPath, sizeof(config.frameDumpPath), value, "frameDumpPath");
        } else if (strcmp(key, "fxIndicator") == 0) {
            config.fxIndicator = ParseBool(value, config.fxIndicator, "fxIndicator");
        } else if (strcmp(key, "anisotropy") == 0) {
            config.anisotropy = ParseClampedFloat(value, 0.0f, 16.0f, config.anisotropy, "anisotropy");
        } else if (strcmp(key, "textureSharpen") == 0) {
            // Pre-textureEffect spelling: textureSharpen=1/0 meant sharpen/off.
            bool on = ParseBool(value, config.textureEffect == EffectKind::Sharpen, "textureSharpen");
            printf("[opengl32_enh_cpp] config: 'textureSharpen' is now 'textureEffect=sharpen/none', treating as such\n");
            config.textureEffect = on ? EffectKind::Sharpen : EffectKind::None;
        } else if (strcmp(key, "textureEffect") == 0) {
            EffectKind kind;
            if (!ParseStageName(value, kind) ||
                (kind != EffectKind::None && kind != EffectKind::Sharpen &&
                 kind != EffectKind::Cas && kind != EffectKind::Invert)) {
                printf("[opengl32_enh_cpp] config: 'textureEffect' value '%s' is not one of "
                       "none/sharpen/cas/invert, keeping default\n", value);
            } else {
                config.textureEffect = kind;
            }
        }
    }
    fclose(f);

    ApplyEnableOverrides(config, overrides);

    char stageList[512];
    FormatStageList(config, stageList, sizeof(stageList));
    printf("[opengl32_enh_cpp] config: loaded from '%s' (effect=%s; scale=%.3f, acesStrength=%.3f, "
           "bloomThreshold=%.3f, bloomIntensity=%.3f, sharpness=%.3f, lutPath='%s', lutStrength=%.3f, "
           "vignetteIntensity=%.3f, vignetteRadius=%.3f, chromaticAberrationStrength=%.3f, "
           "taaBlend=%.3f, ditherStrength=%.3f, fsrDenoise=%s, fsrFilmGrain=%.3f, "
           "nrIntensity=%.3f, nrPasses=%d, nrColorStrength=%.3f, nrTonePreservation=%.3f, "
           "nrGrainPreservation=%.3f, localStructureStrength=%.3f, localToneStrength=%.3f, "
           "shimmerSuppression=%.3f, depthVignetteIntensity=%.3f, depthVignetteThreshold=%.3f, "
           "ssaoRadius=%.3f, ssaoIntensity=%.3f, ssaoBias=%.3f, "
           "gamma=%.3f, brightness=%.3f, "
           "frameDumpKey=0x%02X, frameDumpPath='%s', "
           "fxIndicator=%s, anisotropy=%.3f, textureEffect=%s)\n",
           path, stageList, config.scale, config.acesStrength,
           config.bloomThreshold, config.bloomIntensity, config.sharpness,
           config.lutPath, config.lutStrength,
           config.vignetteIntensity, config.vignetteRadius, config.chromaticAberrationStrength,
           config.taaBlend, config.ditherStrength, config.fsrDenoise ? "true" : "false",
           config.fsrFilmGrain,
           config.nrIntensity, config.nrPasses, config.nrColorStrength, config.nrTonePreservation,
           config.nrGrainPreservation, config.localStructureStrength, config.localToneStrength,
           config.shimmerSuppression, config.depthVignetteIntensity, config.depthVignetteThreshold,
           config.ssaoRadius, config.ssaoIntensity, config.ssaoBias,
           config.gamma, config.brightness,
           config.frameDumpKey, config.frameDumpPath,
           config.fxIndicator ? "true" : "false", config.anisotropy,
           EffectNameFor(config.textureEffect));
    return config;
}

AnaxConfig& GetMutableAnaxConfig() {
    static AnaxConfig config = LoadConfig();
    return config;
}

const AnaxConfig& GetAnaxConfig() {
    return GetMutableAnaxConfig();
}
