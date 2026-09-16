#pragma once

// Motion-vector-free temporal antialiasing ("TAA-lite"): blends the current frame with a
// persistent history texture, clamping the history sample into the current frame's local
// 3x3 neighborhood min/max first to bound ghosting on moving content (this proxy has no
// access to the app's per-object motion vectors, unlike a real engine's TAA). One stage in
// the shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes
// dstTexture. No capture/blit/state-save of its own - the caller owns the pipeline's shared
// textures and the app's GL state around the whole chain. Owns its own persistent two-texture
// ping-pong history buffer (RGBA16F, resized alongside width/height) since that survives
// across frames independent of the shared pipeline's own ping-pong (which a caller can't
// assume maps consistently frame to frame - see taa.cpp's header comment for why).

// Runs one TAA-lite frame, called from post_effects.cpp's ApplySelectedEffect(). srcTexture
// and dstTexture are both width x height RGBA16F 2D textures owned by the caller. blend is
// GetAnaxConfig().taaBlend, 0..1: how much of the (clamped) history to keep versus the current
// frame (0 = no temporal blending, 1 = heaviest - most stable but most ghosting risk). The
// very first call after startup or after a viewport-size change has no valid history yet and
// outputs the current frame unchanged, seeding history for the next call. Returns true if
// dstTexture was actually written (the caller must only treat dstTexture as the pipeline's new
// source when this returns true); returns false if GL 4.3 compute support is unavailable or
// shader init failed.
bool ApplyTaa(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float blend);
