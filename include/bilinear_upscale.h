#pragma once

// Runs the BilinearUpscale post effect: resamples the source as if it had been rendered at
// `scale` of the current resolution, then bilinearly reconstructs full size. At scale 1.0 it's
// a same-size round trip that still exercises the full pipeline (see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md). One stage in the shared
// post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture. No
// capture/blit/state-save of its own - the caller owns the pipeline's shared textures and the
// app's GL state around the whole chain.

// Runs the resample pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture
// and dstTexture are both width x height RGBA16F 2D textures owned by the caller. Returns true
// if dstTexture was actually written (the caller must only treat dstTexture as the pipeline's
// new source when this returns true). Requires a GL 4.3+ context (see gl_loader.h's
// GetGlComputeApi()) - on anything older, or if shader compilation/linking fails, this logs
// once and returns false on every call, same as every other failure mode in this DLL.
// `scale` (0.5..1) is the fraction of width/height the source is treated as having been
// rendered at; 1.0 is an exact texel-center copy.
bool ApplyBilinearUpscale(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
    float scale);
