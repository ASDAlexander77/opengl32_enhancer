// See config.h. Reads anax_enhancer.ini (INI-style key=value lines, ';'/'#' comments,
// inline ';' comments) next to this DLL.
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
    return snprintf(outPath, outPathSize, "%sanax_enhancer.ini", modulePath) > 0;
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

EffectKind ParseEffect(const char* value) {
    if (strcmp(value, "none") == 0) return EffectKind::None;
    if (strcmp(value, "invert") == 0) return EffectKind::Invert;
    if (strcmp(value, "bilinear") == 0) return EffectKind::Bilinear;
    if (strcmp(value, "nvscaler") == 0) return EffectKind::NVScaler;
    if (strcmp(value, "nvsharpen") == 0) return EffectKind::NVSharpen;
    printf("[opengl32_enh_cpp] config: unrecognized effect '%s', falling back to none\n", value);
    return EffectKind::None;
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

AnaxConfig ParseConfigFile(const char* path) {
    AnaxConfig config;

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

        if (strcmp(key, "effect") == 0) {
            config.effect = ParseEffect(value);
        } else if (strcmp(key, "sharpness") == 0) {
            config.sharpness = ParseClampedFloat(value, 0.0f, 1.0f, config.sharpness, "sharpness");
        } else if (strcmp(key, "scale") == 0) {
            config.scale = ParseClampedFloat(value, 0.5f, 1.0f, config.scale, "scale");
        }
    }
    fclose(f);

    printf("[opengl32_enh_cpp] config: loaded from '%s' (effect=%d, sharpness=%.3f, scale=%.3f)\n",
           path, static_cast<int>(config.effect), config.sharpness, config.scale);
    return config;
}

const AnaxConfig& GetAnaxConfig() {
    static const AnaxConfig config = LoadConfig();
    return config;
}
