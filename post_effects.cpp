#include <cstdio>

#include "post_effects.h"
#include "config.h"
#include "pixel_invert.h"
#include "bilinear_upscale.h"
#include "nis_effect.h"
#include "hdr_look.h"
#include "lut_grading.h"
#include "taa.h"

void ApplySelectedEffect() {
    const AnaxConfig& config = GetAnaxConfig();

    // Primary effect: captures the raw back buffer and runs the base upscale/misc transform.
    switch (config.effect) {
        case EffectKind::None:
            break;
        case EffectKind::Invert:
            InvertBackBufferColors();
            break;
        case EffectKind::Bilinear:
            ApplyBilinearUpscale();
            break;
        case EffectKind::NVScaler:
            ApplyNVScaler(config.sharpness);
            break;
    }

    // Everything below is an optional addon pass, each independent of the others, always
    // applied in this fixed order: ACES tone map -> LUT grading -> Sharpen -> TAA.
    if (config.enableAcesToneMap) {
        ApplyHdrLook(config.acesStrength);
    }
    if (config.enableLutGrading) {
        ApplyLutGrading(config.lutPath, config.lutStrength);
    }
    if (config.enableSharpen) {
        ApplyNVSharpen(config.sharpness);
    }
    if (config.enableTaa) {
        ApplyTaa(config.taaBlend);
    }
}
