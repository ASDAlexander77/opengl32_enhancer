// See taa_jitter.h.
#include "taa_jitter.h"

// Only ApplyTaaJitterToFrustumFromCurrentViewport below touches either of these. Everything
// else in this file - the maths and the per-frame state - is pure and must stay that way, per
// taa_jitter.h's header comment.
#include "config.h"
#include "gl_loader.h"

namespace {

// Matches post_effects.cpp:64. Needed only by ApplyTaaJitterToFrustumFromCurrentViewport's
// glGetIntegerv call below.
const unsigned int GL_VIEWPORT = 0x0BA2;

// The radical inverse of `index` in `base` - the Halton sequence. Conventionally 1-indexed:
// Halton(0, b) is 0 for every base, which would make one frame of every cycle unjittered.
// Callers below pass index+1 for that reason.
float Halton(unsigned int index, unsigned int base) {
    float result = 0.0f;
    float f = 1.0f;
    unsigned int i = index;
    while (i > 0) {
        f /= (float)base;
        result += f * (float)(i % base);
        i /= base;
    }
    return result;
}

const unsigned int kJitterPeriod = 8;

// --- Per-frame state (see taa_jitter.h for what each of these means).

unsigned int g_frameIndex = 0;
bool g_armed = false;
bool g_realPathRan = false;

float g_appliedDx = 0.0f;
float g_appliedDy = 0.0f;
bool g_applied = false;

float g_prevDx = 0.0f;
float g_prevDy = 0.0f;
bool g_hasPrev = false;

}  // namespace

void TaaJitterOffset(unsigned int index, float& jx, float& jy) {
    const unsigned int n = (index % kJitterPeriod) + 1;
    jx = Halton(n, 2) - 0.5f;
    jy = Halton(n, 3) - 0.5f;
}

void JitterFrustumBounds(double& left, double& right, double& bottom, double& top,
                         float jx, float jy, int width, int height,
                         float& appliedDx, float& appliedDy) {
    if (width <= 0 || height <= 0) {
        appliedDx = 0.0f;
        appliedDy = 0.0f;
        return;
    }

    const double dx = (double)jx * (right - left) / (double)width;
    const double dy = (double)jy * (top - bottom) / (double)height;

    left += dx;
    right += dx;
    bottom += dy;
    top += dy;

    appliedDx = (float)dx;
    appliedDy = (float)dy;
}

void NotifyTaaRealPathRan(bool ran) {
    g_realPathRan = ran;
}

void AdvanceTaaJitter() {
    g_prevDx = g_appliedDx;
    g_prevDy = g_appliedDy;
    g_hasPrev = g_applied;

    g_applied = false;
    g_appliedDx = 0.0f;
    g_appliedDy = 0.0f;

    ++g_frameIndex;

    const AnaxConfig& config = GetAnaxConfig();
    g_armed = g_realPathRan && config.taaJitter && HasEffectStage(config, EffectKind::Taa);
    g_realPathRan = false;
}

void ApplyTaaJitterToFrustum(double& left, double& right, double& bottom, double& top,
                             int viewportWidth, int viewportHeight) {
    if (!g_armed) {
        return;
    }

    float jx = 0.0f, jy = 0.0f;
    TaaJitterOffset(g_frameIndex, jx, jy);

    float dx = 0.0f, dy = 0.0f;
    JitterFrustumBounds(left, right, bottom, top, jx, jy, viewportWidth, viewportHeight, dx, dy);

    // Latched, not accumulated: a second glFrustum in the same frame re-derives the identical
    // PIXEL offset from the identical frame index, so the world and a viewmodel are shifted by
    // the same visible amount. The frustum-unit numbers recorded here are not necessarily the
    // same, because dx = jx * (right - left) / width scales with the extent, and a viewmodel is
    // exactly the case that arrives with a different one. Last call of the frame wins, which is
    // what the resolve wants: post_effects.cpp pairs these with projection_capture.h's frustum,
    // and that keeps the most recent call too, so the pair always describes the same window.
    g_appliedDx = dx;
    g_appliedDy = dy;
    g_applied = (dx != 0.0f || dy != 0.0f);
}

void ApplyTaaJitterToFrustumFromCurrentViewport(double& left, double& right,
                                                double& bottom, double& top) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        return;
    }

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    ApplyTaaJitterToFrustum(left, right, bottom, top, viewport[2], viewport[3]);
}

bool GetTaaJitterApplied(float& dx, float& dy) {
    if (!g_applied) {
        return false;
    }
    dx = g_appliedDx;
    dy = g_appliedDy;
    return true;
}

bool GetPreviousTaaJitterApplied(float& dx, float& dy) {
    if (!g_hasPrev) {
        return false;
    }
    dx = g_prevDx;
    dy = g_prevDy;
    return true;
}
