#pragma once

// Inverts every pixel's RGB channels via a GLSL compute shader - one stage in the shared
// post-effect pipeline (see post_effects.cpp): reads srcTexture, writes the inverted result
// into dstTexture. No capture/blit/state-save of its own - the caller (post_effects.cpp) owns
// the pipeline's shared textures, does the one capture from the real back buffer and the one
// final blit back to it, and saves/restores the app's GL state around the whole chain.

// Runs the invert pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller. Returns true if
// dstTexture was actually written (the caller must only treat dstTexture as the pipeline's new
// source when this returns true); returns false (dstTexture untouched) if GL 4.3 compute
// support is unavailable or shader init failed.
bool ApplyInvert(unsigned int srcTexture, unsigned int dstTexture, int width, int height);
