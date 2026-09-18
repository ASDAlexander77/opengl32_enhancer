// See config_writer.h.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config_writer.h"

namespace {

// Every key the editor can change, and how to render its current value. Keys absent from this
// table are ones the editor never touches, and their lines are copied through untouched.
// Kept in the same order as the shipped ini so an appended block reads sensibly.
const char* const kManagedKeys[] = {
    "effect",
    "scale",
    "bloomThreshold",
    "bloomIntensity",
    "acesStrength",
    "lutPath",
    "lutStrength",
    "vignetteIntensity",
    "vignetteRadius",
    "ssaoRadius",
    "ssaoIntensity",
    "ssaoBias",
    "dofFocusDistance",
    "dofFocusRange",
    "dofBlurStrength",
    "gamma",
    "brightness",
    "depthVignetteIntensity",
    "depthVignetteThreshold",
    "chromaticAberrationStrength",
    "taaBlend",
    "shimmerSuppression",
    "sharpness",
    "fsrDenoise",
    "fsrFilmGrain",
    "ditherStrength",
    "nrIntensity",
    "nrPasses",
    "nrColorStrength",
    "nrTonePreservation",
    "nrGrainPreservation",
    "localStructureStrength",
    "localToneStrength",
    "fxIndicator",
    "anisotropy",
    "textureEffect",
};
const int kManagedKeyCount = (int)(sizeof(kManagedKeys) / sizeof(kManagedKeys[0]));

std::string FormatStageList(const AnaxConfig& config) {
    if (config.stageCount == 0) {
        return "none";
    }
    std::string out;
    for (int i = 0; i < config.stageCount; ++i) {
        if (i > 0) {
            out += ", ";
        }
        out += EffectNameFor(config.stages[i]);
    }
    return out;
}

std::string FormatFloat(float value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f", value);
    return buf;
}

// Renders the current value for `key`, or returns false if this key is not one we manage.
bool FormatValueFor(const AnaxConfig& config, const char* key, std::string& out) {
    if (strcmp(key, "effect") == 0)                      { out = FormatStageList(config); return true; }
    if (strcmp(key, "scale") == 0)                       { out = FormatFloat(config.scale); return true; }
    if (strcmp(key, "bloomThreshold") == 0)              { out = FormatFloat(config.bloomThreshold); return true; }
    if (strcmp(key, "bloomIntensity") == 0)              { out = FormatFloat(config.bloomIntensity); return true; }
    if (strcmp(key, "acesStrength") == 0)                { out = FormatFloat(config.acesStrength); return true; }
    if (strcmp(key, "lutPath") == 0)                     { out = config.lutPath; return true; }
    if (strcmp(key, "lutStrength") == 0)                 { out = FormatFloat(config.lutStrength); return true; }
    if (strcmp(key, "vignetteIntensity") == 0)           { out = FormatFloat(config.vignetteIntensity); return true; }
    if (strcmp(key, "vignetteRadius") == 0)              { out = FormatFloat(config.vignetteRadius); return true; }
    if (strcmp(key, "ssaoRadius") == 0)                  { out = FormatFloat(config.ssaoRadius); return true; }
    if (strcmp(key, "ssaoIntensity") == 0)               { out = FormatFloat(config.ssaoIntensity); return true; }
    if (strcmp(key, "ssaoBias") == 0)                    { out = FormatFloat(config.ssaoBias); return true; }
    if (strcmp(key, "dofFocusDistance") == 0)            { out = FormatFloat(config.dofFocusDistance); return true; }
    if (strcmp(key, "dofFocusRange") == 0)               { out = FormatFloat(config.dofFocusRange); return true; }
    if (strcmp(key, "dofBlurStrength") == 0)             { out = FormatFloat(config.dofBlurStrength); return true; }
    if (strcmp(key, "gamma") == 0)                       { out = FormatFloat(config.gamma); return true; }
    if (strcmp(key, "brightness") == 0)                  { out = FormatFloat(config.brightness); return true; }
    if (strcmp(key, "depthVignetteIntensity") == 0)      { out = FormatFloat(config.depthVignetteIntensity); return true; }
    if (strcmp(key, "depthVignetteThreshold") == 0)      { out = FormatFloat(config.depthVignetteThreshold); return true; }
    if (strcmp(key, "chromaticAberrationStrength") == 0) { out = FormatFloat(config.chromaticAberrationStrength); return true; }
    if (strcmp(key, "taaBlend") == 0)                    { out = FormatFloat(config.taaBlend); return true; }
    if (strcmp(key, "shimmerSuppression") == 0)          { out = FormatFloat(config.shimmerSuppression); return true; }
    if (strcmp(key, "sharpness") == 0)                   { out = FormatFloat(config.sharpness); return true; }
    if (strcmp(key, "fsrDenoise") == 0)                  { out = config.fsrDenoise ? "1" : "0"; return true; }
    if (strcmp(key, "fsrFilmGrain") == 0)                { out = FormatFloat(config.fsrFilmGrain); return true; }
    if (strcmp(key, "ditherStrength") == 0)              { out = FormatFloat(config.ditherStrength); return true; }
    if (strcmp(key, "nrIntensity") == 0)                 { out = FormatFloat(config.nrIntensity); return true; }
    if (strcmp(key, "nrPasses") == 0)                    { out = std::to_string(config.nrPasses); return true; }
    if (strcmp(key, "nrColorStrength") == 0)             { out = FormatFloat(config.nrColorStrength); return true; }
    if (strcmp(key, "nrTonePreservation") == 0)          { out = FormatFloat(config.nrTonePreservation); return true; }
    if (strcmp(key, "nrGrainPreservation") == 0)         { out = FormatFloat(config.nrGrainPreservation); return true; }
    if (strcmp(key, "localStructureStrength") == 0)      { out = FormatFloat(config.localStructureStrength); return true; }
    if (strcmp(key, "localToneStrength") == 0)           { out = FormatFloat(config.localToneStrength); return true; }
    if (strcmp(key, "fxIndicator") == 0)                 { out = config.fxIndicator ? "1" : "0"; return true; }
    if (strcmp(key, "anisotropy") == 0)                  { out = FormatFloat(config.anisotropy); return true; }
    if (strcmp(key, "textureEffect") == 0)               { out = EffectNameFor(config.textureEffect); return true; }
    return false;
}

std::string TrimCopy(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// Splits "  key = value ; note" into its key, and the trailing comment (including the ';') if
// there is one. Returns false for a line that is blank, commented out, or has no '='.
bool SplitKeyLine(const std::string& line, std::string& outKey, std::string& outTrailingComment) {
    std::string trimmed = TrimCopy(line);
    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
        return false;
    }
    size_t eq = line.find('=');
    if (eq == std::string::npos) {
        return false;
    }
    outKey = TrimCopy(line.substr(0, eq));
    if (outKey.empty()) {
        return false;
    }

    std::string value = line.substr(eq + 1);
    size_t comment = value.find(';');
    outTrailingComment = (comment == std::string::npos) ? "" : value.substr(comment);
    // A trailing comment keeps the single space before it that the shipped ini uses, so
    // rewriting a line doesn't reflow the file's formatting.
    if (!outTrailingComment.empty()) {
        outTrailingComment = " " + TrimCopy(outTrailingComment);
    }
    return true;
}

}  // namespace

bool WriteConfigToIni(const char* path, const AnaxConfig& config) {
    std::vector<std::string> lines;
    {
        FILE* in = fopen(path, "rb");
        if (in == nullptr) {
            printf("config_writer: could not open '%s' for reading\n", path);
            return false;
        }
        std::string current;
        int c;
        while ((c = fgetc(in)) != EOF) {
            if (c == '\n') {
                lines.push_back(current);
                current.clear();
            } else if (c != '\r') {
                current.push_back((char)c);
            }
        }
        if (!current.empty()) {
            lines.push_back(current);
        }
        fclose(in);
    }

    bool seen[kManagedKeyCount] = {};

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string key;
        std::string trailingComment;
        if (!SplitKeyLine(lines[i], key, trailingComment)) {
            continue;
        }
        // `effects` is accepted as a plural alias on read (see config.cpp), so a file using that
        // spelling must be rewritten through the same "effect" entry rather than left stale.
        const char* lookupKey = (key == "effects") ? "effect" : key.c_str();
        std::string value;
        if (!FormatValueFor(config, lookupKey, value)) {
            continue;
        }
        for (int k = 0; k < kManagedKeyCount; ++k) {
            if (strcmp(kManagedKeys[k], lookupKey) == 0) {
                seen[k] = true;
                break;
            }
        }
        lines[i] = key + "=" + value + trailingComment;
    }

    std::vector<std::string> missing;
    for (int k = 0; k < kManagedKeyCount; ++k) {
        if (seen[k]) {
            continue;
        }
        std::string value;
        if (FormatValueFor(config, kManagedKeys[k], value)) {
            missing.push_back(std::string(kManagedKeys[k]) + "=" + value);
        }
    }
    if (!missing.empty()) {
        lines.push_back("");
        lines.push_back("; --- Added by the config editor: settings that were not present above.");
        for (size_t i = 0; i < missing.size(); ++i) {
            lines.push_back(missing[i]);
        }
    }

    FILE* out = fopen(path, "wb");
    if (out == nullptr) {
        printf("config_writer: could not open '%s' for writing\n", path);
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        fprintf(out, "%s\n", lines[i].c_str());
    }
    fclose(out);
    printf("config_writer: saved %s\n", path);
    return true;
}
