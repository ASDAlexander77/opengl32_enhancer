#pragma once

// Volumetric light shafts ("god rays"): streaks of light radiating from a bright source, built by
// marching each pixel toward that source and accumulating what it passes through. One stage in
// the shared post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture. No
// depth and no projection needed, unlike ssao/dof/fog - which also means this is the one
// depth-era stage that still works on a frame taken before the game established a 3D view.
//
// The hard part, and the reason this is two passes: a post-process shaft effect needs to know
// where the light is on screen, and a wglSwapBuffers proxy has no idea. A configurable screen
// position would be useless in practice - the camera moves, and a position tuned in one room is
// wrong in the next - so the light is DETECTED from the frame instead. The first pass finds the
// brightness-weighted centroid of everything above `threshold`; the second radiates from it.
//
// What that costs, stated plainly: the detector cannot tell a lamp from a bright wall, so a
// large pale surface can pull the centroid toward it and bend the shafts somewhere no light
// actually is. Raising `threshold` until only genuine highlights qualify is the lever for that.
// And when the light is off-screen there is nothing to find, which is exactly when a real engine
// would still draw shafts - a limitation inherent to detecting from the frame rather than being
// told by the renderer.

// Runs both passes, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller.
//
// intensity is GetAnaxConfig().shaftsIntensity, 0..1: how much shaft light is added on top of
// the scene. 0 reproduces the input bit-exact. density is GetAnaxConfig().shaftsDensity, 0..1:
// how far along the ray toward the light each pixel marches, as a fraction of its distance to
// it - low values keep shafts short and tight to the source, high values sweep them across the
// frame. decay is GetAnaxConfig().shaftsDecay, 0..1: how quickly a sample's contribution falls
// off with each step, which is what gives a shaft its taper. threshold is
// GetAnaxConfig().shaftsThreshold, 0..1: the luma above which a pixel counts as light, both for
// finding the source and for what the march accumulates.
//
// When no pixel exceeds `threshold` there is no light to radiate from, and the source is handed
// back untouched rather than shafts being invented from a zero-weight centroid.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable or shader init failed.
bool ApplyLightShafts(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                       float intensity, float density, float decay, float threshold);
