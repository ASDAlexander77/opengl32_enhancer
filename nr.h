#pragma once

// Noise reduction: an edge-aware (bilateral) spatial denoiser. Iterates a 5x5 bilateral filter
// `passes` times (each pass reads the previous pass's output, so passes compound), then a
// final combine pass compares the fully-filtered result against the ORIGINAL src pixel to
// split the change into a luma component and a chroma component and re-mix them independently
// - see nr.cpp for why. One stage in the shared post-effect pipeline (see post_effects.cpp):
// reads srcTexture, writes dstTexture. No capture/blit/state-save of its own - the caller owns
// the pipeline's shared textures and the app's GL state around the whole chain. Owns its own
// filter intermediate textures (RGBA16F, resized alongside width/height) since those aren't
// part of the shared pipeline.

// Runs the NR pass, called from post_effects.cpp's ApplySelectedEffect(). srcTexture and
// dstTexture are both width x height RGBA16F 2D textures owned by the caller.
//
//   intensity          GetAnaxConfig().nrIntensity, 0..1: how large a luma difference the
//                       bilateral filter still treats as "the same surface" (its range sigma).
//                       0 makes every pass an exact no-op (the filter degenerates to picking
//                       out only exactly-matching neighbors); 1 is the most aggressive.
//   passes             GetAnaxConfig().nrPasses, 1..4: how many times the bilateral filter
//                       runs in sequence. More passes compounds the smoothing from a single
//                       pass, independent of how strong each individual pass is (that's what
//                       `intensity` controls) - the two knobs are deliberately orthogonal.
//   colorStrength      GetAnaxConfig().nrColorStrength, 0..1: how much of the filtered result's
//                       CHROMA (color) to keep, as a multiplier on top of whatever `intensity`
//                       already produced. 0 leaves color untouched by NR; 1 uses the full
//                       filtered chroma.
//   tonePreservation   GetAnaxConfig().nrTonePreservation, 0..1: how much to protect LUMA
//                       (brightness/detail) specifically from the filter, independent of
//                       chroma. 0 lets luma be denoised same as color; 1 leaves luma untouched
//                       regardless of `intensity`/`passes` - this is the standard "denoise
//                       color harder than brightness" split most real video/photo denoisers
//                       default toward, since chroma noise reads as far uglier than luma noise
//                       at the same magnitude.
//   grainPreservation  GetAnaxConfig().nrGrainPreservation, 0..1 (0=off): re-adds back some of
//                       the luma detail the filter actually removed (the real per-pixel
//                       residual between original and filtered luma, not synthetic noise -
//                       contrast with FSR's fsrFilmGrain, which bakes in a synthetic pattern).
//                       Lets NR smooth without also flattening a deliberately grainy look.
//
// Returns true if dstTexture was actually written (the caller must only treat dstTexture as
// the pipeline's new source when this returns true); returns false if GL 4.3 compute support
// is unavailable or shader init failed.
bool ApplyNr(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
             float intensity, int passes, float colorStrength, float tonePreservation,
             float grainPreservation);
