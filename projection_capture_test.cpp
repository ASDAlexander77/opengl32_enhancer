// CPU-only checks of projection_capture.h's one-frame history. No GL context: this module reads
// nothing from the driver, it only records what the glFrustum wrapper hands it.
#include <cstdio>

#include "projection_capture.h"

namespace {

int g_failures = 0;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
    return ok;
}

}  // namespace

int main() {
    // Before anything is captured there is no history to report, and a consumer must be able to
    // tell that apart from a frustum that happens to be all zeroes.
    {
        ProjectionParams p;
        Check(!GetPreviousProjection(p),
              "no previous projection before any frame has been captured");
    }

    // One captured frame, then an advance: that frame becomes the previous one.
    CaptureProjectionFrustum(-1.0, 1.0, -0.75, 0.75, 1.0, 4096.0);
    AdvanceProjectionHistory();
    {
        ProjectionParams p;
        bool got = GetPreviousProjection(p);
        Check(got && p.left == -1.0f && p.right == 1.0f,
              "the first captured frustum becomes the previous one after an advance");
    }

    // A second, DIFFERENT frustum - a field-of-view change - must not overwrite the previous
    // one until the next advance. The resolve reads current and previous as a pair mid-frame.
    CaptureProjectionFrustum(-2.0, 2.0, -1.5, 1.5, 1.0, 4096.0);
    {
        ProjectionParams prev;
        ProjectionParams cur;
        GetPreviousProjection(prev);
        GetCapturedProjection(cur);
        Check(prev.left == -1.0f && cur.left == -2.0f,
              "capturing a new frustum does not disturb the previous one");
    }

    AdvanceProjectionHistory();
    {
        ProjectionParams p;
        GetPreviousProjection(p);
        Check(p.left == -2.0f,
              "the advance moves the newly captured frustum into the previous slot");
    }

    if (g_failures != 0) {
        printf("projection_capture_test: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("projection_capture_test: all checks passed\n");
    return 0;
}
