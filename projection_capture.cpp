// See projection_capture.h.
#include <cstdio>

#include "projection_capture.h"

namespace {

ProjectionParams g_captured;
bool g_hasCaptured = false;

// Whether CaptureProjectionFrustum() recorded anything since the last AdvanceProjectionHistory().
// Distinct from g_hasCaptured, which never goes back to false once the first frustum is seen.
// See AdvanceProjectionHistory() for why the difference matters.
bool g_capturedThisFrame = false;

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

    g_capturedThisFrame = true;

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
    // Only a frame that actually captured a frustum promotes one, so this history's promotion
    // rule reads the same as modelview_capture.cpp's (gated on g_latched) instead of on the
    // never-cleared g_hasCaptured.
    //
    // Be clear about what this does and does not buy, because it is easy to over-read: it is
    // behaviour-preserving today, and projection_capture_test.cpp says so where it pins it.
    // g_captured changes in exactly one place, CaptureProjectionFrustum(), which is also the
    // only thing that sets g_capturedThisFrame - so an ungated advance on a frame that captured
    // nothing could only re-copy the identical frustum it had already copied, and g_hasPrevious
    // latches identically either way. What the gate removes is a latent one: the moment anything
    // else writes or resets g_captured (a context loss, a per-level reset, a second recorder),
    // the ungated form would start stamping that value into the previous slot on frames that
    // drew no world pass, and this is the harder of the two bugs to see afterwards.
    //
    // It does NOT close the desync with taa_jitter.h, which is the third member of the set this
    // header says describes one frame - see AdvanceProjectionHistory()'s comment in the header
    // for what remains and why it cannot be fixed from here.
    if (g_capturedThisFrame) {
        g_previous = g_captured;
        g_hasPrevious = true;
    }
    g_capturedThisFrame = false;
}
