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
// The maths (TaaJitterOffset, JitterFrustumBounds) and the per-frame state that drives it
// (NotifyTaaRealPathRan, AdvanceTaaJitter, ApplyTaaJitterToFrustum, GetTaaJitterApplied,
// GetPreviousTaaJitterApplied) are all pure - no GL dependency at all - and are what
// taa_jitter_test.cpp checks without a context. ApplyTaaJitterToFrustumFromCurrentViewport is
// the one exception below: it reads the live viewport and hands off to the pure
// ApplyTaaJitterToFrustum, and is the only one of these the generated glFrustum wrapper needs
// to call.

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

// --- Per-frame state.

// wglSwapBuffers, AFTER the post-effect chain: moves to the next offset in the sequence and
// rotates this frame's applied offset into the previous slot. Same tick as
// AdvanceCameraHistory() and AdvanceProjectionHistory() - see those for why they must agree.
void AdvanceTaaJitter();

// Shifts left/right/bottom/top by this frame's offset via JitterFrustumBounds, if armed; a
// no-op that leaves the bounds bit-identical otherwise. Pure - `viewportWidth`/`viewportHeight`
// are passed in rather than read from GL here, which is what lets taa_jitter_test.cpp check
// this with no GL context at all. Every glFrustum call within one frame receives that frame's
// single offset, so a viewmodel drawn at a different field of view stays consistent with the
// world.
void ApplyTaaJitterToFrustum(double& left, double& right, double& bottom, double& top,
                             int viewportWidth, int viewportHeight);

// What the generated glFrustum wrapper (see generators/gen_wrapper_cpp.py) actually calls,
// before CaptureProjectionFrustum, so the capture records the frustum GL is actually given.
// Reads GL_VIEWPORT through GetGlComputeApi(), skipping the read entirely when gl.loaded is
// false (no current GL context - true of every unit test in this project), and otherwise
// delegates to ApplyTaaJitterToFrustum() above. Deliberately has no unit test of its own:
// beyond the gl.loaded guard it is nothing but a viewport read and a delegation, and the logic
// actually worth checking already has a test, on the pure function above.
void ApplyTaaJitterToFrustumFromCurrentViewport(double& left, double& right,
                                                double& bottom, double& top);

// The offset applied this frame and last, in FRUSTUM UNITS. False when that frame was not
// jittered. The resolve subtracts the PREVIOUS one to rebuild the unjittered previous frustum.
bool GetTaaJitterApplied(float& dx, float& dy);
bool GetPreviousTaaJitterApplied(float& dx, float& dy);

// Called by taa.cpp every frame the stage runs, reporting whether its REAL path ran (as opposed
// to the TAA-lite fallback). This arms the next frame's jitter.
//
// Jitter that nothing resolves is not neutral - it is pure added shimmer - so it is switched on
// only by evidence that a real resolve is consuming it. The one-frame lag is harmless because
// the first frames have no history and output the current frame unchanged regardless. This
// cannot deadlock: the real path's preconditions are depth, both projections, both cameras and
// the world capture - not jitter - so the first world frame runs the real path un-jittered and
// arms the second.
void NotifyTaaRealPathRan(bool ran);
