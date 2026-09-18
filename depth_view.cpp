// See depth_view.h.
#include "depth_view.h"

namespace {

// Below this the frame is treated as carrying no depth variation. A span this narrow is either a
// genuinely flat frame or noise in the bottom bits of the depth buffer, and normalizing across it
// would multiply that noise up into a full-contrast garbage image.
const float kMinUsableSpan = 1e-6f;

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

}  // namespace

DepthStats ComputeDepthStats(const float* depth, size_t count) {
    DepthStats stats;
    if (depth == nullptr || count == 0) {
        return stats;
    }

    float minRaw = depth[0];
    float maxRaw = depth[0];
    for (size_t i = 1; i < count; ++i) {
        if (depth[i] < minRaw) {
            minRaw = depth[i];
        }
        if (depth[i] > maxRaw) {
            maxRaw = depth[i];
        }
    }

    stats.minRaw = minRaw;
    stats.maxRaw = maxRaw;
    stats.hasRange = (maxRaw - minRaw) > kMinUsableSpan;
    return stats;
}

float LinearizeDepth(float rawDepth, float zNear, float zFar) {
    if (zNear <= 0.0f || zFar <= zNear) {
        return 0.0f;
    }
    // Undo the perspective divide: window depth -> NDC -> eye-space distance. Clamping first
    // keeps a depth outside 0..1 (which nothing should produce, but a corrupt dump could) from
    // driving the denominator to zero or negative.
    float ndc = 2.0f * Clamp01(rawDepth) - 1.0f;
    float denominator = zFar + zNear - ndc * (zFar - zNear);
    if (denominator <= 0.0f) {
        return 0.0f;
    }
    return (2.0f * zNear * zFar) / denominator;
}

float DepthToGray(float rawDepth, const DepthStats& stats) {
    if (!stats.hasRange) {
        return 0.5f;
    }
    float t = (rawDepth - stats.minRaw) / (stats.maxRaw - stats.minRaw);
    return 1.0f - Clamp01(t);
}

void RenderDepthToRgba(const float* depth, size_t count, const DepthStats& stats,
                        unsigned char* outRgba) {
    if (depth == nullptr || outRgba == nullptr) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        float gray = DepthToGray(depth[i], stats);
        unsigned char level = (unsigned char)(gray * 255.0f + 0.5f);
        outRgba[i * 4 + 0] = level;
        outRgba[i * 4 + 1] = level;
        outRgba[i * 4 + 2] = level;
        outRgba[i * 4 + 3] = 255;
    }
}
