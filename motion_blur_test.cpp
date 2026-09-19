// Checks MotionBlurReprojection() - the pure CPU matrix math from motion_blur.h - against hand-
// worked cases. No GL context needed for these, so they run first in main() before any window is
// created. Below that, CheckGpuMotionBlur() creates a real GL context and checks ApplyMotionBlur()
// - the compute stage - against synthetic textures, following the same window/context/texture
// scaffolding as ssr_test.cpp (the nearest existing GPU test with this same color+depth-in,
// compute-shader, color-out shape).
#include <windows.h>
#include <cmath>
#include <cstdio>

#include "gl_loader.h"
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

// --- GPU cases for ApplyMotionBlur(), added once ApplyMotionBlur exists (Task 3). ---
namespace {

const unsigned int GL_TEXTURE_2D         = 0x0DE1;
const unsigned int GL_RGBA16F            = 0x881A;
const unsigned int GL_RGBA               = 0x1908;
const unsigned int GL_UNSIGNED_BYTE      = 0x1401;
const unsigned int GL_DEPTH_COMPONENT    = 0x1902;
const unsigned int GL_DEPTH_COMPONENT24  = 0x81A6;
const unsigned int GL_FLOAT              = 0x1406;
const unsigned int GL_TEXTURE_MIN_FILTER = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S     = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T     = 0x2803;
const unsigned int GL_NEAREST            = 0x2600;
const unsigned int GL_CLAMP_TO_EDGE      = 0x812F;
const unsigned int GL_FRAMEBUFFER        = 0x8D40;
const unsigned int GL_COLOR_ATTACHMENT0  = 0x8CE0;

const int kWidth = 128, kHeight = 128;

// HUD rectangle stand-in: x in [80,100), y in [54,74) - a strongly contrasting pure red against
// a mid-gray sinusoid background (see Background() below), so a leaked tap is unmistakable
// rather than merely "a bit brighter".
const int kHudX0 = 80, kHudX1 = 100, kHudY0 = 54, kHudY1 = 74;
// Just inside the rectangle's left edge (80), not its center. This matters: at the case3
// camera/strength/maxRadius below, a point near the CENTER of a 20px-wide rectangle has every
// one of its 7 taps land back inside the rectangle regardless of the coord-level "IsHud(coord)"
// check, since the per-tap "IsHud(tap)" check catches them all redundantly - so deleting the
// coord-level check would pass unnoticed. Close to the edge, the same camera motion walks
// several taps out into the background, so the coord-level check is the ONLY thing keeping this
// pixel bit-exact and its removal is actually exercised.
const int kHudX = 81, kHudY = 64;

const GlComputeApi* g_gl = nullptr;
unsigned int g_readFbo = 0;

// Matches the brief's test-case snippets, which call ReadPixel(tex, x, y, out) directly - the
// GlComputeApi reference and readback FBO are set once in CheckGpuMotionBlur() below.
void ReadPixel(unsigned int tex, int x, int y, unsigned char out[4]) {
    g_gl->glBindFramebuffer(GL_FRAMEBUFFER, g_readFbo);
    g_gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    g_gl->glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

unsigned int CreateColorTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return tex;
}

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

void UploadRgba(const GlComputeApi& gl, unsigned int tex, int width, int height,
                const unsigned char* pixels) {
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void UploadDepth(const GlComputeApi& gl, unsigned int tex, int width, int height,
                 const float* depth) {
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
}

// A busy, non-flat pattern - a flat field would give the blur nothing to average and would hide
// a broken velocity computation entirely (see memory: a flat test field let the NIS overshoot
// bug survive its own test).
unsigned char Background(int x, int y) {
    float v = 128.0f + 40.0f * sinf((float)x * 0.2f) + 20.0f * sinf((float)y * 0.37f);
    if (v < 0.0f) { v = 0.0f; }
    if (v > 255.0f) { v = 255.0f; }
    return (unsigned char)v;
}

void BuildWorldPixels(unsigned char* pixels) {
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            size_t i = ((size_t)y * kWidth + x) * 4;
            unsigned char v = Background(x, y);
            pixels[i + 0] = v; pixels[i + 1] = v; pixels[i + 2] = v; pixels[i + 3] = 255;
        }
    }
}

// captureTex (and srcTex, which matches it) is identical to worldTex everywhere EXCEPT inside
// the HUD rectangle, where it is overwritten with pure red - far outside the background's
// grayscale range (128 +/- 60), so |capture - world| clears the 1/128 threshold by a wide margin
// there and nowhere else.
void BuildCapturePixels(unsigned char* pixels) {
    BuildWorldPixels(pixels);
    for (int y = kHudY0; y < kHudY1; ++y) {
        for (int x = kHudX0; x < kHudX1; ++x) {
            size_t i = ((size_t)y * kWidth + x) * 4;
            pixels[i + 0] = 255; pixels[i + 1] = 0; pixels[i + 2] = 0; pixels[i + 3] = 255;
        }
    }
}

bool CheckGpuMotionBlur() {
    bool ok = true;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxMotionBlurTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return false;
    }

    RECT windowRect = {0, 0, kWidth, kHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "motion_blur_test", WS_OVERLAPPEDWINDOW,
        0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return false;
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
        return false;
    }
    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || !wglMakeCurrent(hdc, hglrc)) {
        printf("FAIL: wglCreateContext/wglMakeCurrent, GetLastError=%lu\n", GetLastError());
        return false;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        printf("FAIL: no GL 4.3 compute support on this context/driver.\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        return false;
    }
    g_gl = &gl;

    const int W = kWidth, H = kHeight;

    unsigned int src = CreateColorTexture(gl, W, H);
    unsigned int dst = CreateColorTexture(gl, W, H);
    unsigned int world = CreateColorTexture(gl, W, H);
    unsigned int capture = CreateColorTexture(gl, W, H);
    unsigned int depth = CreateDepthTexture(gl, W, H);

    static unsigned char worldPixels[kWidth * kHeight * 4];
    static unsigned char capturePixels[kWidth * kHeight * 4];
    static float depthPixels[kWidth * kHeight];
    BuildWorldPixels(worldPixels);
    BuildCapturePixels(capturePixels);
    // Raw depth chosen so LinearEyeDistance(raw, zNear=1, zFar=1000) comes out to roughly 100
    // world units - comfortably past the near plane, comfortably short of the far plane.
    for (int i = 0; i < kWidth * kHeight; ++i) { depthPixels[i] = 0.990991f; }

    UploadRgba(gl, src, W, H, capturePixels);
    UploadRgba(gl, capture, W, H, capturePixels);
    UploadRgba(gl, world, W, H, worldPixels);
    UploadDepth(gl, depth, W, H, depthPixels);

    g_readFbo = 0;
    gl.glGenFramebuffers(1, &g_readFbo);

    ok = Check(gl.glGetError() == 0, "uploading synthetic fixtures leaves no GL error") && ok;

    // Off-centre (left != -right, bottom != -top) so pixel (64,64) sits far enough from the
    // frustum's optical axis to pick up a measurable rotation: a symmetric frustum would put
    // (64,64) almost exactly ON the axis, where a small rotation about view Z produces
    // near-zero screen velocity and the "load bearing" case below would not actually load-bear.
    ProjectionParams projection;
    projection.left = -0.2f; projection.right = 1.0f;
    projection.bottom = -0.2f; projection.top = 1.0f;
    projection.zNear = 1.0f; projection.zFar = 1000.0f;

    // THE load-bearing case. "The frame got blurrier" would pass with the velocity math ignored
    // entirely, so this changes ONLY the previous camera between two runs and requires the same
    // pixel to go from bit-exact to changed - the same framing ssr_test.cpp uses for upThreshold.
    {
        CameraMatrix current;
        float still[16];
        MotionBlurReprojection(current, current, still);
        ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, still, 0.5f, 0.05f);
        unsigned char stillPx[4]; ReadPixel(dst, 64, 64, stillPx);

        CameraMatrix moved;            // previous camera rotated about Z
        moved.m[0] = 0.995f;  moved.m[1] = 0.0998f;
        moved.m[4] = -0.0998f; moved.m[5] = 0.995f;
        float rotated[16];
        MotionBlurReprojection(current, moved, rotated);
        ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, rotated, 0.5f, 0.05f);
        unsigned char movedPx[4]; ReadPixel(dst, 64, 64, movedPx);

        unsigned char srcPx[4]; ReadPixel(src, 64, 64, srcPx);
        ok = Check(stillPx[0] == srcPx[0] && stillPx[1] == srcPx[1] && stillPx[2] == srcPx[2],
                   "a stationary camera leaves the pixel bit-exact") && ok;
        ok = Check(movedPx[0] != srcPx[0] || movedPx[1] != srcPx[1] || movedPx[2] != srcPx[2],
                   "rotating ONLY the previous camera changes the same pixel") && ok;
    }

    // strength 0 is now a guard clause (see motion_blur.h/.cpp): the stage declines outright
    // rather than running a dispatch that merely happens to be bit-exact, saving a full compute
    // dispatch every frame for a user who has the stage listed but turned all the way down. So
    // this no longer reads dst back and compares it to src - with the guard in place dst is
    // never written at all, and a version that dropped the guard but still produced a bit-exact
    // result would pass a "dst == src" check right along with a version that has the guard,
    // which is exactly the gap a mutation check needs closed. Pre-fill dst with a sentinel
    // (same technique as the return-value-contract cases below) so "dst was left untouched" is
    // proven, not assumed from the bool alone.
    {
        const unsigned char kSentinel[4] = {17, 201, 88, 233};
        static unsigned char sentinelPixels[kWidth * kHeight * 4];
        for (int i = 0; i < kWidth * kHeight; ++i) {
            sentinelPixels[i * 4 + 0] = kSentinel[0];
            sentinelPixels[i * 4 + 1] = kSentinel[1];
            sentinelPixels[i * 4 + 2] = kSentinel[2];
            sentinelPixels[i * 4 + 3] = kSentinel[3];
        }
        UploadRgba(gl, dst, W, H, sentinelPixels);

        CameraMatrix current;
        CameraMatrix moved;
        moved.m[0] = 0.995f;  moved.m[1] = 0.0998f;
        moved.m[4] = -0.0998f; moved.m[5] = 0.995f;
        float rotated[16];
        MotionBlurReprojection(current, moved, rotated);
        bool wrote = ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, rotated,
                                      0.0f, 0.05f);
        unsigned char px[4]; ReadPixel(dst, 64, 64, px);
        ok = Check(!wrote, "strength 0 returns false") && ok;
        ok = Check(px[0] == kSentinel[0] && px[1] == kSentinel[1] && px[2] == kSentinel[2] &&
                   px[3] == kSentinel[3],
                   "strength 0 leaves dst untouched") && ok;
    }

    // A pixel inside the HUD rectangle is bit-exact even with the camera moving hard.
    {
        CameraMatrix current;
        CameraMatrix moved;
        moved.m[0] = 0.995f;  moved.m[1] = 0.0998f;
        moved.m[4] = -0.0998f; moved.m[5] = 0.995f;
        float rotated[16];
        MotionBlurReprojection(current, moved, rotated);
        ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, rotated, 1.0f, 0.2f);
        unsigned char px[4]; ReadPixel(dst, kHudX, kHudY, px);
        unsigned char srcPx[4]; ReadPixel(src, kHudX, kHudY, srcPx);
        ok = Check(px[0] == srcPx[0] && px[1] == srcPx[1] && px[2] == srcPx[2],
                   "a HUD pixel is returned bit-exact while the world blurs") && ok;
    }

    // Resolution for the mutation table's fourth row: deleting `if (IsHud(tap)) { continue; }`
    // fails none of the three cases above, because none of them puts a WORLD pixel close enough
    // to the HUD rectangle for a tap to actually land inside it - this closes that hole. A pure
    // sideways camera translation gives every pixel the SAME screen-space velocity (constant
    // depth means no parallax), so the tap sweep from a pixel just outside the HUD rectangle can
    // be placed precisely: several of its taps land inside the rectangle by construction.
    {
        CameraMatrix current;
        CameraMatrix moved;
        // Pure translation along view X. Reprojection = previous * inverse(current) = previous
        // itself since current is identity, and with tz=0 the reprojected view-space Z (hence
        // eye distance) is unchanged, so every pixel gets an IDENTICAL uv-space displacement - a
        // uniform pan, unlike the position-dependent rotation used above.
        moved.m[12] = 9.6f;
        float translated[16];
        MotionBlurReprojection(current, moved, translated);

        // kAdjX sits 6 pixels left of the HUD rectangle's left edge (kHudX0 = 80), same row as
        // its vertical center. At strength=1.0 the tap sweep (i = 1..7) lands roughly 1, 3, 4, 6,
        // 7, 9, 10 pixels to the right of this pixel, so taps i=4..7 land inside the rectangle
        // while i=1..3 stay short of it - confirmed against a standalone simulation of this exact
        // shader math before wiring up the GPU test. maxRadius=0.15 (19px) comfortably clears the
        // ~10px sweep without clamping it.
        const int kAdjX = kHudX0 - 6, kAdjY = kHudY;
        ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, translated, 1.0f, 0.15f);
        unsigned char adjPx[4]; ReadPixel(dst, kAdjX, kAdjY, adjPx);
        unsigned char adjSrcPx[4]; ReadPixel(src, kAdjX, kAdjY, adjSrcPx);
        printf("adjacent-to-HUD pixel (%d,%d): src = %d/%d/%d, blurred = %d/%d/%d\n",
               kAdjX, kAdjY, adjSrcPx[0], adjSrcPx[1], adjSrcPx[2],
               adjPx[0], adjPx[1], adjPx[2]);
        // The background is grayscale everywhere (R == G == B), so a leaked HUD tap is easy to
        // recognise: it pulls red up and green/blue down at the same time, splitting channels
        // that a correct blur - which only ever averages grayscale background samples - keeps
        // together.
        int redGreenGap = (int)adjPx[0] - (int)adjPx[1];
        if (redGreenGap < 0) { redGreenGap = -redGreenGap; }
        ok = Check(redGreenGap < 20,
                   "a world pixel adjacent to the HUD does not pick up a red/green split from "
                   "the HUD's colour") && ok;
        ok = Check(adjPx[1] > 90,
                   "a world pixel adjacent to the HUD does not have its green channel pulled "
                   "toward the HUD's zero") && ok;
    }

    // The one shader branch nothing above exercises: "if (before.z > -params.x) { ... return; }"
    // in kMotionBlurShaderSource, which fires when the current frame's depth-derived position
    // reprojects to somewhere behind the PREVIOUS frame's near plane - a previous camera that
    // has since translated far enough forward that it has passed the point entirely. Deleting
    // the guard would feed UvForViewPos() a positive before.z, dividing by a near-zero-or-flipped
    // "dist" and producing a velocity nothing like a real reprojection - so this proves the guard
    // actually fires and writes src through untouched, rather than merely trusting the shader
    // comment's claim about when it does.
    {
        // current is identity: the pixel's own view-space position is here.z == -eyeDist, about
        // -100 (see depthPixels above). previous.m[14] = 150 puts the reprojection's translation
        // component at +150 (current is identity, so reprojection == previous exactly), so
        // before.z = here.z + 150 == +50 - comfortably past the near plane (zNear = 1.0, guard
        // is before.z > -1.0) in the WRONG direction: the point is now behind the camera that
        // is supposed to be looking at it, exactly what "camera moved past this point" means.
        CameraMatrix current;
        CameraMatrix previous;
        previous.m[14] = 150.0f;
        float behindNear[16];
        MotionBlurReprojection(current, previous, behindNear);

        const int kBx = 64, kBy = 64;   // not in the HUD rectangle
        ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection, behindNear, 0.5f, 0.05f);
        unsigned char px[4]; ReadPixel(dst, kBx, kBy, px);
        unsigned char srcPx[4]; ReadPixel(src, kBx, kBy, srcPx);
        ok = Check(px[0] == srcPx[0] && px[1] == srcPx[1] && px[2] == srcPx[2],
                   "a pixel reprojecting behind the previous near plane is returned bit-exact") &&
             ok;
    }

    // ApplyMotionBlur's return-value contract - "returns true ONLY if it actually wrote
    // dstTexture" - had zero coverage above: every call so far discards the bool. Task 4 is
    // about to swap its ping-pong buffer on that return value, so a stage that declines (a guard
    // clause fires) but still reports true - or the reverse - would show garbage on screen. Each
    // false case pre-fills dst with a sentinel color unrelated to anything else in this test and
    // reads it back, so "dst was left untouched" is proven rather than assumed from the bool
    // alone.
    {
        const unsigned char kSentinel[4] = {17, 201, 88, 233};
        static unsigned char sentinelPixels[kWidth * kHeight * 4];
        for (int i = 0; i < kWidth * kHeight; ++i) {
            sentinelPixels[i * 4 + 0] = kSentinel[0];
            sentinelPixels[i * 4 + 1] = kSentinel[1];
            sentinelPixels[i * 4 + 2] = kSentinel[2];
            sentinelPixels[i * 4 + 3] = kSentinel[3];
        }

        CameraMatrix current;
        float identityReprojection[16];
        MotionBlurReprojection(current, current, identityReprojection);

        auto isSentinel = [&](const unsigned char px[4]) {
            return px[0] == kSentinel[0] && px[1] == kSentinel[1] &&
                   px[2] == kSentinel[2] && px[3] == kSentinel[3];
        };

        // depthTexture == 0.
        {
            UploadRgba(gl, dst, W, H, sentinelPixels);
            bool wrote = ApplyMotionBlur(src, dst, 0, world, capture, W, H, projection,
                                          identityReprojection, 0.5f, 0.05f);
            unsigned char px[4]; ReadPixel(dst, 64, 64, px);
            ok = Check(!wrote, "depthTexture == 0 returns false") && ok;
            ok = Check(isSentinel(px), "depthTexture == 0 leaves dst untouched") && ok;
        }

        // width <= 0.
        {
            UploadRgba(gl, dst, W, H, sentinelPixels);
            bool wrote = ApplyMotionBlur(src, dst, depth, world, capture, 0, H, projection,
                                          identityReprojection, 0.5f, 0.05f);
            unsigned char px[4]; ReadPixel(dst, 64, 64, px);
            ok = Check(!wrote, "width <= 0 returns false") && ok;
            ok = Check(isSentinel(px), "width <= 0 leaves dst untouched") && ok;
        }

        // Invalid projection (zNear <= 0).
        {
            UploadRgba(gl, dst, W, H, sentinelPixels);
            ProjectionParams badProjection = projection;
            badProjection.zNear = 0.0f;
            bool wrote = ApplyMotionBlur(src, dst, depth, world, capture, W, H, badProjection,
                                          identityReprojection, 0.5f, 0.05f);
            unsigned char px[4]; ReadPixel(dst, 64, 64, px);
            ok = Check(!wrote, "an invalid projection (zNear <= 0) returns false") && ok;
            ok = Check(isSentinel(px), "an invalid projection leaves dst untouched") && ok;
        }

        // A normal, fully-valid call returns true.
        {
            UploadRgba(gl, dst, W, H, sentinelPixels);
            bool wrote = ApplyMotionBlur(src, dst, depth, world, capture, W, H, projection,
                                          identityReprojection, 0.5f, 0.05f);
            ok = Check(wrote, "a normal, fully-valid call returns true") && ok;
        }
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok;
}

}  // namespace

int main() {
    bool ok = CheckReprojection();
    ok = CheckGpuMotionBlur() && ok;

    if (ok) {
        printf("ALL PASS\n");
    } else {
        printf("FAILURE(S)\n");
    }
    return ok ? 0 : 1;
}
