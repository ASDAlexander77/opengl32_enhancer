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

// The colour space a value in the pipeline is currently in.
//
// `Linear` is light: the quantity that may legitimately be averaged, blurred, thresholded or
// tone-mapped. `Display` is sRGB-encoded - what the 8-bit back buffer holds and what the game
// handed us. The distinction matters because sRGB is roughly a 2.2-power encoding, so the
// average of two encoded values is NOT the encoding of their average.
enum class ColorSpace {
    Linear,
    Display,
};

// Which space a stage wants to be handed. Only consulted when srgbCorrect=1; with it off the
// chain never asks, and no conversion is ever generated.
//
// Three of the twenty-five stages want `Display`, and each for the same underlying reason -
// they are about the OUTPUT rather than about the light. Everything else averages, filters or
// blends, and all of those are only meaningful on light.
//
// `acestonemap` is deliberately `Linear` and this is the subtle one: the Narkowicz fit takes
// linear scene light and produces a display-referred range whose values must still be encoded
// afterwards (`color = ACESFitted(linear); color = encode(color);`). Its output is therefore
// still 'linear, not yet encoded'. That is what lets this be a single per-stage lookup rather
// than a separate input and output space for every stage.
ColorSpace ColorSpaceFor(EffectKind stage);

// Whether ApplySelectedEffect can return without doing anything. Extracted as a predicate
// rather than left inline because the supersampling term is the line that stands between a
// working frame and a black screen: with a render target armed, this function's present blit
// is the ONLY thing that moves the offscreen image onto the display, so returning early
// there shows the player nothing at all. Inline, that term was pinned by no test - the suite
// stayed green with it deleted.
bool ShouldSkipEffectChain(int stageCount, bool hasRealUpscale, bool supersampleActive);

// Whether ApplySelectedEffect can return before it touches GL at all - the earlier of the two
// early returns, over config alone plus the supersampling latch. Extracted for the same reason
// as ShouldSkipEffectChain, and after the same bug: the supersampling term was missing here
// entirely, so effect=none with no frame-dump key and no window override was a black screen
// whenever a render target was armed. A predicate is a thing a truth table can pin; an inline
// condition, as this one showed twice, is not.
bool ShouldSkipAllWork(int stageCount, int frameDumpKey, bool windowSizeOverrideActive,
                       bool supersampleActive);

// How many exact halvings the present resolve should run before its final blit, shrinking
// srcWidth x srcHeight towards dstWidth x dstHeight.
//
// The resolve used to be a single GL_LINEAR glBlitFramebuffer, and that is one bilinear tap -
// at most a 2x2 box, wherever the destination pixel's centre happens to land. At exactly 2x it
// is the right answer and the whole frame resolves correctly; above 2x it throws away most of
// the samples supersampling just paid four, nine or twenty-five times the fill rate to produce,
// and the image still aliases. renderWidth/renderHeight are free-form, so ratios above 2x are
// not an exotic configuration - they are what anyone typing a big number gets.
//
// A GL_LINEAR blit that halves a dimension exactly IS a 2x2 box average, so N of them average a
// 2^N x 2^N box, and the final blit covers whatever non-power-of-two remainder is left. Both
// axes halve together so the aspect ratio never changes partway through.
//
// Returns 0 whenever the source is already within 2x of the destination, which includes every
// frame with supersampling off - so the unsupersampled present is bit-for-bit the blit it has
// always been.
int ResolveHalvingSteps(int srcWidth, int srcHeight, int dstWidth, int dstHeight);
