#pragma once

// Captures a copy of the frame BEFORE the game draws its HUD, so a stage can tell an overlay
// pixel from a world pixel. Depth cannot do this: Quake II's R_SetGL2D disables depth testing
// to draw the HUD, and depth writes require depth testing, so HUD pixels never write depth -
// they keep whatever the world wrote behind them.
//
// WHEN, not what. `glFrustum` arms a world pass and the first `glOrtho` of the frame fires the
// latch, which is the same discrimination modelview_capture.h and projection_capture.h already
// make at the same two hook points. At the first `glOrtho` the world is finished and the 2D
// pass has not started, so the back buffer at that instant is exactly the world.
//
// Unlike the post-effect chain, this runs INSIDE the game's own GL state, so it saves and
// restores the active texture unit, the 2D texture binding, the read buffer and the read
// framebuffer binding around its one copy. See texture_effect.cpp, which does the same at
// glTexImage2D.
//
// Everything here is gated on a consumer actually being listed in the config - `motionblur` or
// `taa`, whose reprojected path masks HUD pixels out of the resolve the same way; see
// StageNeedsWorldCapture in config.h, and LatchWorldFrame() in the .cpp. A stock ini, or
// effect=none, never reaches the GL calls below at all. The two share the one capture, so
// listing both costs no more than either alone.
//
// glGetError() around the copy DRAINS whatever the game left pending first, rather than just
// reading it: reading clears the flag, so blindly checking it after the copy would both
// misattribute a pre-existing error to this capture AND make the game's own error-checking
// (e.g. a Quake II-family GL_CheckErrors() call) silently lose an error it would otherwise have
// reported. Drain-then-check keeps this capture's own diagnosis honest without taking anything
// away from the game.
//
// Where it breaks, and how it degrades:
//   - 3D drawn AFTER the first glOrtho (a rendered portrait inside a dialogue box, an inset
//     viewport) is absent from the capture, so those pixels differ from the finished frame and
//     read as HUD to a consumer: they simply do not get the effect. Never corruption.
//   - A frame that issues no glOrtho leaves no capture, and GetWorldOnlyFrame() returns false.
//   - A game that draws its HUD without glOrtho never captures at all. False, not a guess.

// glFrustum: a world pass is starting.
void NotifyWorldPassBegan();

// glOrtho: the 2D pass is starting, so the world is complete. Latches on the first call of a
// frame that was armed; later calls in the same frame do nothing.
void NotifyTwoDPassBegan();

// Called at swap AFTER the effect chain has run - the chain is the consumer, so invalidating
// on entry would destroy the very frame it needs. Mirrors AdvanceCameraHistory() in
// modelview_capture.h, which is called at the same point for the same reason.
void InvalidateWorldFrame();

// The world-only frame for THIS frame, or false if none was latched. Never returns a stale
// texture: a frame with no world pass fails closed rather than handing back the previous
// frame's world, because being wrong here would be invisible.
//
// `width`/`height` are the viewport at latch time. A consumer whose own dimensions differ must
// decline rather than sample a mismatched texture.
//
// Also checks the GL context generation, not just LatchWorldFrame()'s writer-side check: if the
// game destroys its context and creates a new one between this frame's first glOrtho and its
// wglSwapBuffers, g_latched would otherwise still read true and hand back a texture name that
// belonged to the dead context. New contexts hand out low texture names, so that name could
// plausibly alias one of the pipeline's own new textures rather than fail visibly - the reader
// has to refuse independently of the writer to catch that window.
bool GetWorldOnlyFrame(unsigned int& texture, int& width, int& height);
