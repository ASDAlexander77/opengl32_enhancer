#pragma once

// Motion-vector-free temporal antialiasing ("TAA-lite"): blends the current frame with a
// persistent history texture, clamping the history sample into the current frame's local
// 3x3 neighborhood min/max first to bound ghosting on moving content (this proxy has no
// access to the app's per-object motion vectors, unlike a real engine's TAA). Same
// capture/compute/blit structure as bilinear_upscale.cpp's ApplyBilinearUpscale(), plus a
// two-texture ping-pong history buffer that survives across frames.

// Runs one TAA-lite frame on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. blend is GetAnaxConfig().taaBlend, 0..1: how much of the (clamped)
// history to keep versus the current frame (0 = no temporal blending, 1 = heaviest -
// most stable but most ghosting risk). The very first call after startup or after a
// viewport-size change has no valid history yet and outputs the current frame unchanged,
// seeding history for the next call. Safe to call every frame; a no-op (falls back
// silently) if GL 4.3 compute support is unavailable or shader init failed.
void ApplyTaa(float blend);
