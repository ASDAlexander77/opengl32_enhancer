#pragma once

// Ordered (Bayer 4x4) dithering: the final addon stage, run last (right before the real
// swap - see post_effects.cpp) so it dithers whatever the rest of the pipeline produced.
// Adds a small per-pixel offset (at most +-0.5 of one 8-bit step, scaled by `strength`)
// before the result is written back to the 8-bit back buffer, spatially randomizing which
// way each pixel rounds so flat color bands break up into a finer dither pattern instead of
// showing as hard steps - the standard technique for masking 8-bit banding, and particularly
// relevant here since every stage in this pipeline (tonemap, bloom, sharpen, LUT grading)
// round-trips through its own 8-bit RGBA8 texture. Same capture/compute/blit structure as
// bilinear_upscale.cpp's ApplyBilinearUpscale().

// Runs the dither pass on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. strength is GetAnaxConfig().ditherStrength, 0..1: 0 reproduces the input
// unchanged, 1 applies the full +-0.5-LSB dither offset. Safe to call every frame; a no-op
// (falls back silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyDither(float strength);
