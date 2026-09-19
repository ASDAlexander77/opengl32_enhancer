// CPU-only checks of taa_jitter.h's pure maths - the Halton sequence and the frustum shift.
// No GL context is created here, so this target is deliberately absent from CMakeLists.txt's
// "gpu"-labelled list.
#include <cmath>
#include <cstdio>

#include "taa_jitter.h"

namespace {

int g_failures = 0;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
    return ok;
}

bool NearlyEqual(double a, double b, double tol) { return fabs(a - b) < tol; }

// The pixel an eye-space point lands on under glFrustum's own projection. Written out here
// rather than imported so the test states the ground truth independently of the code it is
// checking: x_ndc = 2n*xe / ((r-l)*(-ze)) - (r+l)/(r-l), then NDC to pixels.
double ProjectToPixelX(double left, double right, double zNear, double xEye, double zEye,
                       int width) {
    double xNdc = (2.0 * zNear * xEye) / ((right - left) * (-zEye)) - (right + left) / (right - left);
    return (xNdc + 1.0) * 0.5 * (double)width;
}

bool CheckHaltonSequence() {
    bool ok = true;

    // Exact values, not just "in range". Halton(1,2)=0.5 and Halton(1,3)=1/3, shifted by -0.5
    // to centre the offsets on the pixel. Pinning the first two entries is what makes the
    // "return a constant" mutation fail loudly rather than drifting.
    {
        float jx = 0.0f, jy = 0.0f;
        TaaJitterOffset(0, jx, jy);
        ok = Check(NearlyEqual(jx, 0.0, 1e-6) && NearlyEqual(jy, 1.0 / 3.0 - 0.5, 1e-6),
                   "index 0 is Halton(1,2)-0.5 and Halton(1,3)-0.5") && ok;

        TaaJitterOffset(1, jx, jy);
        ok = Check(NearlyEqual(jx, 0.25 - 0.5, 1e-6) && NearlyEqual(jy, 2.0 / 3.0 - 0.5, 1e-6),
                   "index 1 is Halton(2,2)-0.5 and Halton(2,3)-0.5") && ok;
    }

    // All eight distinct, all inside the pixel, and roughly centred. Distinctness is the
    // property that matters: an 8-cycle of identical offsets would anti-alias nothing.
    {
        float xs[8] = {0};
        float ys[8] = {0};
        for (unsigned int i = 0; i < 8; ++i) {
            TaaJitterOffset(i, xs[i], ys[i]);
        }

        bool allDistinct = true;
        for (int i = 0; i < 8; ++i) {
            for (int j = i + 1; j < 8; ++j) {
                if (NearlyEqual(xs[i], xs[j], 1e-6) && NearlyEqual(ys[i], ys[j], 1e-6)) {
                    allDistinct = false;
                }
            }
        }
        ok = Check(allDistinct, "the eight offsets in one cycle are all distinct") && ok;

        bool inRange = true;
        double sumX = 0.0, sumY = 0.0;
        for (int i = 0; i < 8; ++i) {
            if (xs[i] < -0.5f || xs[i] > 0.5f || ys[i] < -0.5f || ys[i] > 0.5f) {
                inRange = false;
            }
            sumX += xs[i];
            sumY += ys[i];
        }
        ok = Check(inRange, "every offset lies within +/-0.5 of the pixel centre") && ok;
        ok = Check(fabs(sumX / 8.0) < 0.1 && fabs(sumY / 8.0) < 0.1,
                   "the cycle is roughly centred (mean within 0.1 px of zero)") && ok;
    }

    // The cycle is 8 long, so index 8 repeats index 0. This is what lets the caller hold a
    // free-running frame counter instead of wrapping it itself.
    {
        float x0 = 0.0f, y0 = 0.0f, x8 = 0.0f, y8 = 0.0f;
        TaaJitterOffset(0, x0, y0);
        TaaJitterOffset(8, x8, y8);
        ok = Check(NearlyEqual(x0, x8, 1e-6) && NearlyEqual(y0, y8, 1e-6),
                   "the sequence repeats with a period of 8") && ok;
    }

    return ok;
}

bool CheckFrustumShift() {
    bool ok = true;

    // A 90-degree-ish 4:3 frustum at 1600x1200, the size Anachronox runs at on the test rig.
    const double kLeft = -1.0, kRight = 1.0, kBottom = -0.75, kTop = 0.75;
    const double kNear = 1.0;
    const int kWidth = 1600, kHeight = 1200;

    // Half a pixel of jitter must move the frustum window by exactly half a pixel's worth of
    // its width - 0.5 * 2.0 / 1600. Dropping the division by width leaves this off by 1600x.
    {
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        float dx = 0.0f, dy = 0.0f;
        JitterFrustumBounds(l, r, b, t, 0.5f, 0.0f, kWidth, kHeight, dx, dy);

        const double expected = 0.5 * (kRight - kLeft) / (double)kWidth;  // 0.000625
        ok = Check(NearlyEqual(l, kLeft + expected, 1e-12) &&
                   NearlyEqual(r, kRight + expected, 1e-12),
                   "jx=0.5 shifts left and right by half a pixel of frustum width") && ok;
        ok = Check(NearlyEqual(b, kBottom, 1e-12) && NearlyEqual(t, kTop, 1e-12),
                   "a horizontal-only jitter leaves bottom and top untouched") && ok;
        ok = Check(NearlyEqual(dx, expected, 1e-12) && NearlyEqual(dy, 0.0, 1e-12),
                   "the applied offsets are reported in frustum units") && ok;
    }

    // The width of the window must not change - only its centre moves. A formula that added
    // the offset to one edge only would still pass the shift check above on `left`.
    {
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        float dx = 0.0f, dy = 0.0f;
        JitterFrustumBounds(l, r, b, t, -0.37f, 0.21f, kWidth, kHeight, dx, dy);
        ok = Check(NearlyEqual(r - l, kRight - kLeft, 1e-12) &&
                   NearlyEqual(t - b, kTop - kBottom, 1e-12),
                   "jittering translates the frustum window without resizing it") && ok;
    }

    // THE SIGN CONVENTION. Adding dx to both edges moves r+l by 2*dx, so x_ndc drops by
    // 2*jx/width - the rendered image translates by exactly -jx pixels. Everything downstream
    // consumes matrices rather than offsets, so this is the one place a sign can be wrong.
    {
        const double kXEye = 0.0, kZEye = -1.0;
        const double before = ProjectToPixelX(kLeft, kRight, kNear, kXEye, kZEye, kWidth);

        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        float dx = 0.0f, dy = 0.0f;
        const float kJx = 0.5f;
        JitterFrustumBounds(l, r, b, t, kJx, 0.0f, kWidth, kHeight, dx, dy);
        const double after = ProjectToPixelX(l, r, kNear, kXEye, kZEye, kWidth);

        ok = Check(NearlyEqual(after, before - (double)kJx, 1e-9),
                   "a jitter of +jx moves the rendered image by exactly -jx pixels") && ok;
    }

    // Reconstructing the original bounds by subtracting the reported offsets must be exact,
    // because the resolve rebuilds the unjittered previous frustum this way every frame.
    {
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        float dx = 0.0f, dy = 0.0f;
        JitterFrustumBounds(l, r, b, t, 0.42f, -0.31f, kWidth, kHeight, dx, dy);
        ok = Check(NearlyEqual(l - dx, kLeft, 1e-12) && NearlyEqual(r - dx, kRight, 1e-12) &&
                   NearlyEqual(b - dy, kBottom, 1e-12) && NearlyEqual(t - dy, kTop, 1e-12),
                   "subtracting the reported offsets recovers the original bounds exactly") && ok;
    }

    // A degenerate viewport must not divide by zero. The hook cannot assume glGetIntegerv
    // returned something sane on the very first frame.
    {
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        float dx = 1.0f, dy = 1.0f;
        JitterFrustumBounds(l, r, b, t, 0.5f, 0.5f, 0, 0, dx, dy);
        ok = Check(l == kLeft && r == kRight && b == kBottom && t == kTop &&
                   dx == 0.0f && dy == 0.0f,
                   "a zero-sized viewport leaves the frustum untouched and reports no offset") && ok;
    }

    return ok;
}

}  // namespace

int main() {
    CheckHaltonSequence();
    CheckFrustumShift();

    if (g_failures != 0) {
        printf("taa_jitter_test: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("taa_jitter_test: all checks passed\n");
    return 0;
}
