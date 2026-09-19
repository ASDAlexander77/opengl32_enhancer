#pragma once

#include "modelview_capture.h"
#include "projection_capture.h"

// Camera motion blur: smears the frame along the camera's own movement between the previous
// frame and this one. One stage in the shared post-effect pipeline (see post_effects.cpp).
//
// CAMERA motion only, permanently. A character walking across a stationary frame does not
// smear: the proxy sees fixed-function geometry, not per-object transforms, so an object's
// velocity does not exist anywhere in the interception surface to be read. This is a property
// of what can be intercepted, not a feature deferred to later.

// Builds the matrix that carries a view-space position from THIS frame's camera into the
// PREVIOUS frame's camera: M = previous * inverse(current). Pure - no GL calls, no state.
//
// A view matrix is rigid, so the inverse is Rt-transpose and -Rt-transpose * t; no general 4x4
// inversion is needed or wanted. `out` is column-major like CameraMatrix itself, so element
// (row, col) is out[col * 4 + row].
void MotionBlurReprojection(const CameraMatrix& current, const CameraMatrix& previous,
                            float out[16]);
