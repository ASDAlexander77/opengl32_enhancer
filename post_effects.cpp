#include <cstdio>

#include "post_effects.h"
#include "config.h"
#include "pixel_invert.h"
#include "bilinear_upscale.h"
#include "nis_effect.h"
#include "taa.h"
#include "hdr_look.h"

void ApplySelectedEffect() {
    const AnaxConfig& config = GetAnaxConfig();

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
        case EffectKind::NVSharpen:
            ApplyNVSharpen(config.sharpness);
            break;
        case EffectKind::TAA:
            ApplyTaa(config.taaBlend);
            break;
        case EffectKind::HdrLook:
            ApplyHdrLook(config.hdrStrength);
            break;
    }
}
