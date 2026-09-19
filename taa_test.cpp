// Creates a real OpenGL context, clears the back buffer to a known color, captures it into a
// src RGBA16F texture once (mimicking what post_effects.cpp's shared pipeline now does once
// per frame), and calls ApplyTaa() four times in a row against that unchanging src texture -
// exercising the historyValid=0 passthrough path (call 1) and the ping-pong blend/clamp path
// across multiple successful frames (calls 2-4). Since the scene never changes, every call
// should still reproduce the clear color (the neighborhood clamp box degenerates to a single
// value when the frame is static, so blending never drifts) - this also functions as the
// ghosting-mitigation check: if the ping-pong role-flip or clamp logic were wrong, a static
// scene would still visibly drift or corrupt after a few frames, which this test would catch.
#include <windows.h>
#include <cmath>
#include <cstdio>

#include "gl_loader.h"
#include "projection_capture.h"
#include "taa.h"
#include "taa_jitter.h"

namespace {

const unsigned int GL_TEXTURE_2D         = 0x0DE1;
const unsigned int GL_RGBA16F            = 0x881A;
const unsigned int GL_TEXTURE_MIN_FILTER = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S     = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T     = 0x2803;
const unsigned int GL_LINEAR             = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE      = 0x812F;
const unsigned int GL_FRAMEBUFFER        = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER   = 0x8CA8;
const unsigned int GL_COLOR_ATTACHMENT0  = 0x8CE0;
const unsigned int GL_BACK               = 0x0405;
const unsigned int GL_NEAREST            = 0x2600;
const unsigned int GL_DEPTH_COMPONENT    = 0x1902;
const unsigned int GL_DEPTH_COMPONENT24  = 0x81A6;
const unsigned int GL_FLOAT              = 0x1406;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel, int callNum) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: call %d, %s channel = %d, expected ~%d (tolerance %d)\n", callNum, channel, actual, expected, tolerance);
        return false;
    }
    return true;
}

unsigned int CreatePipelineTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return tex;
}

void UploadRgba(const GlComputeApi& gl, unsigned int tex, int width, int height,
                 const unsigned char* pixels) {
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 0x1908 /* GL_RGBA */,
                        0x1401 /* GL_UNSIGNED_BYTE */, pixels);
}

// A busy (spatially high-contrast) checkerboard, so the neighborhood clamp's box is wide open
// at every cell boundary - the scenario where a scene-cut ghost can most easily hide, since a
// wide box accepts almost any stale value. `phaseShift` moves the cell boundaries sideways,
// so the same on-screen pixel can be assigned to a "light" cell in one call and a "dark" cell
// in another while both frames remain equally busy - simulating a scene cut, not just a
// brightness change, without needing real motion vectors to describe it.
void BuildChecker(unsigned char* pixels, int width, int height, int cellSize, int phaseShift,
                   unsigned char dark, unsigned char light) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            int cx = (x - phaseShift) / cellSize;
            if (x - phaseShift < 0) { cx -= 1; }  // floor division for negative x
            int cy = y / cellSize;
            unsigned char v = ((cx + cy) % 2 == 0) ? light : dark;
            pixels[i + 0] = v; pixels[i + 1] = v; pixels[i + 2] = v; pixels[i + 3] = 255;
        }
    }
}

// CreateDepthTexture and UploadDepth are copied verbatim from motion_blur_test.cpp. Motion blur
// and TAA's real path are the only two stages here that take both colour and depth, so sharing
// these would mean a test-support library with two callers - duplication is the cheaper of the
// two.
unsigned int CreateDepthTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, width, height);
    return tex;
}

void UploadDepth(const GlComputeApi& gl, unsigned int tex, int width, int height,
                 const float* depth) {
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
}

// --- Real-path fixtures. ------------------------------------------------------------------
//
// Shared shape: 64x64, a frustum of left/right -1..1 and bottom/top -0.75..0.75 with
// zNear=1, zFar=1000, and a depth buffer filled with 0.990991 - the value motion_blur_test.cpp
// established puts every pixel at an eye distance of roughly 100, far enough that a small
// camera rotation moves pixels by a useful number of texels without anything landing behind
// the near plane.
//
// These run after the TAA-lite blocks in main(), which leave a valid 64x64 history behind: the
// ping-pong pair is only reset when the size changes, and it does not change here. So no
// fixture below may assume its first call takes the historyValid=0 passthrough - each one
// primes history with its own content instead, and says how.

const int kRealW = 64;
const int kRealH = 64;

ProjectionParams RealProjection() {
    ProjectionParams p;
    p.left = -1.0f; p.right = 1.0f;
    p.bottom = -0.75f; p.top = 0.75f;
    p.zNear = 1.0f; p.zFar = 1000.0f;
    return p;
}

void FillDepth(float* depth, int count) {
    for (int i = 0; i < count; ++i) { depth[i] = 0.990991f; }
}

// A column-major yaw about the world Y axis, as a reprojection matrix. A pure rotation with no
// translation is what a camera turning in place produces, which is the case TAA has to survive.
void YawReprojection(float degrees, float out[16]) {
    const float r = degrees * 3.14159265358979f / 180.0f;
    const float c = cosf(r);
    const float s = sinf(r);
    for (int i = 0; i < 16; ++i) { out[i] = 0.0f; }
    out[0] = c;   out[8] = s;
    out[5] = 1.0f;
    out[2] = -s;  out[10] = c;
    out[15] = 1.0f;
}

// A smooth horizontal ramp - every column a different value, so a history lookup that lands on
// the wrong column is visible as a wrong value rather than hiding inside flat content.
void BuildRamp(unsigned char* pixels, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            unsigned char v = (unsigned char)(x * 255 / (width - 1));
            pixels[i + 0] = v; pixels[i + 1] = v; pixels[i + 2] = v; pixels[i + 3] = 255;
        }
    }
}

// Alternating full-contrast columns. The off-screen-history fixture below needs this rather
// than BuildRamp's gradient: the resolve clips history to one standard deviation of the current
// frame's 3x3 neighbourhood, and a gradient's neighbourhood is so uniform that the clip alone
// holds surviving history to about three 8-bit levels - measured at 3, against that fixture's
// tolerance of 3, so on a ramp it cannot tell the range check working from the range check
// missing and passes on the boundary. With neighbouring columns 0 and 255 the clip box is wide
// open, stale history shows up at full strength (measured 109), and the guard is covered.
void BuildStripes(unsigned char* pixels, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            unsigned char v = (x % 2 == 0) ? 0 : 255;
            pixels[i + 0] = v; pixels[i + 1] = v; pixels[i + 2] = v; pixels[i + 3] = 255;
        }
    }
}

unsigned char ReadR(const GlComputeApi& gl, unsigned int readFbo, unsigned int tex, int x, int y) {
    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    unsigned char px[4] = {0, 0, 0, 0};
    gl.glReadPixels(x, y, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, px);
    return px[0];
}

// A pixel the game drew an overlay on - worldTex and captureTex disagree there - must come out
// bit-identical to the current frame. This is the assertion that keeps subtitles from ghosting
// while the camera turns, and it uses a NON-identity reprojection on purpose: a version that
// quietly reprojected HUD pixels along with everything else would move this one.
bool CheckRealHudPixelsAreUntouched(const GlComputeApi& gl, unsigned int readFbo) {
    bool ok = true;
    const int n = kRealW * kRealH;

    unsigned int src = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int dst = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int world = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int capture = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int depth = CreateDepthTexture(gl, kRealW, kRealH);

    unsigned char* ramp = new unsigned char[(size_t)n * 4];
    BuildRamp(ramp, kRealW, kRealH);
    UploadRgba(gl, src, kRealW, kRealH, ramp);
    UploadRgba(gl, world, kRealW, kRealH, ramp);

    // capture == world everywhere EXCEPT the probe, where the game "drew" something.
    unsigned char* overlay = new unsigned char[(size_t)n * 4];
    for (int i = 0; i < n * 4; ++i) { overlay[i] = ramp[i]; }
    const int kHudX = 10, kHudY = 10;
    const size_t hudIdx = ((size_t)kHudY * kRealW + kHudX) * 4;
    overlay[hudIdx + 0] = 255; overlay[hudIdx + 1] = 0; overlay[hudIdx + 2] = 0;
    UploadRgba(gl, capture, kRealW, kRealH, overlay);

    float* depthPixels = new float[n];
    FillDepth(depthPixels, n);
    UploadDepth(gl, depth, kRealW, kRealH, depthPixels);

    const ProjectionParams proj = RealProjection();
    float yaw[16];
    YawReprojection(10.0f, yaw);

    // Two calls: the first pulls history toward this fixture's ramp, the second is the one
    // under test. The HUD assertion holds on either call - the HUD early-out is unconditional -
    // but the "something changed" guard below wants history that resembles this content.
    ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, proj, proj, yaw,
                 0.0f, 0.0f, 0.85f);
    bool wrote = ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, proj, proj,
                              yaw, 0.0f, 0.0f, 0.85f);
    ok = Check(wrote, "the real path reports that it wrote dst") && ok;

    const unsigned char hudOut = ReadR(gl, readFbo, dst, kHudX, kHudY);
    const unsigned char hudSrc = ramp[hudIdx + 0];
    ok = Check(hudOut == hudSrc, "a HUD pixel comes out bit-identical to the current frame") && ok;

    // Guard against a stage that did nothing at all and would pass the line above trivially:
    // somewhere in a yawed frame, at least one non-HUD pixel must differ from src.
    bool anyChanged = false;
    for (int x = 20; x < kRealW - 1 && !anyChanged; ++x) {
        const unsigned char out = ReadR(gl, readFbo, dst, x, 32);
        if (out != ramp[(((size_t)32 * kRealW) + x) * 4]) { anyChanged = true; }
    }
    ok = Check(anyChanged, "a yawed frame changes at least one non-HUD pixel") && ok;

    delete[] ramp; delete[] overlay; delete[] depthPixels;
    gl.glDeleteTextures(1, &src); gl.glDeleteTextures(1, &dst);
    gl.glDeleteTextures(1, &world); gl.glDeleteTextures(1, &capture);
    gl.glDeleteTextures(1, &depth);
    return ok;
}

// A stationary camera with identical frusta: history reprojects onto itself, so the blend has
// nothing to move toward and the frame must sit still. This is the case that catches a wrong
// frustum on either side of the lookup, because any mismatch shifts the history sample by a
// fraction of a texel and the ramp turns that straight into a value error.
bool CheckRealStationaryCameraIsStable(const GlComputeApi& gl, unsigned int readFbo) {
    bool ok = true;
    const int n = kRealW * kRealH;

    unsigned int src = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int dst = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int world = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int capture = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int depth = CreateDepthTexture(gl, kRealW, kRealH);

    unsigned char* ramp = new unsigned char[(size_t)n * 4];
    BuildRamp(ramp, kRealW, kRealH);
    UploadRgba(gl, src, kRealW, kRealH, ramp);
    UploadRgba(gl, world, kRealW, kRealH, ramp);
    UploadRgba(gl, capture, kRealW, kRealH, ramp);   // capture == world: no HUD anywhere

    float* depthPixels = new float[n];
    FillDepth(depthPixels, n);
    UploadDepth(gl, depth, kRealW, kRealH, depthPixels);

    const ProjectionParams proj = RealProjection();
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    for (int call = 0; call < 4; ++call) {
        ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, proj, proj, identity,
                     0.0f, 0.0f, 0.85f);
    }

    bool stable = true;
    for (int x = 4; x < kRealW - 4; ++x) {
        const unsigned char out = ReadR(gl, readFbo, dst, x, 32);
        const unsigned char expected = ramp[(((size_t)32 * kRealW) + x) * 4];
        if (out > expected + 2 || out + 2 < expected) { stable = false; }
    }
    ok = Check(stable, "a stationary camera leaves the frame stable across four calls") && ok;

    delete[] ramp; delete[] depthPixels;
    gl.glDeleteTextures(1, &src); gl.glDeleteTextures(1, &dst);
    gl.glDeleteTextures(1, &world); gl.glDeleteTextures(1, &capture);
    gl.glDeleteTextures(1, &depth);
    return ok;
}

// A large yaw sends the leading edge's history off the side of the previous frame. There is no
// history for those pixels - not history worth clamping - so they must take the current frame.
// Without the range check they would sample the clamped edge texel and drag it inward.
//
// The current frame is full-contrast stripes, not a ramp, so that the variance clip is not
// silently doing the range check's job - see BuildStripes.
bool CheckRealOffscreenHistoryIsRejected(const GlComputeApi& gl, unsigned int readFbo) {
    bool ok = true;
    const int n = kRealW * kRealH;

    unsigned int src = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int dst = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int world = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int capture = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int depth = CreateDepthTexture(gl, kRealW, kRealH);

    // Seed history with flat mid-grey, then switch src to the stripes. Any history that
    // survives is therefore visible as a pull toward 128.
    unsigned char* grey = new unsigned char[(size_t)n * 4];
    for (int i = 0; i < n; ++i) {
        grey[i * 4 + 0] = 128; grey[i * 4 + 1] = 128; grey[i * 4 + 2] = 128; grey[i * 4 + 3] = 255;
    }
    unsigned char* stripes = new unsigned char[(size_t)n * 4];
    BuildStripes(stripes, kRealW, kRealH);

    float* depthPixels = new float[n];
    FillDepth(depthPixels, n);
    UploadDepth(gl, depth, kRealW, kRealH, depthPixels);

    const ProjectionParams proj = RealProjection();
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float bigYaw[16];
    YawReprojection(45.0f, bigYaw);

    UploadRgba(gl, src, kRealW, kRealH, grey);
    UploadRgba(gl, world, kRealW, kRealH, grey);
    UploadRgba(gl, capture, kRealW, kRealH, grey);
    ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, proj, proj, identity,
                 0.0f, 0.0f, 0.85f);
    ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, proj, proj, identity,
                 0.0f, 0.0f, 0.85f);

    UploadRgba(gl, src, kRealW, kRealH, stripes);
    UploadRgba(gl, world, kRealW, kRealH, stripes);
    UploadRgba(gl, capture, kRealW, kRealH, stripes);
    ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, proj, proj, bigYaw,
                 0.0f, 0.0f, 0.85f);

    // Column 0 under a 45-degree yaw reprojects well outside the previous frame.
    const unsigned char out = ReadR(gl, readFbo, dst, 0, 32);
    const unsigned char expected = stripes[((size_t)32 * kRealW) * 4];
    printf("Column 0 after a 45-degree yaw: %d (current frame=%d; surviving grey history pulls toward 128)\n",
           out, expected);
    ok = Check(out <= expected + 3 && out + 3 >= expected,
               "history that falls off the previous frame is rejected, not clamped inward") && ok;

    delete[] grey; delete[] stripes; delete[] depthPixels;
    gl.glDeleteTextures(1, &src); gl.glDeleteTextures(1, &dst);
    gl.glDeleteTextures(1, &world); gl.glDeleteTextures(1, &capture);
    gl.glDeleteTextures(1, &depth);
    return ok;
}

// THE HEADLINE CLAIM: that jitter plus accumulation actually anti-aliases. Every other test
// here checks that the stage does not break something; this one checks that it does its job.
//
// A hard vertical edge is rendered once per frame at the frame's own sub-pixel offset, exactly
// as jittering the frustum would produce, and fed through the real path with the matching
// jittered frusta and a stationary camera. A converged TAA must land the edge texel near the
// average coverage - a genuine intermediate value - instead of the hard 0-or-255 a single
// un-jittered frame gives. With jitter forced to zero every frame is identical, the edge texel
// is 0 in all of them, and no intermediate value can appear; that is the mutation.
//
// kAaFrames is three full jitter periods rather than one for a reason that is easy to miss:
// this fixture inherits whatever history the fixture before it left at 64x64, and at blend=0.85
// that contamination only decays by 15% per call. One period of eight is not enough calls to
// drive it under the lower bound, so a zero-jitter build would still show a leftover
// intermediate value and the mutation would go unnoticed. Three periods leaves it at well under
// one 8-bit level while the jittered case has long since reached its steady cycle.
bool CheckRealJitterAntiAliases(const GlComputeApi& gl, unsigned int readFbo) {
    bool ok = true;
    const int n = kRealW * kRealH;
    const float kEdgeX = 32.25f;   // deliberately NOT on a texel boundary
    const int kAaFrames = 24;      // three full periods of TaaJitterOffset's 8-frame sequence

    unsigned int src = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int dst = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int world = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int capture = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int depth = CreateDepthTexture(gl, kRealW, kRealH);

    float* depthPixels = new float[n];
    FillDepth(depthPixels, n);
    UploadDepth(gl, depth, kRealW, kRealH, depthPixels);

    unsigned char* frame = new unsigned char[(size_t)n * 4];
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const ProjectionParams base = RealProjection();

    for (int call = 0; call < kAaFrames; ++call) {
        float jx = 0.0f, jy = 0.0f;
        TaaJitterOffset((unsigned int)call, jx, jy);

        // A hard edge, rasterised as the game would after the frustum shifted the image by -jx.
        for (int y = 0; y < kRealH; ++y) {
            for (int x = 0; x < kRealW; ++x) {
                size_t i = ((size_t)y * kRealW + x) * 4;
                unsigned char v = ((float)x + jx < kEdgeX) ? 0 : 255;
                frame[i + 0] = v; frame[i + 1] = v; frame[i + 2] = v; frame[i + 3] = 255;
            }
        }
        UploadRgba(gl, src, kRealW, kRealH, frame);
        UploadRgba(gl, world, kRealW, kRealH, frame);
        UploadRgba(gl, capture, kRealW, kRealH, frame);

        // The current frustum carries this frame's jitter; the previous stays unjittered,
        // exactly as post_effects.cpp reconstructs it. Getting these two the same way round is
        // the whole point of the fixture.
        double l = base.left, r = base.right, b = base.bottom, t = base.top;
        float dx = 0.0f, dy = 0.0f;
        JitterFrustumBounds(l, r, b, t, jx, jy, kRealW, kRealH, dx, dy);

        ProjectionParams cur = base;
        cur.left = (float)l; cur.right = (float)r;
        cur.bottom = (float)b; cur.top = (float)t;

        ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, cur, base, identity,
                     dx, dy, 0.85f);
    }

    // The texel straddling the edge must have converged to something between the two extremes.
    const unsigned char edgeTexel = ReadR(gl, readFbo, dst, 32, 32);
    printf("Edge texel after %d jittered frames: %d\n", kAaFrames, edgeTexel);
    ok = Check(edgeTexel > 20 && edgeTexel < 235,
               "jittered accumulation resolves the edge texel to an intermediate value") && ok;

    delete[] depthPixels; delete[] frame;
    gl.glDeleteTextures(1, &src); gl.glDeleteTextures(1, &dst);
    gl.glDeleteTextures(1, &world); gl.glDeleteTextures(1, &capture);
    gl.glDeleteTextures(1, &depth);
    return ok;
}

// A steep gradient - 16 levels per column across the left sixteen columns, flat 255 after.
// The alignment fixture below needs a much stronger local gradient than BuildRamp's four levels
// per column: what a mis-aimed history fetch costs is (displacement x gradient), and the
// variance clip caps the visible part of it at one standard deviation of the 3x3 neighbourhood,
// which is itself proportional to the gradient. So the gradient is the only lever that widens
// the gap between "aimed correctly" and "aimed one jitter away".
void BuildSteepRamp(unsigned char* pixels, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            int value = x * 16;
            if (value > 255) { value = 255; }
            unsigned char v = (unsigned char)value;
            pixels[i + 0] = v; pixels[i + 1] = v; pixels[i + 2] = v; pixels[i + 3] = 255;
        }
    }
}

// THE FETCH IS AIMED AT THE PIXEL GRID, NOT AT THE SCENE POINT. A jittered current frustum, an
// unjittered previous one, an identity reprojection and unchanging content: the camera has not
// moved, so every pixel must resolve against its OWN history and the frame must not drift at
// all. This is the one case the other four fixtures cannot see - three of them pass `proj, proj`
// so there is no jitter to mis-handle, and the anti-aliasing fixture's 20..235 band is orders of
// magnitude too wide for a quarter-pixel bias.
//
// Unprojecting with the jittered frustum and projecting into the unjittered one answers "where
// was this scene point last frame", which is one jitter away from "where was this pixel last
// frame". Leave that in and the fetch lands at uv + jx/width every single frame; the history
// recursion turns a constant sub-pixel offset into a standing displacement of blend/(1-blend)
// times it, bounded only by the variance clip.
//
// A CONSTANT offset is used rather than the Halton cycle on purpose: a cycling offset partly
// averages out, while a constant one drives the error to a steady value that either is or is
// not there. kAlignFrames is generous for the same reason the anti-aliasing fixture's count is -
// history inherited from the fixture before this one decays only 15% per call.
bool CheckRealJitteredFetchStaysOnTheGrid(const GlComputeApi& gl, unsigned int readFbo) {
    bool ok = true;
    const int n = kRealW * kRealH;
    const int kAlignFrames = 24;
    const int kProbeX = 8;          // inside the steep stretch, clear of both ends
    const int kProbeY = 32;
    const float kConstantJitterPx = 0.375f;   // a value the real Halton sequence does produce

    unsigned int src = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int dst = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int world = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int capture = CreatePipelineTexture(gl, kRealW, kRealH);
    unsigned int depth = CreateDepthTexture(gl, kRealW, kRealH);

    unsigned char* ramp = new unsigned char[(size_t)n * 4];
    BuildSteepRamp(ramp, kRealW, kRealH);
    UploadRgba(gl, src, kRealW, kRealH, ramp);
    UploadRgba(gl, world, kRealW, kRealH, ramp);
    UploadRgba(gl, capture, kRealW, kRealH, ramp);   // capture == world: no HUD anywhere

    float* depthPixels = new float[n];
    FillDepth(depthPixels, n);
    UploadDepth(gl, depth, kRealW, kRealH, depthPixels);

    const ProjectionParams base = RealProjection();
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    double l = base.left, r = base.right, b = base.bottom, t = base.top;
    float dx = 0.0f, dy = 0.0f;
    JitterFrustumBounds(l, r, b, t, kConstantJitterPx, 0.0f, kRealW, kRealH, dx, dy);
    ProjectionParams jittered = base;
    jittered.left = (float)l; jittered.right = (float)r;
    jittered.bottom = (float)b; jittered.top = (float)t;

    for (int call = 0; call < kAlignFrames; ++call) {
        ApplyTaaReal(src, dst, depth, world, capture, kRealW, kRealH, jittered, base, identity,
                     dx, dy, 0.85f);
    }

    const unsigned char out = ReadR(gl, readFbo, dst, kProbeX, kProbeY);
    const unsigned char expected = ramp[(((size_t)kProbeY * kRealW) + kProbeX) * 4];
    printf("Static camera under a constant %.3f px jitter: probe=%d, current frame=%d\n",
           kConstantJitterPx, out, expected);
    ok = Check(out <= expected + 1 && out + 1 >= expected,
               "a jittered frustum with a static camera still resolves onto the same pixel") && ok;

    delete[] ramp; delete[] depthPixels;
    gl.glDeleteTextures(1, &src); gl.glDeleteTextures(1, &dst);
    gl.glDeleteTextures(1, &world); gl.glDeleteTextures(1, &capture);
    gl.glDeleteTextures(1, &depth);
    return ok;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxTaaTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "taa_test", WS_OVERLAPPEDWINDOW,
        0, 0, 128, 128, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        printf("FAIL: ChoosePixelFormat/SetPixelFormat, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || !wglMakeCurrent(hdc, hglrc)) {
        printf("FAIL: wglCreateContext/wglMakeCurrent, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float, float, float, float);
    typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int);
    typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);
    PFNGLCLEARCOLORPROC pGlClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glClearColor");
    PFNGLCLEARPROC pGlClear = (PFNGLCLEARPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glClear");
    PFNGLVIEWPORTPROC pGlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glViewport");
    const unsigned int GL_COLOR_BUFFER_BIT = 0x00004000;
    pGlViewport(0, 0, 128, 128);
    pGlClearColor(0.8f, 0.2f, 0.1f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);

    bool ok = true;
    const GlComputeApi& gl = GetGlComputeApi();
    int width = 128, height = 128;

    unsigned int srcTex = CreatePipelineTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    unsigned int dstTex = CreatePipelineTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    for (int call = 1; call <= 4; ++call) {
        // shimmerSuppression=0.0 here - the existing history/clamp behavior this loop checks
        // must be unaffected when the feature is off.
        bool wrote = ApplyTaa(srcTex, dstTex, width, height, 0.85f, 0.0f);
        unsigned int err = gl.glGetError();
        if (!wrote || err != 0) {
            printf("FAIL: call %d left glGetError() = 0x%04X (wrote=%d)\n", call, err, wrote ? 1 : 0);
            ok = false;
            continue;
        }
        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after call %d: r=%d g=%d b=%d a=%d\n", call, pixel[0], pixel[1], pixel[2], pixel[3]);
        bool callOk = CheckClose(pixel[0], 204, 2, "r", call);
        callOk = CheckClose(pixel[1], 51, 2, "g", call) && callOk;
        callOk = CheckClose(pixel[2], 25, 2, "b", call) && callOk;
        if (callOk) {
            printf("PASS: call %d reproduced the cleared color\n", call);
        }
        ok = callOk && ok;
    }

    // shimmerSuppression=1.0 on a perfectly static scene: current and clamped history already
    // agree everywhere, so stillness=1.0 and the effective blend rises toward 0.95 - but since
    // there's no actual difference to blend between, the output must still reproduce the same
    // color, not drift toward some other value. This is what would catch a sign error or a
    // stillness computation that's inverted (kicking in on MOTION instead of stillness).
    {
        bool wrote = ApplyTaa(srcTex, dstTex, width, height, 0.85f, 1.0f);
        unsigned int err = gl.glGetError();
        if (!wrote || err != 0) {
            printf("FAIL: shimmerSuppression=1.0 call left glGetError() = 0x%04X (wrote=%d)\n", err, wrote ? 1 : 0);
            ok = false;
        } else {
            gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
            gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
            unsigned char pixel[4] = {0, 0, 0, 0};
            gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
            printf("Center pixel after shimmerSuppression=1.0 call: r=%d g=%d b=%d a=%d\n",
                   pixel[0], pixel[1], pixel[2], pixel[3]);
            bool callOk = CheckClose(pixel[0], 204, 2, "r", 5);
            callOk = CheckClose(pixel[1], 51, 2, "g", 5) && callOk;
            callOk = CheckClose(pixel[2], 25, 2, "b", 5) && callOk;
            if (callOk) {
                printf("PASS: shimmerSuppression=1.0 on a static scene still reproduced the cleared color\n");
            }
            ok = callOk && ok;
        }
    }

    // Fast scene change on BUSY content: this is the ghosting case a flat clear color can't
    // reproduce, because a flat frame makes the neighborhood clamp degenerate to a single
    // value (see this file's header comment) - trivially rejecting any stale history and
    // hiding a real bug. A checkerboard keeps the clamp box wide open at every cell boundary,
    // which is exactly the situation where stale history can numerically fit inside the new
    // frame's local range even though the on-screen content is completely different.
    {
        const int width2 = 64;
        const int height2 = 64;
        const int cellSize = 4;
        const unsigned char kDark = 40;
        const unsigned char kLight = 220;
        // Probe pixel (4,4) sits at a cell boundary - some of its 3x3 neighbors are dark, some
        // light, in EVERY scheme below, so the clamp box is wide there in every call.
        const int probeX = 4;
        const int probeY = 4;

        unsigned int srcTex2 = CreatePipelineTexture(gl, width2, height2);
        unsigned int dstTex2 = CreatePipelineTexture(gl, width2, height2);

        size_t texelCount = (size_t)width2 * height2;
        unsigned char* schemeA = new unsigned char[texelCount * 4];
        // phaseShift=0: probe (4,4) -> cx=1,cy=1 -> (1+1)%2==0 -> light (220).
        BuildChecker(schemeA, width2, height2, cellSize, 0, kDark, kLight);

        // Seed converged history on scheme A - two calls: the first takes the historyValid=0
        // passthrough path (this is a fresh width/height, so EnsureTextures reset it even
        // though earlier blocks in this file already ran TAA at a different size), the second
        // confirms the clamp settles since the content isn't changing.
        UploadRgba(gl, srcTex2, width2, height2, schemeA);
        ApplyTaa(srcTex2, dstTex2, width2, height2, 0.85f, 0.0f);
        ApplyTaa(srcTex2, dstTex2, width2, height2, 0.85f, 0.0f);

        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex2, 0);
        unsigned char seedPixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(probeX, probeY, 1, 1, 0x1908, 0x1401, seedPixel);
        printf("Probe pixel after seeding on scheme A: r=%d (expected ~%d)\n", seedPixel[0], kLight);
        bool seedOk = CheckClose(seedPixel[0], kLight, 4, "r", 6);
        ok = seedOk && ok;

        // The cut: phaseShift=cellSize flips the probe pixel's cell role to dark, while
        // keeping the frame just as busy everywhere. ONE call, simulating the very next frame
        // after a fast scene change.
        unsigned char* schemeB = new unsigned char[texelCount * 4];
        BuildChecker(schemeB, width2, height2, cellSize, cellSize, kDark, kLight);
        UploadRgba(gl, srcTex2, width2, height2, schemeB);
        bool wroteCut = ApplyTaa(srcTex2, dstTex2, width2, height2, 0.85f, 0.0f);
        unsigned int errCut = gl.glGetError();
        if (!wroteCut || errCut != 0) {
            printf("FAIL: scene-cut call left glGetError() = 0x%04X (wrote=%d)\n", errCut, wroteCut ? 1 : 0);
            ok = false;
        } else {
            gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex2, 0);
            unsigned char cutPixel[4] = {0, 0, 0, 0};
            gl.glReadPixels(probeX, probeY, 1, 1, 0x1908, 0x1401, cutPixel);
            printf("Probe pixel one frame after the cut: r=%d (new content=%d, stale ghost would be near %d)\n",
                   cutPixel[0], kDark, kLight);
            // A tolerance of 40 still comfortably separates "converged to new content" (~40)
            // from "ghosting" (the unrejected-history math above predicts ~193 for this exact
            // fixture at blend=0.85) - this is not a hair's-width tolerance tightening.
            bool cutOk = CheckClose(cutPixel[0], kDark, 40, "r", 7);
            if (cutOk) {
                printf("PASS: scene cut on busy content converged in one frame, no lingering ghost\n");
            }
            ok = cutOk && ok;
        }

        delete[] schemeA;
        delete[] schemeB;
        gl.glDeleteTextures(1, &srcTex2);
        gl.glDeleteTextures(1, &dstTex2);
    }

    // The real path (ApplyTaaReal). These run last, after the TAA-lite blocks above, and share
    // the one ping-pong history with them - see the fixtures' own note about what that means
    // for the first call of each.
    ok = CheckRealHudPixelsAreUntouched(gl, readFbo) && ok;
    ok = CheckRealStationaryCameraIsStable(gl, readFbo) && ok;
    ok = CheckRealOffscreenHistoryIsRejected(gl, readFbo) && ok;
    ok = CheckRealJitterAntiAliases(gl, readFbo) && ok;
    ok = CheckRealJitteredFetchStaysOnTheGrid(gl, readFbo) && ok;

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
