// Checks MotionBlurReprojection() - the pure CPU matrix math from motion_blur.h - against hand-
// worked cases. No GL context needed for these, so they run first in main() before any window is
// created; a later task adds GPU cases for ApplyMotionBlur() to this same file.
#include <cmath>
#include <cstdio>

#include "motion_blur.h"

namespace {

int g_failures = 0;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
    return ok;
}

bool NearlyEqual(float a, float b) { return fabsf(a - b) < 1e-4f; }

bool CheckReprojection() {
    bool ok = true;

    // Identity in, identity out. Weak on its own - an identity matrix is symmetric, so a
    // transposed inverse passes this case unchanged. That is exactly the trap the modelview
    // capture work hit, which is why the rotation case below is the load-bearing one.
    {
        CameraMatrix current;   // defaults to identity
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        bool isIdentity = true;
        for (int i = 0; i < 16; ++i) {
            float expected = (i % 5 == 0) ? 1.0f : 0.0f;
            if (!NearlyEqual(m[i], expected)) { isIdentity = false; }
        }
        ok = Check(isIdentity, "identical cameras reproject to the identity matrix") && ok;
    }

    // Pure translation. current translates the world by -d along view Z (the camera moved
    // forward by d); previous is identity. M = inverse(current), so it translates back by +d.
    {
        CameraMatrix current;
        current.m[14] = -40.0f;
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        ok = Check(NearlyEqual(m[14], 40.0f) && NearlyEqual(m[12], 0.0f) &&
                   NearlyEqual(m[13], 0.0f),
                   "a pure forward translation inverts to an equal backward one") && ok;
    }

    // Pure rotation - the case that catches a transposed inverse. current is +90 degrees about
    // Z (column-major: m[0]=cos, m[1]=sin, m[4]=-sin, m[5]=cos), previous is identity, so M is
    // -90 degrees about Z. Using R instead of R-transpose flips the sign of both off-diagonal
    // terms, which this catches and the identity case above cannot.
    {
        CameraMatrix current;
        current.m[0] = 0.0f;  current.m[1] = 1.0f;
        current.m[4] = -1.0f; current.m[5] = 0.0f;
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        ok = Check(NearlyEqual(m[0], 0.0f) && NearlyEqual(m[1], -1.0f) &&
                   NearlyEqual(m[4], 1.0f) && NearlyEqual(m[5], 0.0f),
                   "a +90 degree rotation inverts to -90 degrees, transpose included") && ok;
    }

    // Rotation AND translation together. The inverse translation is -R-transpose * t, so it
    // must be rotated too - a version that negated t without rotating it passes both cases
    // above and fails this one.
    {
        CameraMatrix current;
        current.m[0] = 0.0f;  current.m[1] = 1.0f;
        current.m[4] = -1.0f; current.m[5] = 0.0f;
        current.m[12] = 10.0f; current.m[13] = 0.0f; current.m[14] = 0.0f;
        CameraMatrix previous;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        // -R-transpose * (10,0,0) with R = Rz(90): R-transpose * (10,0,0) = (0,-10,0), negated
        // gives (0,10,0).
        ok = Check(NearlyEqual(m[12], 0.0f) && NearlyEqual(m[13], 10.0f) &&
                   NearlyEqual(m[14], 0.0f),
                   "the inverse translation is rotated, not merely negated") && ok;
    }

    // A non-identity `previous`. Added because the mutation table's third row - dropping the
    // `previous *` multiply entirely and returning inverse(current) unmodified - passes all four
    // cases above: every one of them uses an identity `previous`, under which
    // previous * inverse(current) == inverse(current) whether or not `previous` is actually
    // read. `previous` here translates by (0, 0, 5); if it were ignored, m[14] would come back
    // 40 instead of 45.
    {
        CameraMatrix current;
        current.m[14] = -40.0f;
        CameraMatrix previous;
        previous.m[14] = 5.0f;
        float m[16];
        MotionBlurReprojection(current, previous, m);
        ok = Check(NearlyEqual(m[12], 0.0f) && NearlyEqual(m[13], 0.0f) &&
                   NearlyEqual(m[14], 45.0f) &&
                   !NearlyEqual(m[14], 40.0f),
                   "a non-identity previous camera changes the result versus inverse(current) alone") && ok;
    }

    return ok;
}

}  // namespace

int main() {
    bool ok = CheckReprojection();

    if (ok) {
        printf("ALL PASS\n");
    } else {
        printf("FAILURE(S)\n");
    }
    return ok ? 0 : 1;
}
