#pragma once

// SMAA (Enhanced Subpixel Morphological Antialiasing), hand-ported from the SMAA reference
// implementation (https://github.com/iryoku/smaa, MIT licensed) - see smaa.cpp's header
// comment for the full list of deviations from the reference (compute shaders instead of a
// fragment-shader VS/PS pipeline, PRESET_HIGH's constants baked in directly rather than left
// configurable, no predication/color-edge-detection/temporal-reprojection - none of which
// this project's fixed swap-time pipeline uses). Three-pass pipeline (edge detection ->
// blending weight calculation -> neighborhood blending), same calling convention as every
// other stage in the shared post-effect pipeline (see post_effects.cpp): reads srcTexture,
// writes dstTexture, both width x height RGBA16F 2D textures owned by the caller. No capture/
// blit/state-save of its own. Owns its own persistent edges/blend-weight scratch textures
// (resized alongside width/height, same pattern as taa.cpp's history buffer) since a 3-pass
// effect needs more intermediate storage than the shared pipeline's single ping-pong pair.

// Runs one SMAA pass, called from post_effects.cpp's ApplySelectedEffect(). Returns true if
// dstTexture was actually written (the caller must only treat dstTexture as the pipeline's
// new source when this returns true); returns false if GL 4.3 compute support is unavailable
// or shader init failed, same fallback behavior as every other stage in this DLL.
bool ApplySmaa(unsigned int srcTexture, unsigned int dstTexture, int width, int height);
