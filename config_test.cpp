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
        Check(config.scale == 1.0f, "missing file falls back to default scale");
        Check(config.enableAcesToneMap == false, "missing file falls back to default enableAcesToneMap (false)");
        Check(config.acesStrength == 0.5f, "missing file falls back to default acesStrength");
        Check(config.enableLutGrading == false, "missing file falls back to default enableLutGrading (false)");
        Check(strcmp(config.lutPath, "") == 0, "missing file falls back to default lutPath (empty)");
        Check(config.lutStrength == 1.0f, "missing file falls back to default lutStrength");
        Check(config.enableSharpen == false, "missing file falls back to default enableSharpen (false)");
        Check(config.sharpness == 0.5f, "missing file falls back to default sharpness");
        Check(config.enableTaa == false, "missing file falls back to default enableTaa (false)");
        Check(config.taaBlend == 0.5f, "missing file falls back to default taaBlend");
    }

    // Well-formed file.
    {
        WriteFixture("config_test_valid.ini",
            "effect=nvscaler\n"
            "scale=0.8\n"
            "enableSharpen=true\n"
            "sharpness=0.75\n");
        AnaxConfig config = ParseConfigFile("config_test_valid.ini");
        Check(config.effect == EffectKind::NVScaler, "valid file parses effect=nvscaler");
        Check(config.scale == 0.8f, "valid file parses scale=0.8");
        Check(config.enableSharpen == true, "valid file parses enableSharpen=true");
        Check(config.sharpness == 0.75f, "valid file parses sharpness=0.75");
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

    // All addon fields together: ACES tone map, LUT grading, sharpen, TAA.
    {
        WriteFixture("config_test_addons.ini",
            "effect=nvscaler\n"
            "enableAcesToneMap=true\n"
            "acesStrength=0.3\n"
            "enableLutGrading=true\n"
            "lutPath=luts/look.cube\n"
            "lutStrength=0.7\n"
            "enableSharpen=true\n"
            "sharpness=0.6\n"
            "enableTaa=true\n"
            "taaBlend=0.85\n");
        AnaxConfig config = ParseConfigFile("config_test_addons.ini");
        Check(config.effect == EffectKind::NVScaler, "parses effect=nvscaler alongside addons");
        Check(config.enableAcesToneMap == true, "parses enableAcesToneMap=true");
        Check(config.acesStrength == 0.3f, "parses acesStrength=0.3");
        Check(config.enableLutGrading == true, "parses enableLutGrading=true");
        Check(strcmp(config.lutPath, "luts/look.cube") == 0, "parses lutPath=luts/look.cube");
        Check(config.lutStrength == 0.7f, "parses lutStrength=0.7");
        Check(config.enableSharpen == true, "parses enableSharpen=true");
        Check(config.sharpness == 0.6f, "parses sharpness=0.6");
        Check(config.enableTaa == true, "parses enableTaa=true");
        Check(config.taaBlend == 0.85f, "parses taaBlend=0.85");
    }

    // effect=taa / hdrlook / nvsharpen (pre-addon config files) migrate to the equivalent
    // addon flag.
    {
        WriteFixture("config_test_taa_migrate.ini", "effect=taa\n");
        AnaxConfig config = ParseConfigFile("config_test_taa_migrate.ini");
        Check(config.effect == EffectKind::None, "effect=taa migrates effect to none");
        Check(config.enableTaa == true, "effect=taa migrates to enableTaa=true");
    }
    {
        WriteFixture("config_test_hdrlook_migrate.ini", "effect=hdrlook\n");
        AnaxConfig config = ParseConfigFile("config_test_hdrlook_migrate.ini");
        Check(config.effect == EffectKind::None, "effect=hdrlook migrates effect to none");
        Check(config.enableAcesToneMap == true, "effect=hdrlook migrates to enableAcesToneMap=true");
    }
    {
        WriteFixture("config_test_nvsharpen_migrate.ini", "effect=nvsharpen\n");
        AnaxConfig config = ParseConfigFile("config_test_nvsharpen_migrate.ini");
        Check(config.effect == EffectKind::None, "effect=nvsharpen migrates effect to none");
        Check(config.enableSharpen == true, "effect=nvsharpen migrates to enableSharpen=true");
    }

    // enableHdrLook / hdrStrength (this same day's earlier addon-flag names) migrate to
    // enableAcesToneMap / acesStrength.
    {
        WriteFixture("config_test_hdrlook_key_migrate.ini",
            "enableHdrLook=true\n"
            "hdrStrength=0.4\n");
        AnaxConfig config = ParseConfigFile("config_test_hdrlook_key_migrate.ini");
        Check(config.enableAcesToneMap == true, "enableHdrLook migrates to enableAcesToneMap=true");
        Check(config.acesStrength == 0.4f, "hdrStrength migrates to acesStrength=0.4");
    }

    // An ini line longer than ParseConfigFile's 256-byte line buffer is truncated safely by
    // fgets (never overflows AnaxConfig::lutPath, which is also 256 bytes) and doesn't stop
    // later keys on their own lines from parsing.
    {
        char longPath[300];
        memset(longPath, 'x', sizeof(longPath) - 1);
        longPath[sizeof(longPath) - 1] = '\0';
        char contents[400];
        snprintf(contents, sizeof(contents), "lutPath=%s\nlutStrength=0.5\n", longPath);
        WriteFixture("config_test_lutpath_long.ini", contents);
        AnaxConfig config = ParseConfigFile("config_test_lutpath_long.ini");
        Check(strlen(config.lutPath) < sizeof(config.lutPath), "overlong ini line's lutPath never overflows the 256-byte buffer");
        Check(config.lutStrength == 0.5f, "overlong lutPath line doesn't block later keys from parsing");
    }

    // Bad values fall back to defaults for just that field, not the whole config.
    {
        WriteFixture("config_test_bad.ini",
            "effect=not_a_real_effect\n"
            "sharpness=nope\n"
            "scale=5.0\n"
            "taaBlend=-1.0\n"
            "acesStrength=2.0\n"
            "lutStrength=-2.0\n");
        AnaxConfig config = ParseConfigFile("config_test_bad.ini");
        Check(config.effect == EffectKind::None, "unrecognized effect falls back to none");
        Check(config.sharpness == 0.5f, "unparseable sharpness falls back to default");
        Check(config.scale == 1.0f, "out-of-range scale (5.0) clamps to max 1.0");
        Check(config.taaBlend == 0.0f, "out-of-range taaBlend (-1.0) clamps to min 0.0");
        Check(config.acesStrength == 1.0f, "out-of-range acesStrength (2.0) clamps to max 1.0");
        Check(config.lutStrength == 0.0f, "out-of-range lutStrength (-2.0) clamps to min 0.0");
    }

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
