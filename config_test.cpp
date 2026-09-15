// Exercises ParseConfigFile() against hand-written fixture .ini files. Run manually and
// check its exit code (this project has no unit-test framework/ctest wiring - see
// wrapper_test.cpp for the same "small executable, check exit code" pattern).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        printf("PASS: %s\n", what);
    }
}

void WriteFixture(const char* path, const char* contents) {
    FILE* f = fopen(path, "w");
    if (f == nullptr) {
        printf("FAIL: could not create fixture '%s'\n", path);
        exit(1);
    }
    fputs(contents, f);
    fclose(f);
}

}  // namespace

int main() {
    // Missing file -> defaults.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.effect == EffectKind::None, "missing file falls back to effect=none");
        Check(config.sharpness == 0.5f, "missing file falls back to default sharpness");
        Check(config.scale == 1.0f, "missing file falls back to default scale");
    }

    // Well-formed file.
    {
        WriteFixture("config_test_valid.ini",
            "effect=nvsharpen\n"
            "sharpness=0.75\n"
            "scale=0.8\n");
        AnaxConfig config = ParseConfigFile("config_test_valid.ini");
        Check(config.effect == EffectKind::NVSharpen, "valid file parses effect=nvsharpen");
        Check(config.sharpness == 0.75f, "valid file parses sharpness=0.75");
        Check(config.scale == 0.8f, "valid file parses scale=0.8");
    }

    // Comments, blank lines, surrounding whitespace, inline comments.
    {
        WriteFixture("config_test_comments.ini",
            "; this is a comment\n"
            "\n"
            "  effect = bilinear   \n"
            "scale=0.6 ; half res\n");
        AnaxConfig config = ParseConfigFile("config_test_comments.ini");
        Check(config.effect == EffectKind::Bilinear, "comments/whitespace: effect=bilinear parsed");
        Check(config.scale == 0.6f, "comments/whitespace: scale=0.6 parsed with inline comment stripped");
    }

    // Bad values fall back to defaults for just that field, not the whole config.
    {
        WriteFixture("config_test_bad.ini",
            "effect=not_a_real_effect\n"
            "sharpness=nope\n"
            "scale=5.0\n");
        AnaxConfig config = ParseConfigFile("config_test_bad.ini");
        Check(config.effect == EffectKind::None, "unrecognized effect falls back to none");
        Check(config.sharpness == 0.5f, "unparseable sharpness falls back to default");
        Check(config.scale == 1.0f, "out-of-range scale (5.0) clamps to max 1.0");
    }

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
