#pragma once

// Entry point called from wglSwapBuffers (see wrapper.cpp / generators/gen_wrapper_cpp.py)
// just before the real swap. Reads the cached config (see config.h) and dispatches to the
// selected effect. Never fails or throws: every effect that isn't implemented yet, or that
// needs GL 4.3 compute-shader support this context doesn't have, silently no-ops (the real
// swap still happens, unmodified).
void ApplySelectedEffect();
