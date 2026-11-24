#pragma once

#include "projection_capture.h"

// Depth of field: blurs what the viewer is not looking at, using the game's own depth buffer.
// One stage in the shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes
// dstTexture, samples depthTexture (the default framebuffer's depth attachment, blitted by the
// caller just before this runs). No capture/blit/state-save of its own beyond that depth sample.
//
// Distances here are in the GAME'S OWN WORLD UNITS, like ssaoRadius and unlike nearly every other
// parameter in this project - which is why this stage needs the captured projection rather than
// just raw depth. Raw hardware depth is hyperbolic and its numbers mean nothing on their own: on a
// real Anachronox frame the whole scene occupies raw 0.92..1.0 while covering 51..1192 world
// units (see depth_view.h). Denominating focus in world units is what makes a setting that works
// in one room still work in the next.
//
// Belongs EARLY in the effect= list, before tone mapping and grading, for the same reason ssao
// does: it wants the game's own unprocessed colour, and blurring after a bloom or a grade smears
// those stages' output rather than the scene.

// Runs the depth-of-field pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture
// and dstTexture are both width x height RGBA16F 2D textures owned by the caller; depthTexture is
// a width x height depth-format 2D texture holding this frame's raw (hardware, non-linear) depth,
// also owned by the caller. `projection` is the frustum the frame was rendered with, from
// projection_capture.h.
//
// focusDistance is GetAnaxConfig().dofFocusDistance, in world units: the distance that stays
// perfectly sharp. 0 means AUTO - focus on whatever the depth buffer holds at the centre of the
// screen, so the stage follows what the player is looking at. focusRange is
// GetAnaxConfig().dofFocusRange, in world units: the width of the fully sharp band centred on the
// focus distance, and also the distance over which the blur ramps from none to maximum beyond it.
// blurStrength is GetAnaxConfig().dofBlurStrength, 0..1, scaling the maximum blur radius; 0
// reproduces the input bit-exact at every depth.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable, shader init failed, the caller could not supply a valid depthTexture this frame,
// or no usable projection was captured - a dump/frame taken before the game first established a
// 3D view has no way to convert depth into the world units focusDistance is expressed in, and
// guessing would blur against a meaningless distance.
bool ApplyDof(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
               int width, int height, const ProjectionParams& projection,
               float focusDistance, float focusRange, float blurStrength);
