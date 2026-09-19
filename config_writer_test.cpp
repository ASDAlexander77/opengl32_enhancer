// Checks WriteConfigToIni() rewrites values IN PLACE without eating the file around them - see
// config_writer.h. This matters more than a normal round-trip test: opengl32_enhancer.ini is
// mostly documentation, every setting explained inline next to its value, and the config editor
// writes over that file whenever anyone presses Save. A writer that quietly dropped the prose,
// or that "helpfully" uncommented one of the `;effect=...` preset lines, would destroy the
// project's actual settings reference on the first save and no round-trip assertion on the
// parsed values alone would notice.
//
// Runs against the REAL shipped ini (copied next to this test's .exe by CMakeLists.txt) rather
// than a minimal fixture, because the things most likely to break the writer - inline trailing
// comments, commented-out preset lines, blank-line formatting - are exactly what the real file
// is full of and a hand-written fixture would understate.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "config_writer.h"

namespace {

const char* kSourceIni = "opengl32_enhancer.ini";
const char* kWorkIni = "config_writer_test_work.ini";

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    return condition;
}

std::vector<std::string> ReadLines(const char* path) {
    std::vector<std::string> lines;
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        return lines;
    }
    std::string current;
    int c;
    while ((c = fgetc(f)) != EOF) {
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
    fclose(f);
    return lines;
}

bool CopyFile(const char* from, const char* to) {
    std::vector<std::string> lines = ReadLines(from);
    if (lines.empty()) {
        return false;
    }
    FILE* f = fopen(to, "wb");
    if (f == nullptr) {
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        fprintf(f, "%s\n", lines[i].c_str());
    }
    fclose(f);
    return true;
}

int CountCommentLines(const std::vector<std::string>& lines) {
    int count = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        size_t firstNonSpace = lines[i].find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos &&
            (lines[i][firstNonSpace] == ';' || lines[i][firstNonSpace] == '#')) {
            count++;
        }
    }
    return count;
}

bool AnyLineContains(const std::vector<std::string>& lines, const char* needle) {
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    bool ok = true;

    if (!CopyFile(kSourceIni, kWorkIni)) {
        printf("FAIL: could not stage a working copy of '%s'\n", kSourceIni);
        return 1;
    }

    std::vector<std::string> before = ReadLines(kWorkIni);
    int commentsBefore = CountCommentLines(before);
    ok = Check(commentsBefore > 50, "the shipped ini really is comment-heavy (test is meaningful)") && ok;

    AnaxConfig config = ParseConfigFile(kWorkIni);

    // Change one of each kind the writer formats differently: a float, an int, a bool, an enum
    // name and the ordered stage list.
    config.ssaoRadius = 37.5f;
    // dof's settings are world-unit denominated like ssaoRadius, and a stage whose settings the
    // editor silently drops on save is worse than one that isn't there - so they round-trip too.
    config.dofFocusDistance = 125.0f;
    config.dofFocusRange = 48.0f;
    config.dofBlurStrength = 0.65f;
    config.fogStart = 310.0f;
    config.fogIntensity = 0.42f;
    config.fogColorG = 0.33f;
    config.shaftsIntensity = 0.27f;
    config.shaftsDecay = 0.88f;
    config.ssrIntensity = 0.31f;
    config.ssrMaxDistance = 640.0f;
    config.ssrUpThreshold = 0.55f;
    config.gamma = 2.2f;
    config.brightness = 1.4f;
    config.nrPasses = 3;
    config.fxIndicator = !config.fxIndicator;
    bool expectedIndicator = config.fxIndicator;
    config.textureEffect = EffectKind::Invert;
    config.stageCount = 3;
    config.stages[0] = EffectKind::Ssao;
    config.stages[1] = EffectKind::Bloom;
    config.stages[2] = EffectKind::Dither;

    ok = Check(WriteConfigToIni(kWorkIni, config), "WriteConfigToIni() reported success") && ok;

    std::vector<std::string> after = ReadLines(kWorkIni);
    int commentsAfter = CountCommentLines(after);

    // The writer may append a short block for keys the file didn't already contain, so the
    // comment count can legitimately grow by a line or two - but it must never shrink.
    ok = Check(commentsAfter >= commentsBefore,
               "no comment lines were lost (the file's documentation survived a save)") && ok;

    // The commented-out preset lines must still be commented. If the writer treated one as the
    // active `effect=` line it would both rewrite the wrong line and silently enable a preset.
    ok = Check(AnyLineContains(after, ";effect=bilinear, bloom, acestonemap"),
               "commented-out preset effect= lines stayed commented") && ok;

    // A distinctive piece of prose from the middle of the file, to catch wholesale truncation
    // that a comment count alone might not.
    ok = Check(AnyLineContains(after, "Quake II units are roughly an inch"),
               "inline documentation prose survived verbatim") && ok;

    AnaxConfig reloaded = ParseConfigFile(kWorkIni);
    ok = Check(reloaded.ssaoRadius > 37.4f && reloaded.ssaoRadius < 37.6f,
               "float value round-tripped (ssaoRadius)") && ok;
    ok = Check(reloaded.dofFocusDistance > 124.9f && reloaded.dofFocusDistance < 125.1f,
               "float value round-tripped (dofFocusDistance)") && ok;
    ok = Check(reloaded.dofFocusRange > 47.9f && reloaded.dofFocusRange < 48.1f,
               "float value round-tripped (dofFocusRange)") && ok;
    ok = Check(reloaded.dofBlurStrength > 0.64f && reloaded.dofBlurStrength < 0.66f,
               "float value round-tripped (dofBlurStrength)") && ok;
    ok = Check(reloaded.fogStart > 309.9f && reloaded.fogStart < 310.1f,
               "float value round-tripped (fogStart)") && ok;
    ok = Check(reloaded.fogIntensity > 0.41f && reloaded.fogIntensity < 0.43f,
               "float value round-tripped (fogIntensity)") && ok;
    ok = Check(reloaded.fogColorG > 0.32f && reloaded.fogColorG < 0.34f,
               "float value round-tripped (fogColorG)") && ok;
    ok = Check(reloaded.shaftsIntensity > 0.26f && reloaded.shaftsIntensity < 0.28f,
               "float value round-tripped (shaftsIntensity)") && ok;
    ok = Check(reloaded.shaftsDecay > 0.87f && reloaded.shaftsDecay < 0.89f,
               "float value round-tripped (shaftsDecay)") && ok;
    ok = Check(reloaded.ssrIntensity > 0.30f && reloaded.ssrIntensity < 0.32f,
               "float value round-tripped (ssrIntensity)") && ok;
    ok = Check(reloaded.ssrMaxDistance > 639.9f && reloaded.ssrMaxDistance < 640.1f,
               "float value round-tripped (ssrMaxDistance)") && ok;
    ok = Check(reloaded.ssrUpThreshold > 0.54f && reloaded.ssrUpThreshold < 0.56f,
               "float value round-tripped (ssrUpThreshold)") && ok;
    ok = Check(reloaded.gamma > 2.19f && reloaded.gamma < 2.21f,
               "float value round-tripped (gamma)") && ok;
    ok = Check(reloaded.brightness > 1.39f && reloaded.brightness < 1.41f,
               "float value round-tripped (brightness)") && ok;
    ok = Check(reloaded.nrPasses == 3, "int value round-tripped (nrPasses)") && ok;
    ok = Check(reloaded.fxIndicator == expectedIndicator, "bool value round-tripped (fxIndicator)") && ok;
    ok = Check(reloaded.textureEffect == EffectKind::Invert, "enum value round-tripped (textureEffect)") && ok;
    ok = Check(reloaded.stageCount == 3 &&
               reloaded.stages[0] == EffectKind::Ssao &&
               reloaded.stages[1] == EffectKind::Bloom &&
               reloaded.stages[2] == EffectKind::Dither,
               "the ordered stage list round-tripped, order included") && ok;

    // Values the test never touched must come back exactly as the shipped ini had them - a
    // writer that reformatted or dropped untouched keys would show up here.
    AnaxConfig original = ParseConfigFile(kSourceIni);
    ok = Check(reloaded.bloomThreshold == original.bloomThreshold &&
               reloaded.lutStrength == original.lutStrength &&
               strcmp(reloaded.lutPath, original.lutPath) == 0,
               "untouched settings were preserved unchanged") && ok;

    remove(kWorkIni);

    printf("\n%s\n", ok ? "All checks passed." : "Some checks FAILED.");
    return ok ? 0 : 1;
}
