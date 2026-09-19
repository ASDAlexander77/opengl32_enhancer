#pragma once

#include "config.h"  // EffectKind

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

// Which stages read the game's depth buffer. The pipeline consults this TWICE and the two uses
// must not drift apart, which is why it is one function and not two lists: once before the
// chain runs, to decide whether to allocate and blit the depth texture at all (a stage whose
// depth was never captured no-ops, silently and with nothing in the log to explain it), and
// once per stage inside the chain, to skip a depth stage listed after an upscaler - the game's
// depth exists only at its native resolution, so past that point there is nothing left to
// sample. Exported for post_effects_test.cpp, which pins the set.
//
// Keeping this in step with ApplySelectedEffect()'s switch is manual: if you add a stage that
// takes g_pipeline.depthTex, add it here too. It was out of step once - `ssr` consumed depth
// without being listed, so `effect=ssr` on its own allocated no depth texture and the stage did
// nothing at all, while `effect=ssao, ssr` worked because ssao happened to request the depth
// that ssr then used.
bool StageNeedsDepth(EffectKind stage);
