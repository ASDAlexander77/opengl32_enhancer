#pragma once

// Records the game's CAMERA (view) matrix once per frame, so stages can relate this frame's
// pixels to the previous frame's. Recording only - like projection_capture.h, every hook here
// is a notification issued before the real call is forwarded, and none of them changes what the
// game renders.
//
// projection_capture.h already records the world PROJECTION. That is only half of what is needed
// to place a pixel in space: the projection says how view space maps to the screen, but not
// where the camera is or which way it points. taa.h states the consequence outright - "this
// proxy has no access to the app's per-object motion vectors" - and ssr.h is forced to measure
// its up-facing gate in view space rather than world space for the same reason.
//
// This module never computes a matrix. It decides WHEN the modelview matrix holds the camera,
// and then reads the driver's own GL_MODELVIEW_MATRIX. That way the captured value is
// bit-for-bit what the game's GL actually holds and cannot drift from it.
//
// Knowing when is the hard part, and it is a heuristic, not a fact. id Tech 2-era engines draw
// the HUD last under an orthographic projection, and every entity in the world pass overwrites
// the modelview with its own object transform, so "the matrix at swap time" is neither. The
// rules, which mirror and extend projection_capture.h's:
//
//   - glFrustum arms a world pass; glOrtho disarms it. Exactly the discrimination
//     projection_capture.h already relies on, reused.
//   - Modelview edits count only at push/pop depth 0. Quake II-family engines issue every
//     entity transform inside glPushMatrix/glPopMatrix - R_RotateForEntity, R_DrawSkyBox, the
//     alias and brush model paths - so an edit at depth >= 1 is an object transform.
//   - The read is deferred to the first moment the camera is known to be final: a depth-0
//     glPushMatrix, a glOrtho, or the swap, whichever comes first. All three are safe because
//     the modelview still holds the camera at each - glPushMatrix copies the top of stack
//     without modifying it, and Quake II's R_SetGL2D issues its glOrtho before it touches the
//     modelview. Exactly one glGetFloatv per frame results, on the happy path - if the m[15]
//     sentinel below bails out, the pending flag stays set and the next latch point retries.
//   - The first world pass of a frame wins. A later glFrustum in the same frame (a viewmodel at
//     a different FOV, a scope) is a sub-pass of the same camera.
//
// Against Quake II's R_SetupGL this reads: arm at glFrustum; the glLoadIdentity, five glRotatef
// and glTranslatef that follow set the pending flag; the first entity glPushMatrix latches the
// complete camera matrix.
//
// Where it breaks, stated plainly:
//   - A frame with no world pass at all (a menu-only frame) leaves the last camera standing,
//     stale. Same behaviour as the captured frustum.
//   - An engine that establishes its 3D view without glFrustum - a hand-built matrix through
//     glLoadMatrixf - captures nothing, and GetCapturedCamera() stays false. Consumers must
//     no-op rather than guess.
//   - An engine that edits the modelview at depth 0 BEFORE its camera sequence and then never
//     pushes would latch the wrong matrix.
//
// Not synchronized; assumes all calls come from the single render thread, same as gl_loader.h
// and projection_capture.h.

// A view matrix in GL's own layout: column-major, sixteen floats, exactly as
// glGetFloatv(GL_MODELVIEW_MATRIX) hands it over. Element (row, col) is m[col * 4 + row].
// Defaults to the identity so a caller that ignores the bool gets something harmless.
struct CameraMatrix {
    float m[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.0f, 0.0f, 0.0f, 1.0f};
};

// The same thing in world space, which is what a human reading a log and a stage reasoning
// about "up" both actually want. R is the view matrix's upper-left 3x3; its ROWS are the
// camera's axes expressed in world coordinates, which is why right/up/forward are each reached
// by a stride-4 walk of the stored array.
// Deliberately zero rather than an identity-looking default: a zero basis is obviously
// uninitialised at a glance in a log, where (0,0,-1) would read as a real direction.
struct CameraPose {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float right[3]    = {0.0f, 0.0f, 0.0f};
    float up[3]       = {0.0f, 0.0f, 0.0f};
    float forward[3]  = {0.0f, 0.0f, 0.0f};
};

// --- Hooks, called from the generated wrapper (see generators/gen_wrapper_cpp.py) before the
// --- corresponding real call is forwarded. Recording only.

// glFrustum: a world pass begins.
void NotifyWorldProjection();

// glOrtho: the 2D/HUD pass begins. Latches the camera if one is pending, then disarms.
void NotifyTwoDProjection();

// glMatrixMode: which stack subsequent matrix calls apply to.
void NotifyMatrixMode(unsigned int mode);

// glPushMatrix / glPopMatrix: tracked so entity transforms can be told from camera setup.
// A push from depth 0 with a camera pending is the primary latch point.
void NotifyMatrixPush();
void NotifyMatrixPop();

// Any of the eleven calls that modify the current matrix: glLoadIdentity, glLoadMatrixf/d,
// glMultMatrixf/d, glRotatef/d, glTranslatef/d, glScalef/d. Deliberately does not take the
// arguments - this module never replays the algebra, it only notes that something changed.
void NotifyMatrixEdited();

// wglSwapBuffers, BEFORE the post-effect chain: last chance to latch, for a frame that drew no
// entities and no HUD.
void FinalizeCameraForFrame();

// wglSwapBuffers, AFTER the post-effect chain: this frame's camera becomes the previous one and
// the per-frame state is cleared. Runs after the chain so a stage reading during it sees a
// stable current/previous pair.
void AdvanceCameraHistory();

// --- Accessors.

// The last camera ever latched, or false if no world pass has ever been captured. This can be
// stale: a frame with no world pass (see "Where it breaks" above) leaves it holding whatever the
// last successful latch recorded.
bool GetCapturedCamera(CameraMatrix& out);

// The camera latched for the previous frame, or false until the first frame finishes - at which
// point it returns true with `previous` equal to `current`, since there is only one latch to
// report, until the next frame's latch moves `current` on. What reprojection needs; nothing
// consumes it yet.
bool GetPreviousCamera(CameraMatrix& out);

// Turns a view matrix into a world-space position and basis. Pure function, no state.
void DecomposeCamera(const CameraMatrix& in, CameraPose& out);
