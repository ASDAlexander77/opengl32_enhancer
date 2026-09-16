#pragma once

// Single-pass "HDR-look" post-process: an ACES filmic tone-mapping curve (Narkowicz 2015 fit)
// feeding a saturation/highlight grading pass. Output stays ordinary 8-bit SDR (this proxy
// has no way to signal real extended-range output to the display - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's non-goals for the equivalent
// reasoning about resolution). Same capture/compute/blit structure as
// bilinear_upscale.cpp's ApplyBilinearUpscale().

// Runs the grading pass on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. strength is GetAnaxConfig().hdrStrength, 0..1: 0 reproduces the input
// unchanged, 1 applies the full curve. Safe to call every frame; a no-op (falls back
// silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyHdrLook(float strength);
