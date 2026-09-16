#pragma once

// Bloom/glow: extracts pixels above a soft luma threshold, blurs them with a two-pass
// separable 9-tap Gaussian (horizontal then vertical), and additively blends the result back
// onto the captured frame - the classic "fake HDR glow on an SDR image" technique (see this
// plan's context: this proxy has no real HDR capture/output path, so bloom is one of the ways
// an SDR image is made to read as more HDR-like). Same capture/compute/blit skeleton as the
// other effects, extended to four sequential compute dispatches (extract, blur x2, composite)
// instead of one.

// Runs the bloom pass on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. threshold is GetAnaxConfig().bloomThreshold, 0..1: the luma level above
// which pixels start contributing to the glow (soft-kneed, not a hard cutoff). intensity is
// GetAnaxConfig().bloomIntensity, 0..2: 0 reproduces the input unchanged (no bloom
// contribution added), higher values add a stronger glow. Safe to call every frame; a no-op
// (falls back silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyBloom(float threshold, float intensity);
