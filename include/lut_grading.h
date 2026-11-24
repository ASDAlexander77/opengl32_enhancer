#pragma once

// 3D-LUT color grading: loads an Adobe/Iridas-style ".cube" text LUT file into a
// GL_TEXTURE_3D and samples it per-pixel (each pixel's own RGB acts as its own lookup
// coordinate into the cube - the standard LUT-grading technique). One stage in the shared
// post-effect pipeline (see post_effects.cpp): reads srcTexture, writes dstTexture. No
// capture/blit/state-save of its own - the caller owns the pipeline's shared textures and the
// app's GL state around the whole chain. Domain is always assumed [0,1]^3 (a .cube file's
// DOMAIN_MIN/DOMAIN_MAX lines, if present, are ignored - not needed for any LUT this proxy
// ships with).

// Runs the grading pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller. lutPath is
// GetAnaxConfig().lutPath; strength is GetAnaxConfig().lutStrength, 0..1: 0 reproduces the
// input unchanged, 1 applies the full LUT. Returns true if dstTexture was actually written
// (the caller must only treat dstTexture as the pipeline's new source when this returns
// true); returns false if GL 4.3 compute support is unavailable, shader init failed, lutPath
// is empty, or the file at lutPath doesn't exist / isn't a valid .cube file. Re-parses the
// file whenever lutPath changes from the previous call (e.g. via a config reload), not on
// every call.
bool ApplyLutGrading(unsigned int srcTexture, unsigned int dstTexture, int width, int height, const char* lutPath, float strength);
