#pragma once

// 3D-LUT color grading: loads an Adobe/Iridas-style ".cube" text LUT file into a
// GL_TEXTURE_3D and samples it per-pixel (each pixel's own RGB acts as its own lookup
// coordinate into the cube - the standard LUT-grading technique). Same capture/compute/blit
// structure as bilinear_upscale.cpp's ApplyBilinearUpscale(), plus a 3D texture instead of a
// second 2D one. Domain is always assumed [0,1]^3 (a .cube file's DOMAIN_MIN/DOMAIN_MAX lines,
// if present, are ignored - not needed for any LUT this proxy ships with).

// Runs the grading pass on the current back buffer, called from wglSwapBuffers via
// post_effects.cpp. lutPath is GetAnaxConfig().lutPath; strength is
// GetAnaxConfig().lutStrength, 0..1: 0 reproduces the input unchanged, 1 applies the full LUT.
// Safe to call every frame. A no-op (falls back silently, leaving the back buffer untouched)
// if GL 4.3 compute support is unavailable, shader init failed, lutPath is empty, or the file
// at lutPath doesn't exist / isn't a valid .cube file. Re-parses the file whenever lutPath
// changes from the previous call (e.g. via a config reload), not on every call.
void ApplyLutGrading(const char* lutPath, float strength);
