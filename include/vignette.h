#pragma once

// Vignette: darkens the frame toward the corners, leaving the center untouched. Purely a
// look/framing effect - it draws the eye to the middle of the screen and, on this SDR
// pipeline, exaggerates the perceived contrast between the (still bright) center and the
// falloff around it, which reads as more "HDR-like" depth than a uniformly lit frame. One
// stage in the shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes
// dstTexture. No capture/blit/state-save of its own.

// Runs the vignette pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture
// and dstTexture are both width x height RGBA16F 2D textures owned by the caller.
//
// intensity is GetAnaxConfig().vignetteIntensity, 0..1: how dark the corners get (0
// reproduces the input unchanged, 1 fades the corners to black). radius is
// GetAnaxConfig().vignetteRadius, 0..1: where the darkening starts, as a fraction of the
// center-to-corner distance - everything closer to the center than `radius` is left exactly
// alone, and the falloff ramps smoothly from there out to the corner. A large radius gives a
// tight vignette confined to the corners; a small one darkens most of the frame.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as
// the pipeline's new source when this returns true); returns false if GL 4.3 compute support
// is unavailable or shader init failed.
bool ApplyVignette(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                   float intensity, float radius);
