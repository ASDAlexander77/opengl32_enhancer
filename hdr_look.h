#pragma once

// Single-pass ACES filmic tone-mapping curve (Narkowicz 2015 fit) - the "AcesToneMap" stage
// of the fixed post-effect pipeline (see post_effects.cpp), upstream of bloom/LUT grading.
// Output stays ordinary display-referred values (this proxy has no way to signal real
// extended-range output to the display - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md's non-goals for the equivalent
// reasoning about resolution). One stage in the shared post-effect pipeline: reads
// srcTexture, writes dstTexture. No capture/blit/state-save of its own - the caller owns the
// pipeline's shared textures and the app's GL state around the whole chain.

// Runs the tone-mapping pass, called from post_effects.cpp's ApplySelectedEffect().
// srcTexture and dstTexture are both width x height RGBA16F 2D textures owned by the caller.
// strength is GetAnaxConfig().acesStrength, 0..1: 0 reproduces the input unchanged, 1 applies
// the full curve. Returns true if dstTexture was actually written (the caller must only treat
// dstTexture as the pipeline's new source when this returns true); returns false if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplyHdrLook(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float strength);
