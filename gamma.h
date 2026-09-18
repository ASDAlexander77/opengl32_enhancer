#pragma once

// Single-pass gamma / brightness correction - the "Gamma" stage of the post-effect pipeline
// (see post_effects.cpp). This is the "the game is too dark on my monitor" control, and it is a
// real gamma curve rather than an additive lift: brightness is applied as a linear GAIN and the
// gamma exponent on top of it, so black stays black at every setting instead of being raised
// off the floor into a washed-out grey the way `+ offset` would.
//
// Per pixel, in this order:
//     c = max(color.rgb * brightness, 0)
//     c = pow(c, vec3(1 / gamma))
//
// brightness first so it is a gain on scene values, not on already-encoded ones. The max()
// keeps pow() off negatives (which would be NaN); values above 1.0 are left alone, since the
// pipeline's textures are RGBA16F and this stage can legitimately sit upstream of tone mapping.
// Alpha passes through untouched.
//
// Placement: normally late in the effect= list, after grading and before `dither` - it is an
// output correction, so it wants to see the finished image. Nothing enforces that, and putting
// it early (as an exposure control feeding tone mapping) is a legitimate thing to try.
//
// One stage in the shared post-effect pipeline: reads srcTexture, writes dstTexture. No
// capture/blit/state-save of its own - the caller owns the pipeline's shared textures and the
// app's GL state around the whole chain.

// Runs the gamma/brightness pass, called from post_effects.cpp's ApplySelectedEffect().
// srcTexture and dstTexture are both width x height RGBA16F 2D textures owned by the caller.
// gamma is GetAnaxConfig().gamma, 0.5..3.0, and brightness is GetAnaxConfig().brightness,
// 0..2. gamma=1 with brightness=1 reproduces the input EXACTLY - the pow() is skipped outright
// at gamma=1 rather than relying on pow(x, 1.0) being bit-exact, so the documented no-op really
// is one. Returns true if dstTexture was actually written (the caller must only treat
// dstTexture as the pipeline's new source when this returns true); returns false if GL 4.3
// compute support is unavailable or shader init failed.
bool ApplyGamma(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                float gamma, float brightness);
