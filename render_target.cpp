// See render_target.h.
#include "render_target.h"

#include <cmath>

#include "config.h"

namespace {

// The game's own full-frame viewport - the denominator of the scale factor, and the size the
// finished frame is downsampled back to on present.
int g_refWidth = 0;
int g_refHeight = 0;
bool g_haveRef = false;

// Whether the next NotifyGameViewport is the frame's first, and therefore the reference.
bool g_expectFirstViewport = false;

// The per-frame latch. Nothing reads config to decide whether to scale - everything reads this,
// so the viewport hook and the capture path cannot disagree within a frame.
bool g_armed = false;

// One edge, scaled. Both edges of a rectangle go through this, and the size is their difference,
// so adjacent rectangles abut exactly however fractional the factor is.
int ScaleEdge(int edge, int numerator, int denominator) {
    if (denominator <= 0) {
        return edge;
    }
    double scaled = ((double)edge * (double)numerator) / (double)denominator;
    return (int)lround(scaled);
}

}  // namespace

void NotifyFrameBoundary() {
    g_expectFirstViewport = true;
}

void NotifyGameViewport(int x, int y, int width, int height) {
    (void)x;
    (void)y;
    if (!g_expectFirstViewport) {
        return;
    }
    g_expectFirstViewport = false;
    if (width <= 0 || height <= 0) {
        return;
    }
    g_refWidth = width;
    g_refHeight = height;
    g_haveRef = true;
}

void ArmSupersampleForFrame(bool targetReady) {
    const AnaxConfig& config = GetAnaxConfig();
    // No `renderWidth > 0` check: g_haveRef implies the reference is positive, so the >=
    // comparisons below already imply it. A mutation test proved the explicit check
    // unreachable, which is the definition of code that is not doing anything.
    g_armed = targetReady &&
              g_haveRef &&
              config.renderWidth >= g_refWidth && config.renderHeight >= g_refHeight;
}

bool IsSupersampleActive() {
    return g_armed;
}

void ScaleGameRect(int& x, int& y, int& width, int& height) {
    if (!g_armed) {
        return;
    }
    const AnaxConfig& config = GetAnaxConfig();
    int left = ScaleEdge(x, config.renderWidth, g_refWidth);
    int right = ScaleEdge(x + width, config.renderWidth, g_refWidth);
    int bottom = ScaleEdge(y, config.renderHeight, g_refHeight);
    int top = ScaleEdge(y + height, config.renderHeight, g_refHeight);
    x = left;
    width = right - left;
    y = bottom;
    height = top - bottom;
}

void ResetRenderTargetState() {
    g_refWidth = 0;
    g_refHeight = 0;
    g_haveRef = false;
    g_expectFirstViewport = false;
    g_armed = false;
}
