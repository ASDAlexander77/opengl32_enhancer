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
// frame (0 = no temporal blending, 1 = heaviest - most stable but most ghosting risk).
//
// The neighborhood clamp alone can't fully stop ghosting: it only bounds history to the
// CURRENT frame's own local color range at each pixel, and a busy/detailed region has a wide
// range, so stale history from a completely different scene can numerically fit inside it and
// survive almost unclamped - then take many frames to fade at a high `blend`, visible as a
// persisting ghost after a fast scene change. So `blend` is only a ceiling: internally, each
// pixel's EFFECTIVE blend weight is reduced automatically the larger its RAW (pre-clamp)
// difference from history is - a direct, motion-vector-free "did this pixel's content just
// change" signal, independent of what the clamp box happens to allow. A pixel that's actually
// still showing the same thing keeps the full configured `blend`; a pixel that just changed
// converges toward the current frame in about one call, not several.
//
// shimmerSuppression is GetAnaxConfig().shimmerSuppression, 0..1 (0=off): a fixed `blend` still
// lets a pixel that's essentially unchanged frame to frame (dithered/noisy specular highlights,
// foliage, fine detail near the antialiasing/shading noise floor) flicker at whatever floor
// `blend` leaves in place. When the current frame and the clamped history already nearly
// agree - i.e. nothing is actually moving there - this raises the EFFECTIVE blend weight for
// just that pixel, toward history, on top of `blend`; on a real edge or genuine motion (current
// and clamped history disagree) it has no effect, so it can't add extra ghosting to moving
// content the way raising `blend` itself would.
//
// The very first call after startup or after a viewport-size change has no valid history yet
// and outputs the current frame unchanged, seeding history for the next call. Returns true if
// dstTexture was actually written (the caller must only treat dstTexture as the pipeline's new
// source when this returns true); returns false if GL 4.3 compute support is unavailable or
// shader init failed.
bool ApplyTaa(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float blend,
              float shimmerSuppression);
