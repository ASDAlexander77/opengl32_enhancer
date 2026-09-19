// See projection_capture.h.
#include <cstdio>

#include "projection_capture.h"

namespace {

ProjectionParams g_captured;
bool g_hasCaptured = false;

ProjectionParams g_previous;
bool g_hasPrevious = false;

}  // namespace

void CaptureProjectionFrustum(double left, double right, double bottom, double top,
                               double zNear, double zFar) {
    if (zNear <= 0.0 || zFar <= zNear || right == left || top == bottom) {
        return;
    }

    g_captured.left = (float)left;
    g_captured.right = (float)right;
    g_captured.bottom = (float)bottom;
    g_captured.top = (float)top;
    g_captured.zNear = (float)zNear;
    g_captured.zFar = (float)zFar;

    if (!g_hasCaptured) {
        printf("[opengl32_enh_cpp] projection: captured first frustum (near=%.3f far=%.3f), "
               "depth-based stages can now unproject\n", g_captured.zNear, g_captured.zFar);
        g_hasCaptured = true;
    }
}

bool GetCapturedProjection(ProjectionParams& out) {
    if (!g_hasCaptured) {
        return false;
    }
    out = g_captured;
    return true;
}

bool GetPreviousProjection(ProjectionParams& out) {
    if (!g_hasPrevious) {
        return false;
    }
    out = g_previous;
    return true;
}

void AdvanceProjectionHistory() {
    if (g_hasCaptured) {
        g_previous = g_captured;
        g_hasPrevious = true;
    }
}
