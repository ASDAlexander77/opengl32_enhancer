// CPU-only checks of taa_jitter.h's pure maths - the Halton sequence and the frustum shift.
// No GL context is created here, so this target is deliberately absent from CMakeLists.txt's
// "gpu"-labelled list.
#include <cmath>
#include <cstdio>

#include "config.h"
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
        // 1e-9 rather than the 1e-12 above because dx/dy are floats by design - see the
        // reconstruction block below for the full reasoning, which applies verbatim here.
        ok = Check(NearlyEqual(dx, expected, 1e-9) && NearlyEqual(dy, 0.0, 1e-9),
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

    // Reconstructing the original bounds by subtracting the reported offsets must be close,
    // because the resolve rebuilds the unjittered previous frustum this way every frame.
    // appliedDx/appliedDy are float BY DESIGN - they get subtracted from ProjectionParams,
    // whose fields are already float, so the reconstruction can only ever be exact to float
    // precision (~1.4e-11 here). Do not tighten this back to 1e-12: 1e-9 on a 2.0-wide frustum
    // is still under a millionth of a pixel at 1600 wide.
    {
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        float dx = 0.0f, dy = 0.0f;
        JitterFrustumBounds(l, r, b, t, 0.42f, -0.31f, kWidth, kHeight, dx, dy);
        ok = Check(NearlyEqual(l - dx, kLeft, 1e-9) && NearlyEqual(r - dx, kRight, 1e-9) &&
                   NearlyEqual(b - dy, kBottom, 1e-9) && NearlyEqual(t - dy, kTop, 1e-9),
                   "subtracting the reported offsets recovers the original bounds to float precision") && ok;
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

bool CheckArming() {
    bool ok = true;

    const double kLeft = -1.0, kRight = 1.0, kBottom = -0.75, kTop = 0.75;
    // Same 1600x1200 viewport as CheckFrustumShift, passed explicitly: ApplyTaaJitterToFrustum
    // is pure and takes the viewport as arguments rather than reading GL_VIEWPORT itself (see
    // taa_jitter.h) - that split is what lets this run with no GL context at all.
    const int kWidth = 1600, kHeight = 1200;

    // The arming rule also requires `taa` to be in the effect chain and taaJitter to be on (see
    // AdvanceTaaJitter in taa_jitter.cpp). A default-constructed AnaxConfig has stageCount == 0,
    // so without this every case below would be disarmed for a reason that has nothing to do
    // with the one it means to test. Set it once, for every case in this function, via
    // GetMutableAnaxConfig() (see config.h) - the same escape hatch world_capture_test.cpp and
    // window_override_test.cpp use to drive a config directly, with no ini file involved.
    AnaxConfig& config = GetMutableAnaxConfig();
    config.stageCount = 0;
    config.stages[config.stageCount++] = EffectKind::Taa;
    config.taaJitter = true;

    // Disarmed by default: nothing has told the module that a real TAA resolve is consuming
    // the jitter, so it must not touch the game's projection. Jitter with nothing resolving it
    // is strictly worse than no jitter - it is added shimmer and nothing else. The chain set up
    // above already satisfies the arming rule's other two conditions, so this case is disarmed
    // for exactly one reason: NotifyTaaRealPathRan(false).
    {
        NotifyTaaRealPathRan(false);
        AdvanceTaaJitter();
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        ApplyTaaJitterToFrustum(l, r, b, t, kWidth, kHeight);
        ok = Check(l == kLeft && r == kRight && b == kBottom && t == kTop,
                   "a disarmed frame leaves the frustum bit-identical") && ok;

        float dx = 1.0f, dy = 1.0f;
        ok = Check(!GetTaaJitterApplied(dx, dy),
                   "a disarmed frame reports no applied offset") && ok;
    }

    // Armed once the resolve reports that its real path ran. The signal is one frame lagged on
    // purpose, which is why the Advance call sits between the notify and the apply.
    {
        NotifyTaaRealPathRan(true);
        AdvanceTaaJitter();
        double l = kLeft, r = kRight, b = kBottom, t = kTop;
        ApplyTaaJitterToFrustum(l, r, b, t, kWidth, kHeight);
        ok = Check(l != kLeft || b != kBottom,
                   "an armed frame shifts the frustum") && ok;

        float dx = 0.0f, dy = 0.0f;
        ok = Check(GetTaaJitterApplied(dx, dy) && (dx != 0.0f || dy != 0.0f),
                   "an armed frame reports the offset it applied") && ok;
    }

    // The previous frame's offset must survive the advance, because the resolve rebuilds the
    // UNJITTERED previous frustum by subtracting exactly it. Reading the current offset there
    // instead would leave a permanent sub-pixel error in every history lookup.
    {
        NotifyTaaRealPathRan(true);
        AdvanceTaaJitter();
        double l1 = kLeft, r1 = kRight, b1 = kBottom, t1 = kTop;
        ApplyTaaJitterToFrustum(l1, r1, b1, t1, kWidth, kHeight);
        float firstDx = 0.0f, firstDy = 0.0f;
        GetTaaJitterApplied(firstDx, firstDy);

        NotifyTaaRealPathRan(true);
        AdvanceTaaJitter();
        double l2 = kLeft, r2 = kRight, b2 = kBottom, t2 = kTop;
        ApplyTaaJitterToFrustum(l2, r2, b2, t2, kWidth, kHeight);

        float prevDx = 0.0f, prevDy = 0.0f;
        ok = Check(GetPreviousTaaJitterApplied(prevDx, prevDy) &&
                   NearlyEqual(prevDx, firstDx, 1e-12) && NearlyEqual(prevDy, firstDy, 1e-12),
                   "the previous frame's offset is retained across an advance") && ok;

        float curDx = 0.0f, curDy = 0.0f;
        GetTaaJitterApplied(curDx, curDy);
        ok = Check(!NearlyEqual(curDx, prevDx, 1e-12) || !NearlyEqual(curDy, prevDy, 1e-12),
                   "consecutive armed frames use different offsets") && ok;
    }

    // Two glFrustum calls in one frame - a viewmodel drawn at a genuinely different field of
    // view - must be shifted by the same PIXEL offset. That, not an equal frustum-unit offset,
    // is the property that holds and the one that matters: the world and the viewmodel have to
    // land at the same sub-pixel position on screen, and the frustum-unit numbers cannot match
    // when the extents differ, because dx = jx * (right - left) / width scales with the extent.
    //
    // The second frustum is twice as wide and twice as tall, so an implementation that
    // re-derived the pixel offset per call (correct) and one that reused the first call's
    // frustum-unit shift (wrong - it would halve the viewmodel's apparent shift) differ by a
    // clean factor of two here.
    {
        NotifyTaaRealPathRan(true);
        AdvanceTaaJitter();

        double la = kLeft, ra = kRight, ba = kBottom, ta = kTop;
        ApplyTaaJitterToFrustum(la, ra, ba, ta, kWidth, kHeight);
        float dxA = 0.0f, dyA = 0.0f;
        GetTaaJitterApplied(dxA, dyA);

        // A wider frustum: the viewmodel's own field of view, same viewport.
        const double kWideLeft = 2.0 * kLeft, kWideRight = 2.0 * kRight;
        const double kWideBottom = 2.0 * kBottom, kWideTop = 2.0 * kTop;
        double lb = kWideLeft, rb = kWideRight, bb = kWideBottom, tb = kWideTop;
        ApplyTaaJitterToFrustum(lb, rb, bb, tb, kWidth, kHeight);
        float dxB = 0.0f, dyB = 0.0f;
        GetTaaJitterApplied(dxB, dyB);

        // Recover each call's pixel offset by undoing its own scaling. Tolerance as at :117 and
        // explained at :151-154 - the reported offsets are floats by design.
        const double pxA = (double)dxA * (double)kWidth / (kRight - kLeft);
        const double pyA = (double)dyA * (double)kHeight / (kTop - kBottom);
        const double pxB = (double)dxB * (double)kWidth / (kWideRight - kWideLeft);
        const double pyB = (double)dyB * (double)kHeight / (kWideTop - kWideBottom);
        ok = Check(NearlyEqual(pxA, pxB, 1e-9) && NearlyEqual(pyA, pyB, 1e-9),
                   "both glFrustum calls in one frame are shifted by the same pixel offset") && ok;

        // The bounds really were shifted, by each frustum's own scale - so the check above is
        // not passing on two offsets that are both zero. Tested on the pair rather than on x
        // alone: TaaJitterOffset's first sample of every cycle is jx=0 exactly (Halton(1,2) is
        // 0.5), and which sample this block lands on depends on how many advances the checks
        // above it happened to make. jy is never zero - Halton(n,3) cannot equal 0.5 - so the
        // magnitude of the pair always is not.
        const double shiftA = (la - kLeft) * (la - kLeft) + (ba - kBottom) * (ba - kBottom);
        ok = Check(shiftA > 0.0 &&
                   NearlyEqual(lb - kWideLeft, 2.0 * (la - kLeft), 1e-9) &&
                   NearlyEqual(bb - kWideBottom, 2.0 * (ba - kBottom), 1e-9),
                   "the wider frustum is shifted by twice the frustum-unit offset of the narrow "
                   "one, for the same pixel offset") && ok;

        // And the consequence for the resolve: the latched frustum-unit offset is the LAST
        // call's, which is the one that matches the frustum projection_capture.h also kept.
        const double latchedGap = ((double)dxB - (double)dxA) * ((double)dxB - (double)dxA) +
                                  ((double)dyB - (double)dyA) * ((double)dyB - (double)dyA);
        ok = Check(NearlyEqual(dxB, 2.0 * (double)dxA, 1e-9) &&
                   NearlyEqual(dyB, 2.0 * (double)dyA, 1e-9) && latchedGap > 0.0,
                   "the latched offset is the last call's, in that call's own frustum units") && ok;
    }

    return ok;
}

}  // namespace

int main() {
    CheckHaltonSequence();
    CheckFrustumShift();
    CheckArming();

    if (g_failures != 0) {
        printf("taa_jitter_test: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("taa_jitter_test: all checks passed\n");
    return 0;
}
