#pragma once

// CAS (AMD FidelityFX Contrast Adaptive Sharpening), hand-ported from the reference
// implementation (https://github.com/GPUOpen-Effects/FidelityFX-CAS, MIT licensed) - see
// cas.cpp's header comment for the deliberate deviations from it.
//
// How this differs from the other two sharpeners already here, since three is a lot:
//   - `sharpen` (NVIDIA Image Scaling's NVSharpen) is the heaviest: a full USM-style filter
//     with its own coefficient tables and a large neighborhood.
//   - `fsr`'s RCAS is limiter-driven - it refuses to sharpen wherever doing so would clip,
//     which makes it very safe but means it declines to touch already-saturated edges at all.
//   - `cas` is the lightest: one 3x3 neighborhood, a per-channel "how much headroom is there"
//     amount, and a simple cross-shaped filter. It sharpens more uniformly across the image
//     than RCAS and costs less than NVSharpen, which is the usual reason to pick it.
//
// Only the non-scaling path is ported (the reference's `noScaling = true` branch). CAS can
// also upscale, but `fsr`/`nvscaler` already cover that and do it better, so this stage is
// sharpen-only and always runs 1:1.

// Runs the CAS pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller. `sharpness` is
// 0..1, mapped to the reference's -1/lerp(8,5,sharpness) peak term: 0 is the default (lowest
// ringing), 1 is maximum.
//
// Returns true if dstTexture was written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false (dstTexture untouched) if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplyCas(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
    float sharpness);
