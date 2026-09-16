#pragma once

// NVScaler (edge-adaptive resample + sharpen) and NVSharpen (sharpen-only) post-process
// effects, ported from NVIDIA's Image Scaling SDK (NIS) compute shaders - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's "GPU pipeline" and
// "Shader source and NIS reuse" sections. Each is one stage in the shared post-effect
// pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture. No capture/blit/
// state-save of their own - the caller owns the pipeline's shared textures and the app's GL
// state around the whole chain.

// Runs NVScaler (NIS_SCALER=1: directional edge-adaptive resample + adaptive sharpen),
// called from post_effects.cpp's ApplySelectedEffect(). srcTexture and dstTexture are both
// width x height RGBA16F 2D textures owned by the caller. sharpness is
// GetAnaxConfig().sharpness, 0..1 (see NVScalerUpdateConfig in nis_config.h for how it maps
// to the algorithm's internal strength/limit parameters). Returns true if dstTexture was
// actually written (the caller must only treat dstTexture as the pipeline's new source when
// this returns true); returns false if GL 4.3 compute support is unavailable, shader init
// failed, or the config was rejected (scale out of [0.5,1] range).
bool ApplyNVScaler(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float sharpness);

// Runs NVSharpen (NIS_SCALER=0: adaptive directional sharpen only, no resample). Same
// calling convention and fallback behavior as ApplyNVScaler.
bool ApplyNVSharpen(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float sharpness);
