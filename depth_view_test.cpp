// See depth_view.h. Pure CPU math over a depth plane - no GL context and no GPU, so this is an
// unlabeled test that runs anywhere, CI included (see CMakeLists.txt's "gpu" label note).
//
// The numbers in kReal* below are not invented: they are the depth plane actually measured in a
// 1600x1200 Anachronox dump written by the F12 hotkey (frustum zNear=4, zFar=8192). That frame
// is the reason this module exists, so it is what the interesting cases are checked against.
#include <cstdio>

#include "depth_view.h"
#include "frame_dump.h"

namespace {

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

bool CheckClose(float actual, float expected, float tolerance, const char* what) {
    float diff = actual - expected;
    if (diff < 0.0f) {
        diff = -diff;
    }
    bool ok = diff <= tolerance;
    printf("%s: %s (got %.6f, expected %.6f +/- %.6f)\n", ok ? "PASS" : "FAIL", what,
           actual, expected, tolerance);
    if (!ok) {
        ++g_failures;
    }
    return ok;
}

// The real measured span of that Anachronox dump's depth plane, and the frustum it was rendered
// with. Raw hardware depth is hyperbolic, so a frame covering 51..1192 world units occupies only
// the top 7.5% of the 0..1 raw range - which is precisely why displaying raw depth directly
// shows a flat rectangle and why DepthToGray normalizes across the frame's own span.
const float kRealMin = 0.922031f;
const float kRealMax = 0.997131f;
const float kRealNear = 4.0f;
const float kRealFar = 8192.0f;

}  // namespace

int main() {
    // --- ComputeDepthStats -------------------------------------------------------------------
    {
        const float depth[] = {0.95f, 0.92f, 0.99f, 0.97f};
        DepthStats stats = ComputeDepthStats(depth, 4);
        Check(stats.hasRange, "ComputeDepthStats reports a range for a varying frame");
        CheckClose(stats.minRaw, 0.92f, 1e-6f, "ComputeDepthStats finds the nearest raw depth");
        CheckClose(stats.maxRaw, 0.99f, 1e-6f, "ComputeDepthStats finds the farthest raw depth");
    }

    {
        // A frame whose depth never varies carries no usable depth - a dump taken on a menu
        // screen, or one where the depth read came back cleared. It must be reported as such
        // rather than turned into a division by zero.
        const float depth[] = {1.0f, 1.0f, 1.0f};
        DepthStats stats = ComputeDepthStats(depth, 3);
        Check(!stats.hasRange, "ComputeDepthStats reports no range for a flat frame");
    }

    {
        DepthStats stats = ComputeDepthStats(nullptr, 0);
        Check(!stats.hasRange, "ComputeDepthStats handles an empty plane without crashing");
    }

    {
        // Not bit-identical, but close enough that normalizing across it would divide by very
        // nearly zero and amplify depth-buffer quantization noise into a garbage image. Treated
        // as "no range" for the same reason a perfectly flat frame is.
        const float depth[] = {0.5f, 0.5000001f};
        DepthStats stats = ComputeDepthStats(depth, 2);
        Check(!stats.hasRange, "ComputeDepthStats reports no range for a near-flat frame");
    }

    // --- LinearizeDepth ----------------------------------------------------------------------
    {
        CheckClose(LinearizeDepth(0.0f, kRealNear, kRealFar), kRealNear, 1e-3f,
                   "LinearizeDepth maps raw 0 to the near plane");
        CheckClose(LinearizeDepth(1.0f, kRealNear, kRealFar), kRealFar, 1e-1f,
                   "LinearizeDepth maps raw 1 to the far plane");
    }

    {
        // The whole point of linearizing: the arithmetic midpoint of the raw range is nowhere
        // near the midpoint of the world-space range. 51..1192 world units has a midpoint of
        // ~621, but raw 0.9596 is really only ~98 units out.
        float mid = (kRealMin + kRealMax) * 0.5f;
        CheckClose(LinearizeDepth(kRealMin, kRealNear, kRealFar), 51.0079f, 0.05f,
                   "LinearizeDepth turns the frame's nearest raw depth into world units");
        CheckClose(LinearizeDepth(kRealMax, kRealNear, kRealFar), 1191.9376f, 2.0f,
                   "LinearizeDepth turns the frame's farthest raw depth into world units");
        CheckClose(LinearizeDepth(mid, kRealNear, kRealFar), 97.8293f, 0.1f,
                   "LinearizeDepth is hyperbolic, not linear, across the frame's range");
    }

    {
        // No projection was captured (a dump taken before the game first drew a 3D view), or a
        // degenerate one. There is no world scale to report, so say so with 0 rather than
        // returning a plausible-looking number.
        CheckClose(LinearizeDepth(0.5f, 0.0f, 8192.0f), 0.0f, 1e-6f,
                   "LinearizeDepth returns 0 when zNear is not positive");
        CheckClose(LinearizeDepth(0.5f, 4.0f, 4.0f), 0.0f, 1e-6f,
                   "LinearizeDepth returns 0 when zFar does not exceed zNear");
    }

    // --- DepthToGray -------------------------------------------------------------------------
    {
        DepthStats stats;
        stats.minRaw = kRealMin;
        stats.maxRaw = kRealMax;
        stats.hasRange = true;

        CheckClose(DepthToGray(kRealMin, stats), 1.0f, 1e-5f,
                   "DepthToGray renders the nearest pixel white");
        CheckClose(DepthToGray(kRealMax, stats), 0.0f, 1e-5f,
                   "DepthToGray renders the farthest pixel black");
        CheckClose(DepthToGray((kRealMin + kRealMax) * 0.5f, stats), 0.5f, 1e-5f,
                   "DepthToGray puts the middle of the frame's range at mid-gray");
    }

    {
        // The defect this module exists to fix, stated as a test: across the real frame's span,
        // the displayed values must cover the full 0..1 range. Showing raw depth directly would
        // spread them over 0.0751 - a rectangle no eye can read as a depth map.
        DepthStats stats;
        stats.minRaw = kRealMin;
        stats.maxRaw = kRealMax;
        stats.hasRange = true;

        float spread = DepthToGray(kRealMin, stats) - DepthToGray(kRealMax, stats);
        Check(spread > 0.99f, "DepthToGray spreads a real frame's squashed range across full contrast");
    }

    {
        // Nothing to normalize against: a flat mid-gray reads as "this frame has no depth
        // variation", which is the honest answer, and cannot divide by zero.
        DepthStats stats;
        stats.minRaw = 1.0f;
        stats.maxRaw = 1.0f;
        stats.hasRange = false;
        CheckClose(DepthToGray(1.0f, stats), 0.5f, 1e-6f,
                   "DepthToGray returns flat mid-gray when the frame has no depth range");
    }

    {
        DepthStats stats;
        stats.minRaw = 0.4f;
        stats.maxRaw = 0.6f;
        stats.hasRange = true;
        CheckClose(DepthToGray(0.1f, stats), 1.0f, 1e-6f,
                   "DepthToGray clamps a depth nearer than the frame's own minimum");
        CheckClose(DepthToGray(0.9f, stats), 0.0f, 1e-6f,
                   "DepthToGray clamps a depth farther than the frame's own maximum");
    }

    // --- RenderDepthToRgba -------------------------------------------------------------------
    {
        DepthStats stats;
        stats.minRaw = 0.0f;
        stats.maxRaw = 1.0f;
        stats.hasRange = true;

        const float depth[] = {0.0f, 1.0f};
        unsigned char rgba[8] = {};
        RenderDepthToRgba(depth, 2, stats, rgba);

        Check(rgba[0] == 255 && rgba[1] == 255 && rgba[2] == 255,
              "RenderDepthToRgba writes the nearest texel as white");
        Check(rgba[4] == 0 && rgba[5] == 0 && rgba[6] == 0,
              "RenderDepthToRgba writes the farthest texel as black");
        Check(rgba[3] == 255 && rgba[7] == 255,
              "RenderDepthToRgba writes an opaque alpha");
    }

    // --- round trip through a real dump file -------------------------------------------------
    {
        // The path the user actually takes: F12 writes a dump, the editor reads it back and has
        // to turn its depth plane into a picture. Exercises WriteFrameDump/ReadFrameDump for
        // real (a temp file on disk), not a stand-in, since "the depth does not survive the
        // dump" is exactly the failure this whole feature exists to rule out.
        const int kWidth = 4;
        const int kHeight = 4;
        const int kTexels = kWidth * kHeight;

        unsigned char color[kTexels * 4];
        float depth[kTexels];
        for (int i = 0; i < kTexels; ++i) {
            color[i * 4 + 0] = 10;
            color[i * 4 + 1] = 20;
            color[i * 4 + 2] = 30;
            color[i * 4 + 3] = 255;
            // A ramp across the same squashed band a real frame occupies.
            depth[i] = 0.92f + 0.075f * ((float)i / (float)(kTexels - 1));
        }

        ProjectionParams projection;
        projection.left = -4.0f;
        projection.right = 4.0f;
        projection.bottom = -3.0f;
        projection.top = 3.0f;
        projection.zNear = kRealNear;
        projection.zFar = kRealFar;

        const char* path = "depth_view_test_roundtrip.dump";
        bool wrote = WriteFrameDump(path, kWidth, kHeight, true, projection, color, depth);
        Check(wrote, "WriteFrameDump writes a dump carrying a depth plane");

        FrameDumpHeader header{};
        unsigned char* readColor = nullptr;
        float* readDepth = nullptr;
        bool read = ReadFrameDump(path, header, &readColor, &readDepth);
        Check(read, "ReadFrameDump reads that dump back");

        if (read) {
            DepthStats stats = ComputeDepthStats(readDepth, (size_t)kTexels);
            Check(stats.hasRange, "a round-tripped dump still carries usable depth variation");
            CheckClose(stats.minRaw, 0.92f, 1e-5f, "the nearest depth survives the round trip");
            CheckClose(stats.maxRaw, 0.995f, 1e-5f, "the farthest depth survives the round trip");

            unsigned char rgba[kTexels * 4];
            RenderDepthToRgba(readDepth, (size_t)kTexels, stats, rgba);
            Check(rgba[0] == 255, "the round-tripped frame's nearest texel renders white");
            Check(rgba[(kTexels - 1) * 4] == 0, "the round-tripped frame's farthest texel renders black");

            // This ramp starts at raw 0.92, a shade nearer than the measured dump's 0.922031,
            // so it linearizes to 49.72 world units rather than that frame's 51.01.
            CheckClose(LinearizeDepth(stats.minRaw, header.projection.zNear, header.projection.zFar),
                       49.7208f, 0.05f, "the dump's own frustum linearizes its nearest depth");

            FreeFrameDump(readColor, readDepth);
        }

        remove(path);
    }

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
