#pragma once

// Bloom/glow: extracts pixels above a soft luma threshold, blurs them with a two-pass
// separable 9-tap Gaussian (horizontal then vertical), and additively blends the result back
// onto srcTexture - the classic "fake HDR glow on an SDR image" technique. One stage in the
// shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture. No
// capture/blit/state-save of its own - the caller owns the pipeline's shared textures and the
// app's GL state around the whole chain. Owns its own bright-pass/blur intermediate textures
// (RGBA16F, resized alongside width/height) since those aren't part of the shared pipeline.

// Runs the bloom pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller. threshold is
// GetAnaxConfig().bloomThreshold, 0..1: the luma level above which pixels start contributing
// to the glow (soft-kneed, not a hard cutoff). intensity is GetAnaxConfig().bloomIntensity,
// 0..2: 0 reproduces the input unchanged (no bloom contribution added), higher values add a
// stronger glow. Returns true if dstTexture was actually written (the caller must only treat
// dstTexture as the pipeline's new source when this returns true); returns false if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplyBloom(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float threshold, float intensity);
