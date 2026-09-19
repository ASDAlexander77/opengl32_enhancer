#pragma once

// Records the 3D view's projection frustum as the game sets it, so depth-consuming post-process
// stages can turn the raw (non-linear, hardware) depth buffer back into view-space positions.
//
// A wglSwapBuffers proxy cannot just read GL_PROJECTION_MATRIX at swap time: id Tech 2-era
// engines draw the HUD/2D overlay last, under an orthographic projection, so by the time the
// frame is presented the current projection matrix is the HUD's, not the world's. Intercepting
// glFrustum instead (see generators/gen_wrapper_cpp.py) catches the world projection at the
// moment the engine establishes it, and keeping the most recent one seen means the HUD's later
// glOrtho never overwrites it - glOrtho is a different entry point and is not recorded here.
//
// Quake II engines (Anachronox included) set the 3D projection through MYgluPerspective, which
// calls glFrustum, so this is the entry point that actually fires. A game that builds its
// projection some other way (glLoadMatrixf with a hand-built matrix) records nothing, and
// HasCapturedProjection() stays false - stages must then no-op rather than guess.

struct ProjectionParams {
    // glFrustum's own arguments, kept verbatim. left/right/bottom/top are the near plane's
    // extents, so the frustum may legitimately be asymmetric (off-center projections) - the
    // unprojection in ssao.cpp handles that rather than assuming a centered frustum.
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
    float zNear = 0.0f;
    float zFar = 0.0f;
};

// Called from the generated glFrustum wrapper on every call the game makes, before forwarding
// it to the real opengl32.dll. Records whatever left/right/bottom/top it is handed, which is
// not always what the game itself asked for: taa_jitter.h's ApplyTaaJitterToFrustum() is the
// one thing in this project designed to shift those bounds before this function ever sees them.
// That is the right value to keep, not a bug to work around - the depth buffer for this frame is
// rendered with the (possibly jittered) matrix GL was actually given, so every depth consumer
// that reads what is stored here unprojects correctly against that same matrix, with no need to
// know jitter happened at all.
// Degenerate frusta (zNear <= 0, zFar <= zNear, or an empty extent) are ignored rather than
// recorded, since they would produce a divide-by-zero or inverted unprojection downstream.
void CaptureProjectionFrustum(double left, double right, double bottom, double top,
                               double zNear, double zFar);

// The most recent valid frustum CaptureProjectionFrustum() saw, or false if the game has not
// called glFrustum at all yet (a menu-only frame before the world is first drawn, or an engine
// that builds its projection matrix some other way). Not synchronized; assumes all calls come
// from the single render thread, same as gl_loader.h.
bool GetCapturedProjection(ProjectionParams& out);

// The frustum captured on the PREVIOUS frame, or false until two frames have been seen. What
// reprojection needs: a pixel's position this frame is only meaningful against where the same
// point projected last frame, and a game may change field of view between the two.
bool GetPreviousProjection(ProjectionParams& out);

// wglSwapBuffers, AFTER the post-effect chain: this frame's frustum becomes the previous one,
// IF this frame captured one. A frame with no glFrustum call (a menu, a loading screen) leaves
// the previous slot alone rather than re-promoting an older frustum as though it belonged to
// the frame just finished - see the definition for what that would cost a consumer.
// Deliberately the same tick as modelview_capture.h's AdvanceCameraHistory() and
// taa_jitter.h's AdvanceTaaJitter(), because a consumer reads projection, camera and jitter
// history as one set describing one frame - if they advanced at different moments they would
// describe different frames and the reprojection would be silently wrong. The promotion rule
// matches AdvanceCameraHistory()'s for the same reason: that one promotes only if the camera
// was latched this frame, this one only if a frustum was captured.
//
// One residual, stated rather than implied: taa_jitter.h's history is the third member of that
// set and does NOT follow this rule - it clears its "has previous" flag on any frame that
// applied no jitter. So after a frame with no glFrustum, this slot still holds the last
// world-pass frustum (with that frame's jitter baked in) while GetPreviousTaaJitterApplied()
// reports nothing to subtract, and the resolve is off by up to half a pixel until the next
// jittered frame rotates through. Bounded and transient, and it is a taa_jitter.h question,
// not one this file can answer on its own.
void AdvanceProjectionHistory();
