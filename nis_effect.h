#pragma once

// NVScaler (edge-adaptive resample + sharpen) and NVSharpen (sharpen-only) post-process
// effects, ported from NVIDIA's Image Scaling SDK (NIS) compute shaders - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's "GPU pipeline" and
// "Shader source and NIS reuse" sections, and this plan's Global Constraints for the exact
// deviations from the upstream Vulkan GLSL. Same capture/compute/blit structure as
// bilinear_upscale.cpp's ApplyBilinearUpscale().

// Runs NVScaler (NIS_SCALER=1: directional edge-adaptive resample + adaptive sharpen) on
// the current back buffer, called from wglSwapBuffers via post_effects.cpp. sharpness is
// GetAnaxConfig().sharpness, 0..1 (see NVScalerUpdateConfig in nis_config.h for how it maps
// to the algorithm's internal strength/limit parameters). Safe to call every frame; a no-op
// (falls back silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyNVScaler(float sharpness);

// Runs NVSharpen (NIS_SCALER=0: adaptive directional sharpen only, no resample) on the
// current back buffer. Same calling convention and fallback behavior as ApplyNVScaler.
void ApplyNVSharpen(float sharpness);
