#pragma once

// Runs the BilinearUpscale post effect: captures the current back buffer into a texture,
// resamples it via a GLSL compute shader (a GL_LINEAR-filtered sampler2D read into an
// image2D write - "upscale" is really just a resample; at the current default scale of
// 1.0 it's a same-size round trip that still exercises the full pipeline, see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md), then blits the result
// back onto the back buffer. Requires a GL 4.3+ context (see gl_loader.h's
// GetGlComputeApi()) - on anything older, or if shader compilation/linking fails, this
// logs once and silently no-ops on every call, same as every other failure mode in this
// DLL.
void ApplyBilinearUpscale();
