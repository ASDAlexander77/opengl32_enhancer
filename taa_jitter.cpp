// See taa_jitter.h.
#include "taa_jitter.h"

namespace {

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
