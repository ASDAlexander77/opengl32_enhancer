#pragma once

// Chromatic aberration: fakes the lateral color fringing a real camera lens produces, by
// sampling the red and blue channels at slightly different radial offsets from the green one.
// The offset grows with distance from the center (and is exactly zero at the center), so the
// middle of the frame stays sharp while the edges pick up subtle red/cyan fringes - the same
// "shot through glass" cue that makes a frame read as photographic rather than synthetic. One
// stage in the shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes
// dstTexture. No capture/blit/state-save of its own.

// Runs the chromatic aberration pass, called from post_effects.cpp's ApplySelectedEffect().
// srcTexture and dstTexture are both width x height RGBA16F 2D textures owned by the caller.
//
// strength is GetAnaxConfig().chromaticAberrationStrength, 0..1, scaling how far apart the
// channels are pulled at the frame edge: 0 reproduces the input unchanged (all three channels
// sample the same texel), and 1 is a deliberately overdone fringe. Small values (~0.2-0.4)
// are where this reads as a lens artifact rather than a broken image.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as
// the pipeline's new source when this returns true); returns false if GL 4.3 compute support
// is unavailable or shader init failed.
bool ApplyChromaticAberration(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                              float strength);
