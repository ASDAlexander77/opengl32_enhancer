#pragma once

// Local contrast enhancement (a "clarity"/"texture" pair, the same idea as the sliders those
// names describe in photo tools): boosts luma detail at two different spatial scales by
// unsharp-masking against two different blur radii, then adds both boosts back onto the
// original color additively (not by scaling RGB by a luma ratio, which would blow up near
// black). One stage in the shared post-effect pipeline (see post_effects.cpp): reads
// srcTexture, writes dstTexture. No capture/blit/state-save of its own - the caller owns the
// pipeline's shared textures and the app's GL state around the whole chain. Owns its own blur
// intermediate textures (RGBA16F, resized alongside width/height) since those aren't part of
// the shared pipeline.

// Runs the local contrast pass, called from post_effects.cpp's ApplySelectedEffect().
// srcTexture and dstTexture are both width x height RGBA16F 2D textures owned by the caller.
//
//   structureStrength  GetAnaxConfig().localStructureStrength, 0..2: boosts the fine-detail
//                       layer (original luma minus a SMALL-radius blur) - texture/micro-detail,
//                       the finest scale this stage touches.
//   toneStrength       GetAnaxConfig().localToneStrength, 0..2: boosts the broad local-contrast
//                       layer (original luma minus a LARGE-radius blur) - separates midtones
//                       from their surroundings over a wider area, the classic "clarity" look.
//
// Both layers are computed from the SAME original pixel (not chained), so the two controls are
// independent - raising one doesn't change what the other measures. 0 for both reproduces the
// input unchanged. Returns true if dstTexture was actually written (the caller must only treat
// dstTexture as the pipeline's new source when this returns true); returns false if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplyLocalContrast(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                         float structureStrength, float toneStrength);
