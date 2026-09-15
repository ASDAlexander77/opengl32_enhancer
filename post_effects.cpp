#include <cstdio>

#include "post_effects.h"
#include "config.h"
#include "pixel_invert.h"
#include "bilinear_upscale.h"

namespace {

const char* EffectName(EffectKind effect) {
    switch (effect) {
        case EffectKind::None: return "none";
        case EffectKind::Invert: return "invert";
        case EffectKind::Bilinear: return "bilinear";
        case EffectKind::NVScaler: return "nvscaler";
        case EffectKind::NVSharpen: return "nvsharpen";
    }
    return "<unknown>";
}

}  // namespace

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
        case EffectKind::NVSharpen: {
            // NIS shader port lands in a follow-up plan. Until then, these are recognized
            // and logged, and fall back to doing nothing, same as effect=none.
            static bool warned = false;
            if (!warned) {
                printf("[opengl32_enh_cpp] post_effects: effect='%s' is not implemented yet, "
                       "falling back to none\n", EffectName(config.effect));
                warned = true;
            }
            break;
        }
    }
}
