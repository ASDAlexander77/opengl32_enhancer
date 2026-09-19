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

    // A frame with NO glFrustum call at all - a menu, a loading screen, a frame the engine
    // skipped. The three histories (projection, camera, jitter) are documented as one set
    // describing one frame, and the other two already sit such a frame out: modelview_capture
    // gates its promotion on g_latched, taa_jitter on g_applied, both of which reset every
    // advance. This pins that AdvanceProjectionHistory() does the same - it promotes only what
    // THIS frame captured - so the previous slot keeps meaning "the last frame that actually
    // drew a world pass" rather than being re-stamped on frames that drew none.
    //
    // HONEST LIMIT, and it is an unusual one: this case cannot fail against the ungated
    // `if (g_hasCaptured)` version it replaced, and the mutation check confirms it does not.
    // The two are value-equivalent by construction: g_captured only ever changes inside
    // CaptureProjectionFrustum(), which is also the only thing that sets the "captured this
    // frame" flag, so an ungated advance on an empty frame can only re-copy the identical
    // frustum it already copied. g_hasPrevious latches the same way in both. The gate is a
    // correctness-preserving clarification, and this case exists to pin the invariant the
    // header now states against the change that WOULD make it observable - anything that
    // resets, clears or otherwise writes g_captured outside a capture.
    AdvanceProjectionHistory();
    CaptureProjectionFrustum(-3.0, 3.0, -2.25, 2.25, 1.0, 4096.0);
    AdvanceProjectionHistory();   // the -3.0 frustum's own frame ends here: it IS promoted
    AdvanceProjectionHistory();   // an empty frame: nothing to promote, previous must stand
    {
        ProjectionParams prev;
        ProjectionParams cur;
        Check(GetPreviousProjection(prev) && prev.left == -3.0f,
              "an advance with no frustum captured this frame leaves the previous slot alone");
        Check(GetCapturedProjection(cur) && cur.left == -3.0f,
              "and leaves the most recent captured frustum readable, so stages still have one");
    }

    if (g_failures != 0) {
        printf("projection_capture_test: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("projection_capture_test: all checks passed\n");
    return 0;
}
