#pragma once

#include "projection_capture.h"

// Screen-space ambient occlusion: darkens creases, corners and contact points by estimating how
// much of each pixel's surrounding hemisphere is blocked by nearby geometry. id Tech 2-era
// renderers bake all their lighting into lightmaps and have no ambient occlusion term at all, so
// this adds the contact shadowing that makes objects look seated in the world rather than
// pasted onto it. One stage in the shared post-effect pipeline (see post_effects.cpp): reads
// srcTexture, writes dstTexture, samples depthTexture (the default framebuffer's depth
// attachment, blitted by the caller just before this runs).
//
// Two compute dispatches over one owned intermediate: first an AO pass (unproject depth to
// view-space positions, derive normals from neighboring positions, then a rotated 16-sample
// hemisphere kernel), then a depth-aware blur that also composites the blurred AO into the color
// in the same pass - the blur is not optional, since a 16-sample kernel alone is far too noisy
// to look at.
//
// Unlike every other stage here, this one needs to know the game's projection to work at all:
// the depth buffer is raw and non-linear, and an occlusion radius only means anything in view
// space. `projection` comes from projection_capture.h, which records it off the game's own
// glFrustum calls.

// Runs the SSAO passes, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller; depthTexture is a
// width x height depth-format 2D texture holding this frame's raw depth.
//
// radius is GetAnaxConfig().ssaoRadius, in world/view units: how far from a pixel geometry still
// counts as occluding it. This is the parameter that actually needs tuning per game, since it is
// the only one denominated in the game's own world scale (Quake II units are roughly 1 unit =
// 1 inch, so sensible values are tens of units, not fractions). intensity is
// GetAnaxConfig().ssaoIntensity, 0..1: how dark full occlusion gets, with 0 an exact no-op.
// bias is GetAnaxConfig().ssaoBias, a small view-space epsilon that stops a flat surface from
// occluding itself through depth-precision noise - too low gives banded self-shadowing, too high
// eats real contact shadows.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as the
// pipeline's new source when this returns true); returns false if GL 4.3 compute support is
// unavailable, shader init failed, the caller could not supply a valid depthTexture this frame,
// or no projection has been captured yet (a menu-only frame before the world is first drawn -
// see projection_capture.h).
bool ApplySsao(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
                int width, int height, const ProjectionParams& projection,
                float radius, float intensity, float bias);
