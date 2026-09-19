#pragma once

// Sub-pixel jitter for real temporal antialiasing: the per-frame offset that makes successive
// frames sample DIFFERENT points inside each pixel, so `taa` has genuinely new information to
// accumulate rather than averaging one sample with itself.
//
// This is the only module in the project that changes what the game renders. Every other
// capture module here - projection_capture.h, modelview_capture.h, world_capture.h - is
// recording-only by design and says so. Jitter cannot be: without it TAA has nothing to
// resolve, and it can only be applied at the moment the game establishes its projection.
//
// The maths below is pure so it can be checked without a GL context, and this module is also
// where the per-frame offset and the rule that gates it belong.

// The offset for a frame, in PIXELS, within +/-0.5 of the pixel centre. Halton(2,3) with a
// period of 8: low-discrepancy, so eight consecutive frames cover the pixel far more evenly
// than eight random draws would, and `index` may free-run - it is wrapped here.
void TaaJitterOffset(unsigned int index, float& jx, float& jy);

// Translates a glFrustum window by a sub-pixel offset, in place. Shifting BOTH edges of each
// axis by the same amount leaves the window's size alone and moves only its centre, which is
// exactly a sub-pixel translation of the rendered image - no change of field of view.
//
// `jx`/`jy` are in pixels (from TaaJitterOffset); `width`/`height` are the viewport in pixels.
// `appliedDx`/`appliedDy` report the shift in FRUSTUM UNITS, which is what the resolve
// subtracts to rebuild the unjittered frustum - they are not the same numbers as jx/jy.
//
// Sign convention, pinned by taa_jitter_test.cpp: an offset of +jx moves the rendered image by
// exactly -jx pixels. A zero or negative viewport dimension leaves everything untouched and
// reports no offset, rather than dividing by zero.
void JitterFrustumBounds(double& left, double& right, double& bottom, double& top,
                         float jx, float jy, int width, int height,
                         float& appliedDx, float& appliedDy);
