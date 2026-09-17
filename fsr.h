#pragma once

// FSR 1 (AMD FidelityFX Super Resolution 1) - EASU edge-adaptive spatial upsampling followed by
// RCAS robust contrast-adaptive sharpening, hand-ported from the reference implementation
// (https://github.com/GPUOpen-Effects/FidelityFX-FSR, MIT licensed) - see fsr.cpp's header
// comment for the deliberate deviations from it.
//
// Chosen over FSR 2/3 on purpose: those are temporal and need per-pixel motion vectors from the
// engine, which an opengl32 proxy has no way to obtain (the same reason `taa` here is a
// history blend rather than true motion-vector TAA). FSR 1 is purely spatial, so it is the
// highest-quality upscaler this architecture can actually host, and it runs on any vendor's GPU.

// Runs EASU then RCAS, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller.
//
// `scale` (0.5..1) is the fraction of the source resolution EASU treats as its input: at 0.75
// the source is resampled as if it had been rendered at 75% and EASU reconstructs full size,
// which is what FSR 1 is actually designed to do. At scale == 1.0 EASU runs 1:1 and RCAS
// supplies the sharpening on its own. `sharpness` (0..1) maps to RCAS's stop-based attenuation,
// 1 being sharpest.
//
// `denoise` enables the reference's FSR_RCAS_DENOISE path, which scales RCAS's sharpening lobe
// down where a pixel looks like isolated noise rather than a real edge. Off by default (as in
// the reference) since it costs a little sharpness on genuinely detailed input.
//
// `filmGrain` (0..1, 0 = off) re-applies grain AFTER reconstruction using the reference's
// FsrLfgaF weighting. FSR's guidance is that grain present before upscaling is resampled into
// mush, so a game wanting grain should add it at this point instead.
//
// Returns true if dstTexture was written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false (dstTexture untouched) if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplyFsr(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
    float scale, float sharpness, bool denoise, float filmGrain);
