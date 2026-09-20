#pragma once

// The two colour-space conversion passes that bracket the post-effect chain when
// srgbCorrect=1 - one stage each in the shared pipeline (see post_effects.cpp): reads
// srcTexture, writes dstTexture. No capture/blit/state-save of its own - the caller owns the
// pipeline's shared textures and the app's GL state around the whole chain.
//
// These exist because sRGB is roughly a 2.2-power ENCODING, and averaging two encoded values
// does not give the encoding of their average. Every stage that averages, blurs, thresholds or
// tone-maps is therefore weighting the wrong quantity until the frame is decoded to linear
// light. See docs/superpowers/specs/2026-09-20-srgb-correctness-design.md.
//
// Both use the EXACT piecewise sRGB transfer function, never a pow(2.2) approximation. The
// whole feature is a correctness claim, and the linear segment near black - below 0.04045 in,
// 0.0031308 out - is exactly where a pow approximation is most wrong. Approximating here would
// be introducing a new error while removing an old one.

// sRGB-encoded -> linear light. srcTexture and dstTexture are both width x height RGBA16F 2D
// textures owned by the caller. Alpha passes through untouched - it is a coverage value, not a
// light measurement, and encoding never applied to it. Negative inputs are clamped to 0 before
// the curve, since pow() of a negative is NaN and one NaN propagates through every later
// stage. Returns true if dstTexture was actually written (the caller must only treat
// dstTexture as the pipeline's new source when this returns true); returns false if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplySrgbDecode(unsigned int srcTexture, unsigned int dstTexture, int width, int height);

// Linear light -> sRGB-encoded. Same contract as ApplySrgbDecode in every respect. Values
// above 1.0 are left to the curve rather than clamped: the pipeline is RGBA16F and a stage
// upstream of tone mapping can legitimately produce them, the same reasoning gamma.h gives.
bool ApplySrgbEncode(unsigned int srcTexture, unsigned int dstTexture, int width, int height);
