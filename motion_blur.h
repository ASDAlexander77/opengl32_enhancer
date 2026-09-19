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

// ORDERING, learned the hard way in Anachronox: a sharpening stage listed AFTER this one
// visibly undoes it. `nvscaler`, `cas` and `sharpen` all amplify local contrast, which is
// precisely what a blur removes, so `effect=..., motionblur, nvscaler, ...` produces a much
// weaker smear than the same list without it - and nothing logs, because both stages are
// working exactly as specified. List sharpeners BEFORE motionblur, or not at all alongside it.
// taa.h's ORDERING block states the exact opposite rule for that stage - a sharpener listed
// AFTER `taa` is the conventional pairing there - so neither rule generalises to the other.
//
// This stage also reads depth, so it must come before any real upscale for the usual reason
// (see StageNeedsDepth in post_effects.h).

// Runs the stage, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are width x height RGBA16F 2D textures owned by the caller; depthTexture is the
// depth attachment the pipeline blitted for this frame.
//
// worldTexture is the pre-HUD frame from world_capture.h and captureTexture is the pipeline's
// pristine back-buffer capture. Where they differ by more than 1/128 the game drew an overlay,
// and that pixel is returned bit-exact - not nearly unchanged, exactly unchanged - and is also
// excluded from every other pixel's blur taps, so dialogue text never bleeds into the world
// behind it. Both must be the same size as width x height; the caller checks that.
//
// projection comes from GetCapturedProjection() and is what makes unprojecting raw depth
// possible at all. reprojection is MotionBlurReprojection()'s 16 floats.
//
// strength is GetAnaxConfig().motionBlurStrength, 0..4: a multiplier on the measured
// screen-space velocity. 0 declines outright and returns false without writing dstTexture at
// all - see the return contract below, not "reproduces the input bit-exact": there is no input
// reproduced into dst for a caller to read. maxRadius is
// GetAnaxConfig().motionBlurMaxRadius, a fraction of the screen: the longest smear allowed.
// That clamp is not a tuning nicety - on a scene cut or teleport the inter-frame camera delta
// is enormous and would smear the whole screen, and the clamp bounds that without needing cut
// detection.
//
// Sky needs no special case: cleared depth unprojects to the far plane, where the translation
// term vanishes and only rotation survives, which is how sky should behave.
//
// Returns true if dstTexture was actually written; returns false if GL 4.3 compute support is
// unavailable, shader init failed, no depth/projection was supplied, worldTexture == 0,
// captureTexture == 0, width <= 0 or height <= 0, or strength <= 0 (0 is now a guard clause, not
// a dispatch that merely happens to be bit-exact - see the .cpp).
bool ApplyMotionBlur(unsigned int srcTexture, unsigned int dstTexture,
                     unsigned int depthTexture, unsigned int worldTexture,
                     unsigned int captureTexture,
                     int width, int height, const ProjectionParams& projection,
                     const float reprojection[16],
                     float strength, float maxRadius);
