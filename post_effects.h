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
// once per stage inside the chain, to detect a depth stage listed after an upscaler - the game's
// depth exists only at its native resolution, so past that point there is nothing left to
// sample. What the chain then DOES about that is StageIsSkippedWhenDepthUnavailable()'s
// question, not this one's. Exported for post_effects_test.cpp, which pins the set.
//
// Keeping this in step with ApplySelectedEffect()'s switch is manual: if you add a stage that
// takes g_pipeline.depthTex, add it here too. It was out of step once - `ssr` consumed depth
// without being listed, so `effect=ssr` on its own allocated no depth texture and the stage did
// nothing at all, while `effect=ssao, ssr` worked because ssao happened to request the depth
// that ssr then used.
//
// This stays here, rather than moving to config.h alongside the analogous StageNeedsWorldCapture,
// because it has exactly one consumer - post_effects.cpp, which declares and defines it in the
// same file this header belongs to. StageNeedsWorldCapture has two consumers in two different
// translation units (post_effects.cpp and world_capture.cpp), so IT lives in config.h, the
// vocabulary both already share. Same reasoning, opposite header, because the two predicates
// have a different number of callers.
bool StageNeedsDepth(EffectKind stage);

// Of the stages StageNeedsDepth() names, which must be dropped from the chain outright when the
// pipeline has already passed a real upscale and the game's depth is no longer available at the
// current resolution. Every one of them except `taa`, which alone has a depth-free second
// implementation (taa.h's ApplyTaa, the motion-vector-free TAA-lite) and therefore falls through
// to that instead of vanishing. See the definition in post_effects.cpp for why this is a named
// exception rather than a general one, and post_effects_test.cpp, which pins the set.
//
// Asking this about a stage StageNeedsDepth() returns false for is meaningless and answers
// false; the chain only consults it inside the `isDepthStage && !atNativeRes` branch.
bool StageIsSkippedWhenDepthUnavailable(EffectKind stage);

// Whether the chain lists any stage that reconstructs a smaller source up to a larger
// destination (bilinear/nvscaler/fsr). Supersampling renders above native and downsamples on
// present instead, so once it is active these stages have nothing left to reconstruct - see the
// call site in ApplySelectedEffect(), which uses this only to log that once, not to change the
// chain. Same-file reasoning as StageNeedsDepth: one consumer, post_effects.cpp.
bool AnyUpscaleStageListed(const AnaxConfig& config);
