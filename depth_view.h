#pragma once

#include <cstddef>

// Turns the depth plane of a frame dump (see frame_dump.h) into something a human can actually
// look at - the config editor's "Depth" view.
//
// Why this is not simply "show the depth values as gray": raw hardware depth is hyperbolic, so a
// real frame occupies a thin sliver at the top of the 0..1 range. The measured 1600x1200
// Anachronox dump this module was built against (frustum zNear=4, zFar=8192) spans raw
// 0.922031..0.997131 - 7.5% of the range - even though it covers roughly 51..1192 world units of
// actual scene. Drawn raw, that is a flat rectangle, which is exactly why a dump can look as
// though it "has no depth" when in fact it has plenty. Normalizing across the frame's OWN span
// is what makes it readable.
//
// The display normalizes RAW depth rather than linearized distance, deliberately. Raw
// normalization is a histogram stretch of a hyperbolic curve, so it spends most of its contrast
// on near geometry - which is where SSAO does its work, and therefore where someone tuning
// ssaoRadius needs to see detail. Linearized distance would push almost the whole frame to white
// (on that dump the midpoint of the raw range is only ~98 units out of a 51..1192 span).
// LinearizeDepth() is still here, for reporting the frame's span in the game's own world units -
// the unit ssaoRadius is denominated in - which belongs in a text readout, not in the pixels.
//
// Pure CPU math over the buffer ReadFrameDump() already hands back: no GL, no shader, no depth
// texture readback, and therefore testable without a GPU.

// The raw-depth span actually present in a frame.
struct DepthStats {
    float minRaw = 0.0f;      // nearest raw depth found
    float maxRaw = 0.0f;      // farthest raw depth found
    bool hasRange = false;    // false when the frame carries no usable depth variation
};

// Scans a frame's depth plane for the span its pixels actually occupy. `count` is the texel
// count (width*height), not a byte count. A null or empty plane, a perfectly flat one (a dump
// taken on a menu screen, or one whose depth read came back cleared), and one whose span is too
// narrow to normalize against without amplifying quantization noise into garbage all come back
// with hasRange == false.
DepthStats ComputeDepthStats(const float* depth, size_t count);

// Raw hardware depth (0..1, non-linear) to eye-space distance in the game's world units, for a
// frame rendered with glFrustum(..., zNear, zFar). Returns 0 - never a plausible-looking number -
// when no usable projection was captured (zNear not positive, or zFar not beyond zNear), which is
// the case for a dump taken before the game first set up a 3D view. See projection_capture.h.
float LinearizeDepth(float rawDepth, float zNear, float zFar);

// Raw hardware depth to a 0..1 gray level, normalized across `stats`: 1.0 (white) at the frame's
// nearest depth, 0.0 (black) at its farthest. Depths outside the frame's own span clamp to those
// ends. When stats.hasRange is false there is nothing to normalize against, so every texel comes
// back as 0.5 - a flat mid-gray that reads as "this frame has no depth variation" rather than as
// a picture of one.
float DepthToGray(float rawDepth, const DepthStats& stats);

// Fills `outRgba` (count*4 bytes, caller-owned) with the grayscale depth image: R=G=B=the gray
// level, alpha opaque. Row order is whatever the caller's depth plane used - a frame dump stores
// OpenGL's row order (row 0 at the bottom), so this round-trips through glTexSubImage2D
// unflipped, the same as the dump's color plane. No-ops on a null pointer either side.
void RenderDepthToRgba(const float* depth, size_t count, const DepthStats& stats,
                        unsigned char* outRgba);
