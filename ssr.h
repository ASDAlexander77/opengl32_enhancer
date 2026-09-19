#pragma once

#include "projection_capture.h"

// Screen-space reflections: surfaces that face upward pick up a mirrored image of whatever the
// frame already contains above them. One stage in the shared post-effect pipeline (see
// post_effects.cpp): reads srcTexture plus the depth attachment, writes dstTexture.
//
// The reflection is traced entirely in view space, reconstructed from depth the same way
// ssao.cpp does it: raw hardware depth unprojects to a view-space position, neighbouring
// positions give a normal, the view vector reflects about that normal, and the resulting ray is
// marched forward a step at a time. Each step is projected back to a screen coordinate and its
// distance compared against what the depth buffer says is actually there - when the ray passes
// BEHIND a surface it has hit it, and that pixel's colour is the reflection.
//
// The hard part is not the trace, it is knowing WHAT should reflect. A real engine has a
// roughness or material channel and consults it per pixel; a GL 1.1 game supplies nothing of the
// kind, so this stage has to guess from geometry alone. The guess is `upThreshold`: only
// surfaces whose view-space normal points far enough upward reflect at all, on the reasoning
// that floors, water and polished tables are the things a player expects to see a reflection in,
// and walls and ceilings mostly are not.
//
// What that costs, stated plainly. It is a guess about orientation standing in for a fact about
// material, so it is wrong in both directions: a carpet reflects as readily as marble, and a
// mirror hung on a wall does not reflect at all. It is also measured in VIEW space, not world
// space, because a wglSwapBuffers proxy never sees the modelview matrix (see
// docs/enhancement-opportunities.md, Tier 2) - so "up" means "up from the camera's point of
// view", and pitching the camera up or down swings the gate off true. Levelling out restores it.
// Finally, only what is already on screen can be reflected: geometry behind the camera or off
// the edge of the frame has no pixels to gather, which is why reflections fade out toward the
// screen border rather than ending abruptly.

// Runs the stage, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller; depthTexture is
// the depth attachment the pipeline blitted for this frame.
//
// projection comes from GetCapturedProjection() and is what makes the reconstruction possible at
// all - without it raw depth is a meaningless hyperbolic number. See projection_capture.h.
//
// intensity is GetAnaxConfig().ssrIntensity, 0..1: how strongly a found reflection is blended
// over the surface. 0 reproduces the input bit-exact. maxDistance is
// GetAnaxConfig().ssrMaxDistance, in the GAME'S OWN WORLD UNITS: how far a ray is allowed to
// travel before giving up, which also sets the step size since the step count is fixed.
// thickness is GetAnaxConfig().ssrThickness, also in world units: how far behind a surface a ray
// may be and still count as having hit it rather than having passed through a thin foreground
// object into empty space. upThreshold is GetAnaxConfig().ssrUpThreshold, 0..1: the minimum
// view-space normal.y for a surface to reflect - 0 makes everything reflective, 1 nothing.
//
// A pixel with no captured geometry (cleared depth, i.e. sky), a surface that fails the up test,
// and a ray that finds nothing are all handed back unchanged, bit-exact rather than nearly so,
// so that a stage which found nothing is indistinguishable from one that never ran.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable, no depth/projection is available, or shader init failed.
bool ApplySsr(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
              int width, int height, const ProjectionParams& projection,
              float intensity, float maxDistance, float thickness, float upThreshold);
