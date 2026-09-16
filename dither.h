#pragma once

// Ordered (Bayer 4x4) dithering: the final addon stage, run last (right before post_effects.cpp
// blits the pipeline's result back to the real back buffer) so it dithers whatever the rest of
// the pipeline produced. Adds a small per-pixel offset (at most +-0.5 of one 8-bit step, scaled
// by `strength`) before quantization, spatially randomizing which way each pixel rounds so flat
// color bands break up into a finer dither pattern instead of showing as hard steps - the
// standard technique for masking 8-bit banding at the final present. One stage in the shared
// post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture. No
// capture/blit/state-save of its own.

// Runs the dither pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller. strength is
// GetAnaxConfig().ditherStrength, 0..1: 0 reproduces the input unchanged, 1 applies the full
// +-0.5-LSB dither offset. Returns true if dstTexture was actually written (the caller must
// only treat dstTexture as the pipeline's new source when this returns true); returns false
// if GL 4.3 compute support is unavailable or shader init failed.
bool ApplyDither(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float strength);
