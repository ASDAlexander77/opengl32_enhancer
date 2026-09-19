#pragma once

#include "modelview_capture.h"
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
// surfaces pointing far enough upward reflect at all, on the reasoning that floors, water and
// polished tables are the things a player expects to see a reflection in, and walls and ceilings
// mostly are not.
//
// What that costs, stated plainly. It is a guess about orientation standing in for a fact about
// material, so it is wrong in both directions: a carpet reflects as readily as marble, and a
// mirror hung on a wall does not reflect at all. That half has not changed and cannot be fixed
// from geometry.
//
// What HAS changed: "up" now means up in the WORLD. It used to mean up from the camera's point
// of view, because a wglSwapBuffers proxy could not see the modelview matrix, so pitching the
// camera swung the gate off true and levelling out restored it. modelview_capture.h supplies the
// camera now, and SsrViewSpaceWorldUp() below turns the game's world up axis into the form the
// shader needs. Two consequences worth knowing: the gate needs to be told WHICH axis is up
// (see SsrWorldUpAxis - it is an engine convention, not something a view matrix reveals), and a
// game whose view matrix is never captured falls back to the old view-space behaviour rather
// than losing reflections.
//
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
// object into empty space. upThreshold is GetAnaxConfig().ssrUpThreshold, 0..1: how far a surface
// must point along world up to reflect, as a dot product - 0 makes everything reflective, 1
// nothing. Its meaning and range are unchanged from when it measured view-space normal.y; it
// simply now measures against the axis it always claimed to.
//
// A pixel with no captured geometry (cleared depth, i.e. sky), a surface that fails the up test,
// and a ray that finds nothing are all handed back unchanged, bit-exact rather than nearly so,
// so that a stage which found nothing is indistinguishable from one that never ran.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable, no depth/projection is available, or shader init failed.
// Which world axis points up in the game's own coordinate system. A view matrix says where the
// camera is and which way it faces, but not which way is up in the WORLD - that is an engine
// convention, and nothing the capture can derive. Quake II-family engines (Anachronox included)
// use +Z, which is why that is the default; id Tech 1 and many others use +Y.
//
// The symptom of getting this wrong is specific and easy to recognise: reflections appear on
// walls and not on floors.
enum class SsrWorldUpAxis {
    X = 0,
    Y = 1,
    Z = 2,
};

// Expresses the game's world up axis in view space, which is the form the gate needs: the shader
// has a surface normal in view space and must ask how far it points up in the WORLD. Pure - no GL
// calls, no state. `camera` is a view matrix from GetCapturedCamera(); see modelview_capture.h.
void SsrViewSpaceWorldUp(const CameraMatrix& camera, SsrWorldUpAxis axis, float outUp[3]);

// viewSpaceWorldUp is the vector SsrViewSpaceWorldUp() produced, or nullptr when no camera has
// been captured. nullptr is not a failure and does not disable the stage: the gate falls back to
// measuring against view-space +Y, which is exactly what it did before world up was available, so
// a game whose view matrix is never captured keeps the reflections it always had.
bool ApplySsr(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
              int width, int height, const ProjectionParams& projection,
              const float* viewSpaceWorldUp,
              float intensity, float maxDistance, float thickness, float upThreshold);
