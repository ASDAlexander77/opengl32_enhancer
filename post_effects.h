#pragma once

// Entry point called from wglSwapBuffers (see wrapper.cpp / generators/gen_wrapper_cpp.py)
// just before the real swap. Reads the cached config (see config.h) and dispatches to the
// selected effect. Never fails or throws: every effect that isn't implemented yet, or that
// needs GL 4.3 compute-shader support this context doesn't have, silently no-ops (the real
// swap still happens, unmodified).
//
// `hdc` is the HDC the game passed to wglSwapBuffers - needed (via window_override.h's
// GetWindowClientSize) to tell the game's own render resolution (GL_VIEWPORT) apart from the
// real window's client size, which is what makes an upscaler stage (bilinear/nvscaler/fsr)
// into a REAL upscale - the game rendering smaller than the window, reconstructed up to fill it
// - rather than the resample-a-same-size-capture preview `scale` alone gives you. See
// ApplySelectedEffect()'s header comment in post_effects.cpp for the full mechanism.
void ApplySelectedEffect(void* hdc);
