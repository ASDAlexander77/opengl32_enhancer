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

// Compares config.stages against an expected sequence, so a test says what the pipeline is
// rather than just what's switched on - the whole point of the ordered `effect` list.
void CheckStages(const AnaxConfig& config, const EffectKind* expected, int expectedCount, const char* what) {
    bool match = config.stageCount == expectedCount;
    for (int i = 0; match && i < expectedCount; ++i) {
        match = config.stages[i] == expected[i];
    }
    if (!match) {
        printf("FAIL: %s\n  expected:", what);
        for (int i = 0; i < expectedCount; ++i) {
            printf(" %s", EffectNameFor(expected[i]));
        }
        printf("\n  actual:  ");
        for (int i = 0; i < config.stageCount; ++i) {
            printf(" %s", EffectNameFor(config.stages[i]));
        }
        printf("\n");
        ++g_failures;
    } else {
        printf("PASS: %s\n", what);
    }
}

}  // namespace

int main() {
    // Missing file -> defaults.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.stageCount == 0, "missing file falls back to an empty pipeline");
        Check(config.scale == 1.0f, "missing file falls back to default scale");
        Check(config.acesStrength == 0.5f, "missing file falls back to default acesStrength");
        Check(config.bloomThreshold == 0.8f, "missing file falls back to default bloomThreshold");
        Check(config.bloomIntensity == 0.5f, "missing file falls back to default bloomIntensity");
        Check(strcmp(config.lutPath, "") == 0, "missing file falls back to default lutPath (empty)");
        Check(config.lutStrength == 1.0f, "missing file falls back to default lutStrength");
        Check(config.sharpness == 0.5f, "missing file falls back to default sharpness");
        Check(config.taaBlend == 0.5f, "missing file falls back to default taaBlend");
        Check(config.vignetteIntensity == 0.3f, "missing file falls back to default vignetteIntensity");
        Check(config.vignetteRadius == 0.7f, "missing file falls back to default vignetteRadius");
        Check(config.chromaticAberrationStrength == 0.3f, "missing file falls back to default chromaticAberrationStrength");
        Check(config.ditherStrength == 1.0f, "missing file falls back to default ditherStrength");
    }

    // A single-stage effect list.
    {
        WriteFixture("config_test_valid.ini",
            "effect=nvscaler\n"
            "scale=0.8\n"
            "sharpness=0.75\n");
        AnaxConfig config = ParseConfigFile("config_test_valid.ini");
        const EffectKind expected[] = {EffectKind::NVScaler};
        CheckStages(config, expected, 1, "single-stage effect=nvscaler");
        Check(config.scale == 0.8f, "valid file parses scale=0.8");
        Check(config.sharpness == 0.75f, "valid file parses sharpness=0.75");
    }

    // The whole pipeline in one list, in a non-default order, with mixed case and stray
    // spaces around the commas.
    {
        WriteFixture("config_test_list.ini",
            "effect = bilinear ,  Bloom,AcesToneMap , lutgrading,  VIGNETTE , chromaticaberration,taa, sharpen ,dither \n"
            "acesStrength=0.3\n"
            "bloomThreshold=0.7\n"
            "bloomIntensity=1.5\n"
            "lutPath=luts/look.cube\n"
            "lutStrength=0.7\n"
            "sharpness=0.6\n"
            "taaBlend=0.85\n"
            "vignetteIntensity=0.25\n"
            "vignetteRadius=0.6\n"
            "chromaticAberrationStrength=0.4\n"
            "ditherStrength=0.5\n");
        AnaxConfig config = ParseConfigFile("config_test_list.ini");
        const EffectKind expected[] = {
            EffectKind::Bilinear, EffectKind::Bloom, EffectKind::AcesToneMap,
            EffectKind::LutGrading, EffectKind::Vignette, EffectKind::ChromaticAberration,
            EffectKind::Taa, EffectKind::Sharpen, EffectKind::Dither,
        };
        CheckStages(config, expected, 9, "full pipeline list parses in order, case- and space-insensitively");
        Check(config.acesStrength == 0.3f, "parses acesStrength=0.3");
        Check(config.bloomThreshold == 0.7f, "parses bloomThreshold=0.7");
        Check(config.bloomIntensity == 1.5f, "parses bloomIntensity=1.5");
        Check(strcmp(config.lutPath, "luts/look.cube") == 0, "parses lutPath=luts/look.cube");
        Check(config.lutStrength == 0.7f, "parses lutStrength=0.7");
        Check(config.sharpness == 0.6f, "parses sharpness=0.6");
        Check(config.taaBlend == 0.85f, "parses taaBlend=0.85");
        Check(config.vignetteIntensity == 0.25f, "parses vignetteIntensity=0.25");
        Check(config.vignetteRadius == 0.6f, "parses vignetteRadius=0.6");
        Check(config.chromaticAberrationStrength == 0.4f, "parses chromaticAberrationStrength=0.4");
        Check(config.ditherStrength == 0.5f, "parses ditherStrength=0.5");
    }

    // Order is preserved, not normalized: the same set of stages in a different order must
    // come back in that different order.
    {
        WriteFixture("config_test_order_a.ini", "effect=bloom, acestonemap, dither\n");
        AnaxConfig configA = ParseConfigFile("config_test_order_a.ini");
        const EffectKind expectedA[] = {EffectKind::Bloom, EffectKind::AcesToneMap, EffectKind::Dither};
        CheckStages(configA, expectedA, 3, "effect=bloom, acestonemap, dither keeps that order");

        WriteFixture("config_test_order_b.ini", "effect=acestonemap, dither, bloom\n");
        AnaxConfig configB = ParseConfigFile("config_test_order_b.ini");
        const EffectKind expectedB[] = {EffectKind::AcesToneMap, EffectKind::Dither, EffectKind::Bloom};
        CheckStages(configB, expectedB, 3, "the same three stages reordered come back reordered");
    }

    // Comments, blank lines, surrounding whitespace, inline comments.
    {
        WriteFixture("config_test_comments.ini",
            "; this is a comment\n"
            "\n"
            "  effect = bilinear, dither  ; upscale then dither\n"
            "scale=0.6 ; half res\n");
        AnaxConfig config = ParseConfigFile("config_test_comments.ini");
        const EffectKind expected[] = {EffectKind::Bilinear, EffectKind::Dither};
        CheckStages(config, expected, 2, "comments/whitespace: effect list parsed with inline comment stripped");
        Check(config.scale == 0.6f, "comments/whitespace: scale=0.6 parsed with inline comment stripped");
    }

    // effect=none, and an empty list, both mean "no post-processing".
    {
        WriteFixture("config_test_none.ini", "effect=none\n");
        AnaxConfig config = ParseConfigFile("config_test_none.ini");
        Check(config.stageCount == 0, "effect=none produces an empty pipeline");
    }
    {
        WriteFixture("config_test_empty_list.ini", "effect=\n");
        AnaxConfig config = ParseConfigFile("config_test_empty_list.ini");
        Check(config.stageCount == 0, "an empty effect list produces an empty pipeline");
    }

    // `effects` (plural) is accepted as a spelling of the same key.
    {
        WriteFixture("config_test_plural.ini", "effects=bloom, dither\n");
        AnaxConfig config = ParseConfigFile("config_test_plural.ini");
        const EffectKind expected[] = {EffectKind::Bloom, EffectKind::Dither};
        CheckStages(config, expected, 2, "'effects' (plural) parses the same as 'effect'");
    }

    // Listing a stage twice is allowed - the pipeline just runs it twice, in both positions.
    {
        WriteFixture("config_test_dupe.ini", "effect=bloom, dither, bloom\n");
        AnaxConfig config = ParseConfigFile("config_test_dupe.ini");
        const EffectKind expected[] = {EffectKind::Bloom, EffectKind::Dither, EffectKind::Bloom};
        CheckStages(config, expected, 3, "a stage listed twice appears twice, in both positions");
    }

    // An unrecognized name is skipped without taking the rest of the list with it.
    {
        WriteFixture("config_test_bad_name.ini", "effect=bloom, not_a_real_effect, dither\n");
        AnaxConfig config = ParseConfigFile("config_test_bad_name.ini");
        const EffectKind expected[] = {EffectKind::Bloom, EffectKind::Dither};
        CheckStages(config, expected, 2, "an unrecognized stage name is skipped, later stages still parse");
    }

    // hdrlook / nvsharpen (the names these stages had when they were primary effects) still
    // resolve, and land in the list at the position they were written in.
    {
        WriteFixture("config_test_aliases.ini", "effect=hdrlook, nvsharpen\n");
        AnaxConfig config = ParseConfigFile("config_test_aliases.ini");
        const EffectKind expected[] = {EffectKind::AcesToneMap, EffectKind::Sharpen};
        CheckStages(config, expected, 2, "legacy names hdrlook/nvsharpen resolve to acestonemap/sharpen");
    }

    // Legacy enableXxx flags with no effect list: the stages come back in the fixed order the
    // pipeline used to hard-code, regardless of what order the keys appear in the file (such
    // a config never got to express an order, so the order it used to get is the right one).
    {
        WriteFixture("config_test_legacy_enable.ini",
            "enableDither=true\n"
            "enableSharpen=true\n"
            "enableBloom=true\n"
            "enableTaa=true\n");
        AnaxConfig config = ParseConfigFile("config_test_legacy_enable.ini");
        const EffectKind expected[] = {
            EffectKind::Bloom, EffectKind::Taa, EffectKind::Sharpen, EffectKind::Dither,
        };
        CheckStages(config, expected, 4, "legacy enableXxx flags rebuild the old fixed pipeline order");
    }

    // A legacy enable flag alongside an effect list: appends what the list didn't mention,
    // and removes what it did. Order of the lines in the file must not matter, which is why
    // the flags are applied only after the whole file has been read.
    {
        WriteFixture("config_test_legacy_mixed.ini",
            "enableDither=true\n"
            "effect=bilinear, bloom\n"
            "enableBloom=false\n");
        AnaxConfig config = ParseConfigFile("config_test_legacy_mixed.ini");
        const EffectKind expected[] = {EffectKind::Bilinear, EffectKind::Dither};
        CheckStages(config, expected, 2,
                    "enableXxx=true appends and enableXxx=false removes, whichever side of 'effect=' they sit on");
    }

    // enableHdrLook / hdrStrength (this same day's earlier addon-flag names) migrate to the
    // acestonemap stage / acesStrength.
    {
        WriteFixture("config_test_hdrlook_key_migrate.ini",
            "enableHdrLook=true\n"
            "hdrStrength=0.4\n");
        AnaxConfig config = ParseConfigFile("config_test_hdrlook_key_migrate.ini");
        Check(HasEffectStage(config, EffectKind::AcesToneMap), "enableHdrLook enables the acestonemap stage");
        Check(config.acesStrength == 0.4f, "hdrStrength migrates to acesStrength=0.4");
    }

    // More stages listed than kMaxEffectStages: the overflow is dropped, nothing is corrupted.
    {
        char contents[1024];
        size_t used = (size_t)snprintf(contents, sizeof(contents), "effect=bloom");
        for (int i = 0; i < kMaxEffectStages + 5; ++i) {
            used += (size_t)snprintf(contents + used, sizeof(contents) - used, ", bloom");
        }
        snprintf(contents + used, sizeof(contents) - used, "\n");
        WriteFixture("config_test_overflow.ini", contents);
        AnaxConfig config = ParseConfigFile("config_test_overflow.ini");
        Check(config.stageCount == kMaxEffectStages, "an over-long effect list is capped at kMaxEffectStages");
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
            "bloomIntensity=5.0\n"
            "lutStrength=-2.0\n"
            "vignetteIntensity=3.0\n"
            "chromaticAberrationStrength=-0.5\n"
            "ditherStrength=-1.0\n");
        AnaxConfig config = ParseConfigFile("config_test_bad.ini");
        Check(config.stageCount == 0, "an effect list of only unrecognized names leaves an empty pipeline");
        Check(config.sharpness == 0.5f, "unparseable sharpness falls back to default");
        Check(config.scale == 1.0f, "out-of-range scale (5.0) clamps to max 1.0");
        Check(config.taaBlend == 0.0f, "out-of-range taaBlend (-1.0) clamps to min 0.0");
        Check(config.acesStrength == 1.0f, "out-of-range acesStrength (2.0) clamps to max 1.0");
        Check(config.bloomIntensity == 2.0f, "out-of-range bloomIntensity (5.0) clamps to max 2.0");
        Check(config.lutStrength == 0.0f, "out-of-range lutStrength (-2.0) clamps to min 0.0");
        Check(config.vignetteIntensity == 1.0f, "out-of-range vignetteIntensity (3.0) clamps to max 1.0");
        Check(config.chromaticAberrationStrength == 0.0f, "out-of-range chromaticAberrationStrength (-0.5) clamps to min 0.0");
        Check(config.ditherStrength == 0.0f, "out-of-range ditherStrength (-1.0) clamps to min 0.0");
    }

    // The gamma stage: both parameters default to an exact passthrough, parse, and clamp to
    // the ranges gamma.h documents. The defaults matter more here than for most stages -
    // `gamma`/`brightness` are the settings a user is most likely to reach for first, and a
    // default that was anything other than 1.0 would silently alter every config that never
    // mentions them.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.gamma == 1.0f, "missing file falls back to default gamma=1.0 (no-op)");
        Check(config.brightness == 1.0f, "missing file falls back to default brightness=1.0 (no-op)");
    }
    {
        WriteFixture("config_test_gamma.ini",
            "effect=acestonemap, Gamma, dither\n"
            "gamma=2.2\n"
            "brightness=1.25\n");
        AnaxConfig config = ParseConfigFile("config_test_gamma.ini");
        const EffectKind expected[] = {EffectKind::AcesToneMap, EffectKind::Gamma, EffectKind::Dither};
        CheckStages(config, expected, 3, "effect=gamma parses as a stage, in list order");
        Check(config.gamma == 2.2f, "parses gamma=2.2");
        Check(config.brightness == 1.25f, "parses brightness=1.25");
    }
    {
        WriteFixture("config_test_gamma_clamp.ini",
            "gamma=10.0\n"
            "brightness=-1.0\n");
        AnaxConfig config = ParseConfigFile("config_test_gamma_clamp.ini");
        Check(config.gamma == 3.0f, "out-of-range gamma (10.0) clamps to max 3.0");
        Check(config.brightness == 0.0f, "out-of-range brightness (-1.0) clamps to min 0.0");
    }
    {
        // gamma=0 would be a divide-by-zero in the shader's 1/gamma, so the low clamp is the
        // thing standing between a typo and a frame of infinities.
        WriteFixture("config_test_gamma_zero.ini", "gamma=0\n");
        AnaxConfig config = ParseConfigFile("config_test_gamma_zero.ini");
        Check(config.gamma == 0.5f, "gamma=0 clamps up to min 0.5 rather than dividing by zero");
    }

    // The FSR extras parse, clamp and default like every other per-stage parameter.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.fsrDenoise == false, "missing file falls back to default fsrDenoise=false");
        Check(config.fsrFilmGrain == 0.0f, "missing file falls back to default fsrFilmGrain=0");
    }
    {
        WriteFixture("config_test_fsr_extras.ini",
            "effect=fsr, cas\n"
            "fsrDenoise=1\n"
            "fsrFilmGrain=0.4\n");
        AnaxConfig config = ParseConfigFile("config_test_fsr_extras.ini");
        const EffectKind expected[] = {EffectKind::Fsr, EffectKind::Cas};
        CheckStages(config, expected, 2, "effect=fsr, cas parses both new stages in order");
        Check(config.fsrDenoise, "parses fsrDenoise=1");
        Check(config.fsrFilmGrain == 0.4f, "parses fsrFilmGrain=0.4");
    }
    {
        WriteFixture("config_test_fsr_clamp.ini", "fsrFilmGrain=5.0\n");
        AnaxConfig config = ParseConfigFile("config_test_fsr_clamp.ini");
        Check(config.fsrFilmGrain == 1.0f, "out-of-range fsrFilmGrain (5.0) clamps to max 1.0");
    }

    // nr and localcontrast parse like every other stage/parameter, including nrPasses' int
    // clamp (ParseClampedInt, not the float path everything else here uses).
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.nrIntensity == 0.5f, "missing file falls back to default nrIntensity=0.5");
        Check(config.nrPasses == 1, "missing file falls back to default nrPasses=1");
        Check(config.nrColorStrength == 1.0f, "missing file falls back to default nrColorStrength=1.0");
        Check(config.nrTonePreservation == 0.0f, "missing file falls back to default nrTonePreservation=0");
        Check(config.nrGrainPreservation == 0.0f, "missing file falls back to default nrGrainPreservation=0");
        Check(config.localStructureStrength == 0.3f, "missing file falls back to default localStructureStrength=0.3");
        Check(config.localToneStrength == 0.3f, "missing file falls back to default localToneStrength=0.3");
        Check(config.shimmerSuppression == 0.0f, "missing file falls back to default shimmerSuppression=0");
        Check(config.fxIndicator == true, "missing file falls back to default fxIndicator=true");
        Check(config.anisotropy == 0.0f, "missing file falls back to default anisotropy=0 (off)");
    }
    {
        WriteFixture("config_test_nr_localcontrast.ini",
            "effect=nr, localcontrast\n"
            "nrIntensity=0.8\n"
            "nrPasses=3\n"
            "nrColorStrength=0.6\n"
            "nrTonePreservation=0.25\n"
            "nrGrainPreservation=0.4\n"
            "localStructureStrength=1.2\n"
            "localToneStrength=0.9\n"
            "shimmerSuppression=0.7\n"
            "fxIndicator=0\n"
            "anisotropy=8\n");
        AnaxConfig config = ParseConfigFile("config_test_nr_localcontrast.ini");
        const EffectKind expected[] = {EffectKind::Nr, EffectKind::LocalContrast};
        CheckStages(config, expected, 2, "effect=nr, localcontrast parses both new stages in order");
        Check(config.nrIntensity == 0.8f, "parses nrIntensity=0.8");
        Check(config.nrPasses == 3, "parses nrPasses=3");
        Check(config.nrColorStrength == 0.6f, "parses nrColorStrength=0.6");
        Check(config.nrTonePreservation == 0.25f, "parses nrTonePreservation=0.25");
        Check(config.nrGrainPreservation == 0.4f, "parses nrGrainPreservation=0.4");
        Check(config.localStructureStrength == 1.2f, "parses localStructureStrength=1.2");
        Check(config.localToneStrength == 0.9f, "parses localToneStrength=0.9");
        Check(config.shimmerSuppression == 0.7f, "parses shimmerSuppression=0.7");
        Check(config.fxIndicator == false, "parses fxIndicator=0");
        Check(config.anisotropy == 8.0f, "parses anisotropy=8");
    }
    {
        WriteFixture("config_test_anisotropy_clamp.ini", "anisotropy=99\n");
        AnaxConfig config = ParseConfigFile("config_test_anisotropy_clamp.ini");
        Check(config.anisotropy == 16.0f, "out-of-range anisotropy (99) clamps to max 16");
    }
    {
        WriteFixture("config_test_nr_clamp.ini", "nrPasses=99\nnrIntensity=-5.0\nlocalToneStrength=9.0\n");
        AnaxConfig config = ParseConfigFile("config_test_nr_clamp.ini");
        Check(config.nrPasses == 4, "out-of-range nrPasses (99) clamps to max 4");
        Check(config.nrIntensity == 0.0f, "out-of-range nrIntensity (-5.0) clamps to min 0.0");
        Check(config.localToneStrength == 2.0f, "out-of-range localToneStrength (9.0) clamps to max 2.0");
    }
    {
        WriteFixture("config_test_nr_passes_not_int.ini", "nrPasses=abc\n");
        AnaxConfig config = ParseConfigFile("config_test_nr_passes_not_int.ini");
        Check(config.nrPasses == 1, "unparseable nrPasses falls back to default 1");
    }

    // textureEffect accepts none/sharpen/invert, defaults to none, rejects anything else, and
    // still honors the legacy textureSharpen=1/0 spelling.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.textureEffect == EffectKind::None, "missing file falls back to default textureEffect=none");
    }
    {
        WriteFixture("config_test_texture_none.ini", "textureEffect=none\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_none.ini");
        Check(config.textureEffect == EffectKind::None, "textureEffect=none parses to None");
    }
    {
        WriteFixture("config_test_texture_sharpen.ini", "textureEffect=sharpen\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_sharpen.ini");
        Check(config.textureEffect == EffectKind::Sharpen, "textureEffect=sharpen parses to Sharpen");
    }
    {
        WriteFixture("config_test_texture_invert.ini", "textureEffect=Invert\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_invert.ini");
        Check(config.textureEffect == EffectKind::Invert, "textureEffect=Invert parses case-insensitively to Invert");
    }
    {
        WriteFixture("config_test_texture_cas.ini", "textureEffect=cas\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_cas.ini");
        Check(config.textureEffect == EffectKind::Cas, "textureEffect=cas parses to Cas");
    }
    {
        // fsr is a real stage name, but not one this hook accepts - it would reallocate its
        // scratch texture per upload size. See texture_effect.h.
        WriteFixture("config_test_texture_fsr.ini", "textureEffect=fsr\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_fsr.ini");
        Check(config.textureEffect == EffectKind::None,
              "textureEffect=fsr is rejected, falls back to none");
    }
    {
        // nr and localcontrast are both multi-pass with their own owned intermediate textures
        // sized to the upload - same width/height churn problem as fsr, worse. See
        // texture_effect.h.
        WriteFixture("config_test_texture_nr.ini", "textureEffect=nr\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_nr.ini");
        Check(config.textureEffect == EffectKind::None,
              "textureEffect=nr is rejected, falls back to none");
    }
    {
        WriteFixture("config_test_texture_localcontrast.ini", "textureEffect=localcontrast\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_localcontrast.ini");
        Check(config.textureEffect == EffectKind::None,
              "textureEffect=localcontrast is rejected, falls back to none");
    }
    {
        WriteFixture("config_test_texture_bad.ini", "textureEffect=bloom\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_bad.ini");
        Check(config.textureEffect == EffectKind::None,
              "textureEffect naming a pipeline-only stage (bloom) is rejected, falls back to none");
    }
    {
        WriteFixture("config_test_texture_legacy_on.ini", "textureSharpen=1\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_legacy_on.ini");
        Check(config.textureEffect == EffectKind::Sharpen, "legacy textureSharpen=1 migrates to textureEffect=sharpen");
    }
    {
        WriteFixture("config_test_texture_legacy_off.ini", "textureSharpen=0\n");
        AnaxConfig config = ParseConfigFile("config_test_texture_legacy_off.ini");
        Check(config.textureEffect == EffectKind::None, "legacy textureSharpen=0 migrates to textureEffect=none");
    }

    // windowWidth/windowHeight: default 0 (leave the window alone), parse, and clamp.
    {
        AnaxConfig config = ParseConfigFile("config_test_does_not_exist.ini");
        Check(config.windowWidth == 0, "missing file falls back to default windowWidth=0");
        Check(config.windowHeight == 0, "missing file falls back to default windowHeight=0");
    }
    {
        WriteFixture("config_test_window_size.ini", "windowWidth=1280\nwindowHeight=720\n");
        AnaxConfig config = ParseConfigFile("config_test_window_size.ini");
        Check(config.windowWidth == 1280, "parses windowWidth=1280");
        Check(config.windowHeight == 720, "parses windowHeight=720");
    }
    {
        WriteFixture("config_test_window_size_clamp.ini", "windowWidth=-5\nwindowHeight=99999\n");
        AnaxConfig config = ParseConfigFile("config_test_window_size_clamp.ini");
        Check(config.windowWidth == 0, "out-of-range windowWidth (-5) clamps to min 0");
        Check(config.windowHeight == 16384, "out-of-range windowHeight (99999) clamps to max 16384");
    }

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
