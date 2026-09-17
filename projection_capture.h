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
// it to the real opengl32.dll. Recording only - never changes what the game asked for.
// Degenerate frusta (zNear <= 0, zFar <= zNear, or an empty extent) are ignored rather than
// recorded, since they would produce a divide-by-zero or inverted unprojection downstream.
void CaptureProjectionFrustum(double left, double right, double bottom, double top,
                               double zNear, double zFar);

// The most recent valid frustum CaptureProjectionFrustum() saw, or false if the game has not
// called glFrustum at all yet (a menu-only frame before the world is first drawn, or an engine
// that builds its projection matrix some other way). Not synchronized; assumes all calls come
// from the single render thread, same as gl_loader.h.
bool GetCapturedProjection(ProjectionParams& out);
