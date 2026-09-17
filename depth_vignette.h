#pragma once

// EXPERIMENTAL/SPIKE: darkens a pixel based on the scene's own depth buffer rather than its
// screen-space distance from the center, the way vignette.h's ordinary vignette does. Answers
// "does depth-based darkening (background fades, foreground stays lit - atmospheric depth
// cueing) read as a visual improvement over the corner-only vignette", not yet a committed
// feature - see vignette.h for the screen-space version this is compared against. One stage in
// the shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture,
// samples depthTexture (the default framebuffer's depth attachment, blitted by the caller just
// before this runs - see post_effects.cpp's capture step). No capture/blit/state-save of its
// own beyond that depth sample.

// Runs the depth vignette pass, called from post_effects.cpp's ApplySelectedEffect().
// srcTexture and dstTexture are both width x height RGBA16F 2D textures owned by the caller;
// depthTexture is a width x height depth-format 2D texture holding this frame's raw (hardware,
// non-linear) depth values, also owned by the caller.
//
// intensity is GetAnaxConfig().depthVignetteIntensity, 0..1: how dark the most distant pixels
// get (0 reproduces the input unchanged, 1 fades them to black). threshold is
// GetAnaxConfig().depthVignetteThreshold, 0..1: the raw depth value below which a pixel is left
// exactly alone - everything nearer than that is untouched, and the falloff ramps smoothly from
// there out to depth=1.0 (the far plane). Raw hardware depth is heavily skewed toward 1.0 by the
// perspective projection, so most on-screen geometry sits well below a moderate threshold and
// only genuinely distant background pushes into the falloff.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable, shader init failed, or the caller could not supply a valid depthTexture this
// frame (e.g. the default framebuffer has no depth attachment).
bool ApplyDepthVignette(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
                         int width, int height, float intensity, float threshold);
