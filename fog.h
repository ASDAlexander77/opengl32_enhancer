#pragma once

#include "projection_capture.h"

// Per-pixel distance fog, computed from the game's own depth buffer. One stage in the shared
// post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture, samples
// depthTexture (the default framebuffer's depth attachment, blitted by the caller just before
// this runs). No capture/blit/state-save of its own beyond that depth sample.
//
// Not the same thing as depthvignette, which fades distant pixels toward BLACK by raw depth. This
// fades them toward a COLOR, over a range denominated in the game's own world units. The color is
// the point: real atmospheric haze lightens and tints distance, it does not darken it, and a
// grey-blue distance is what makes a corridor read as having air in it.
//
// id Tech 2-era engines do have their own fog, but it is per-vertex on geometry that can be very
// coarse, so it bands badly across large surfaces. This is computed per pixel from real depth,
// which is the actual improvement on offer - and it applies to everything the depth buffer knows
// about, including surfaces the engine's own fog never touched.
//
// The ramp is linear between fogStart and fogEnd, matching GL_LINEAR fog, which is the model
// these engines used and therefore the one whose numbers behave the way someone tuning them will
// expect. Belongs EARLY in the effect= list, before tone mapping and grading: fog is part of the
// scene, so it should be graded along with everything else rather than painted over the grade.

// Runs the fog pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller; depthTexture is a
// width x height depth-format 2D texture holding this frame's raw (hardware, non-linear) depth,
// also owned by the caller. `projection` is the frustum the frame was rendered with, from
// projection_capture.h.
//
// fogStart and fogEnd are GetAnaxConfig().fogStart/fogEnd, in world units: no fog at or before
// fogStart, full fog at or beyond fogEnd, linear in between. fogEnd is expected to exceed
// fogStart; if it does not, the ramp collapses and every pixel beyond fogStart is fully fogged
// rather than producing a divide-by-zero. intensity is GetAnaxConfig().fogIntensity, 0..1,
// scaling the whole ramp - 0 reproduces the input bit-exact at every depth. colorR/G/B are
// GetAnaxConfig().fogColorR/G/B, 0..1.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable, shader init failed, the caller could not supply a valid depthTexture this frame,
// or no usable projection was captured - without a frustum there is no way to turn raw depth into
// the world units fogStart/fogEnd are expressed in.
bool ApplyFog(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
               int width, int height, const ProjectionParams& projection,
               float fogStart, float fogEnd, float intensity,
               float colorR, float colorG, float colorB);
