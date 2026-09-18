// Creates a real OpenGL context, renders three horizontal bands at known eye-space depths under
// a real perspective (glFrustum) projection, captures color and depth off the real back buffer
// the same way post_effects.cpp's shared pipeline does, and checks ApplyFog()'s GPU pipeline.
//
// Three bands rather than two, because linear fog has three interesting cases and checking only
// the ends would pass for a step function: a band nearer than fogStart must be bit-exact, a band
// beyond fogEnd must be exactly the fog color, and a band halfway between must land halfway.
// That last one is what actually pins the ramp down.
//
// Geometry (128x128 viewport, glFrustum extent 1 at zNear=1, so eye z=-d spans +/-d):
//   rows   0..42   near band, eye z=-20   (nearer than fogStart=100 -> no fog)
//   rows  43..85   mid  band, eye z=-200  (halfway through the 100..300 ramp)
//   rows  86..127  far  band, eye z=-400  (beyond fogEnd=300 -> full fog)
#include <windows.h>
#include <cstdio>

#include "fog.h"
#include "gl_loader.h"
#include "projection_capture.h"

namespace {

const unsigned int GL_TEXTURE_2D           = 0x0DE1;
const unsigned int GL_RGBA16F              = 0x881A;
const unsigned int GL_DEPTH_COMPONENT24    = 0x81A6;
const unsigned int GL_TEXTURE_MIN_FILTER   = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER   = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S       = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T       = 0x2803;
const unsigned int GL_NEAREST              = 0x2600;
const unsigned int GL_CLAMP_TO_EDGE        = 0x812F;
const unsigned int GL_FRAMEBUFFER          = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER     = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER     = 0x8CA9;
const unsigned int GL_COLOR_ATTACHMENT0    = 0x8CE0;
const unsigned int GL_DEPTH_ATTACHMENT     = 0x8D00;
const unsigned int GL_BACK                 = 0x0405;
const unsigned int GL_DEPTH_BUFFER_BIT     = 0x00000100;
const unsigned int GL_COLOR_BUFFER_BIT     = 0x00004000;
const unsigned int GL_DEPTH_TEST           = 0x0B71;
const unsigned int GL_PROJECTION           = 0x1701;
const unsigned int GL_MODELVIEW            = 0x1700;
const unsigned int GL_QUADS                = 0x0007;

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

bool CheckClose(unsigned char actual, int expected, int tolerance, const char* what) {
    int diff = (int)actual - expected;
    if (diff < 0) {
        diff = -diff;
    }
    bool ok = diff <= tolerance;
    printf("%s: %s (got %d, expected %d +/- %d)\n", ok ? "PASS" : "FAIL", what,
           (int)actual, expected, tolerance);
    if (!ok) {
        ++g_failures;
    }
    return ok;
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

void ReadColorPixel(const GlComputeApi& gl, unsigned int readFbo, unsigned int tex,
                     int x, int y, unsigned char* outPixel) {
    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    gl.glReadPixels(x, y, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, outPixel);
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxFogTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int width = 128, height = 128;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "fog_test", WS_OVERLAPPEDWINDOW,
        0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, wc.hInstance, nullptr);
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

    HMODULE realGl = GetModuleHandleA("opengl32.dll");
    typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float, float, float, float);
    typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int);
    typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);
    typedef void (__stdcall *PFNGLENABLEPROC)(unsigned int);
    typedef void (__stdcall *PFNGLMATRIXMODEPROC)(unsigned int);
    typedef void (__stdcall *PFNGLLOADIDENTITYPROC)(void);
    typedef void (__stdcall *PFNGLFRUSTUMPROC)(double, double, double, double, double, double);
    typedef void (__stdcall *PFNGLBEGINPROC)(unsigned int);
    typedef void (__stdcall *PFNGLENDPROC)(void);
    typedef void (__stdcall *PFNGLVERTEX3FPROC)(float, float, float);
    typedef void (__stdcall *PFNGLCOLOR3FPROC)(float, float, float);
    auto pGlClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(realGl, "glClearColor");
    auto pGlClear = (PFNGLCLEARPROC)GetProcAddress(realGl, "glClear");
    auto pGlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(realGl, "glViewport");
    auto pGlEnable = (PFNGLENABLEPROC)GetProcAddress(realGl, "glEnable");
    auto pGlMatrixMode = (PFNGLMATRIXMODEPROC)GetProcAddress(realGl, "glMatrixMode");
    auto pGlLoadIdentity = (PFNGLLOADIDENTITYPROC)GetProcAddress(realGl, "glLoadIdentity");
    auto pGlFrustum = (PFNGLFRUSTUMPROC)GetProcAddress(realGl, "glFrustum");
    auto pGlBegin = (PFNGLBEGINPROC)GetProcAddress(realGl, "glBegin");
    auto pGlEnd = (PFNGLENDPROC)GetProcAddress(realGl, "glEnd");
    auto pGlVertex3f = (PFNGLVERTEX3FPROC)GetProcAddress(realGl, "glVertex3f");
    auto pGlColor3f = (PFNGLCOLOR3FPROC)GetProcAddress(realGl, "glColor3f");

    const double frustumExtent = 1.0, zNear = 1.0, zFar = 1000.0;

    pGlViewport(0, 0, width, height);
    pGlEnable(GL_DEPTH_TEST);
    pGlMatrixMode(GL_PROJECTION);
    pGlLoadIdentity();
    pGlFrustum(-frustumExtent, frustumExtent, -frustumExtent, frustumExtent, zNear, zFar);
    pGlMatrixMode(GL_MODELVIEW);
    pGlLoadIdentity();

    pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // All three bands are the same grey, so any difference in the result is fog and nothing else.
    // At eye z=-d the visible half-extent is d, so these y ranges cut the screen into thirds.
    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);   // near band, rows 0..42
    pGlVertex3f(-20.0f, -20.0f, -20.0f);
    pGlVertex3f( 20.0f, -20.0f, -20.0f);
    pGlVertex3f( 20.0f, -6.667f, -20.0f);
    pGlVertex3f(-20.0f, -6.667f, -20.0f);
    pGlEnd();
    pGlBegin(GL_QUADS);   // mid band, rows 43..85
    pGlVertex3f(-200.0f, -66.667f, -200.0f);
    pGlVertex3f( 200.0f, -66.667f, -200.0f);
    pGlVertex3f( 200.0f,  66.667f, -200.0f);
    pGlVertex3f(-200.0f,  66.667f, -200.0f);
    pGlEnd();
    pGlBegin(GL_QUADS);   // far band, rows 86..127
    pGlVertex3f(-400.0f, 133.333f, -400.0f);
    pGlVertex3f( 400.0f, 133.333f, -400.0f);
    pGlVertex3f( 400.0f, 400.0f, -400.0f);
    pGlVertex3f(-400.0f, 400.0f, -400.0f);
    pGlEnd();

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        printf("FAIL: no GL 4.3 compute support on this context/driver.\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        return 1;
    }

    unsigned int srcTex = CreateColorTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, srcTex);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    unsigned int depthTex = CreateDepthTexture(gl, width, height);
    unsigned int depthFbo = 0;
    gl.glGenFramebuffers(1, &depthFbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, depthFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthFbo);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    Check(gl.glGetError() == 0, "capturing color and depth leaves no GL error");

    unsigned int dstTex = CreateColorTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    ProjectionParams projection;
    projection.left = (float)-frustumExtent;
    projection.right = (float)frustumExtent;
    projection.bottom = (float)-frustumExtent;
    projection.top = (float)frustumExtent;
    projection.zNear = (float)zNear;
    projection.zFar = (float)zFar;

    const int kNearY = 20, kMidY = 64, kFarY = 107;
    const int kX = 64;
    const unsigned char kSource = 204;   // 0.8 * 255

    // Pure red fog: nothing in the scene is red, so every channel independently shows how much
    // fog was applied - green and blue go DOWN toward 0 while red goes UP toward 255. A grey fog
    // color could hide a bug that scaled all channels equally.
    const float kFogR = 1.0f, kFogG = 0.0f, kFogB = 0.0f;
    const float kFogStart = 100.0f, kFogEnd = 300.0f;

    unsigned char px[4] = {0, 0, 0, 0};

    // Sanity: all three bands really are the same grey before fog runs.
    ReadColorPixel(gl, readFbo, srcTex, kX, kNearY, px);
    CheckClose(px[0], kSource, 0, "source: near band");
    ReadColorPixel(gl, readFbo, srcTex, kX, kMidY, px);
    CheckClose(px[0], kSource, 0, "source: mid band");
    ReadColorPixel(gl, readFbo, srcTex, kX, kFarY, px);
    CheckClose(px[0], kSource, 0, "source: far band");

    // intensity=0 is an exact no-op at every depth.
    {
        bool wrote = ApplyFog(srcTex, dstTex, depthTex, width, height, projection,
                              kFogStart, kFogEnd, 0.0f, kFogR, kFogG, kFogB);
        Check(wrote && gl.glGetError() == 0, "ApplyFog(intensity=0) runs without a GL error");
        ReadColorPixel(gl, readFbo, dstTex, kX, kFarY, px);
        CheckClose(px[0], kSource, 0, "intensity=0 leaves even the farthest band exactly alone");
        CheckClose(px[1], kSource, 0, "intensity=0 leaves green untouched");
    }

    // The real ramp. near (20 units) is inside fogStart so it must be bit-exact; far (400) is
    // past fogEnd so it must be exactly the fog color; mid (200) sits exactly halfway through
    // the 100..300 ramp, so it must land halfway between the two.
    {
        bool wrote = ApplyFog(srcTex, dstTex, depthTex, width, height, projection,
                              kFogStart, kFogEnd, 1.0f, kFogR, kFogG, kFogB);
        Check(wrote && gl.glGetError() == 0, "ApplyFog(intensity=1) runs without a GL error");

        ReadColorPixel(gl, readFbo, dstTex, kX, kNearY, px);
        CheckClose(px[0], kSource, 0, "a band nearer than fogStart is bit-exact (red)");
        CheckClose(px[1], kSource, 0, "a band nearer than fogStart is bit-exact (green)");

        ReadColorPixel(gl, readFbo, dstTex, kX, kFarY, px);
        CheckClose(px[0], 255, 2, "a band beyond fogEnd becomes the fog color (red)");
        CheckClose(px[1], 0, 2, "a band beyond fogEnd becomes the fog color (green)");
        CheckClose(px[2], 0, 2, "a band beyond fogEnd becomes the fog color (blue)");

        ReadColorPixel(gl, readFbo, dstTex, kX, kMidY, px);
        // mix(204, 255, 0.5) = 229.5 ; mix(204, 0, 0.5) = 102
        CheckClose(px[0], 230, 3, "a band halfway along the ramp is halfway fogged (red)");
        CheckClose(px[1], 102, 3, "a band halfway along the ramp is halfway fogged (green)");
    }

    // intensity scales the whole ramp: at 0.5, the fully-fogged far band should sit halfway
    // between the source and the fog color rather than reaching it.
    {
        bool wrote = ApplyFog(srcTex, dstTex, depthTex, width, height, projection,
                              kFogStart, kFogEnd, 0.5f, kFogR, kFogG, kFogB);
        Check(wrote && gl.glGetError() == 0, "ApplyFog(intensity=0.5) runs without a GL error");
        ReadColorPixel(gl, readFbo, dstTex, kX, kFarY, px);
        CheckClose(px[0], 230, 3, "intensity=0.5 halves the fog on the farthest band (red)");
        CheckClose(px[1], 102, 3, "intensity=0.5 halves the fog on the farthest band (green)");
    }

    // No captured projection: distances are in world units, so without a frustum there is
    // nothing to compare against fogStart/fogEnd. Must no-op, like ssao and dof do.
    {
        ProjectionParams empty;
        bool wrote = ApplyFog(srcTex, dstTex, depthTex, width, height, empty,
                              kFogStart, kFogEnd, 1.0f, kFogR, kFogG, kFogB);
        Check(!wrote, "ApplyFog no-ops when no projection was captured");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
