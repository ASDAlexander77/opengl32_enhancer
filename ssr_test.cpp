// Creates a real OpenGL context, renders a real 3D scene under a perspective (glFrustum)
// projection, captures color and depth off the real back buffer the same way post_effects.cpp's
// shared pipeline does, and checks ApplySsr()'s GPU pipeline.
//
// The scene is built so a reflection is MEASURABLE rather than merely "something changed": a
// dark floor with a large bright wall standing on it. A ray leaving the floor bounces up and
// forward into that wall, so a floor pixel must get markedly brighter - and the wall itself,
// being vertical, must not change at all.
//
// The decisive check here is the up-facing gate, because that heuristic is the whole design risk
// of this stage (see ssr.h). It is tested by changing NOTHING but upThreshold: the same floor
// pixel that brightens at 0.7 must come back bit-exact at 1.0. A test that only showed "the
// floor brightened" would pass just as well if the gate were ignored entirely.
//
// Geometry (128x128 viewport, glFrustum extent 1 at zNear=1, so eye z=-d spans +/-d):
//   - floor, dark, the plane y=-20 running from eye z=-10 out to z=-600
//   - wall,  bright, the plane z=-150 spanning y=0..300, standing above the floor
// The floor therefore fills screen rows 0..~62 and the wall rows 64..127. A ray leaving the
// floor at row 30 (eye z=-38) travels about 128 world units before passing behind the wall.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "modelview_capture.h"
#include "projection_capture.h"
#include "ssr.h"

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
const unsigned int GL_MODELVIEW_MATRIX     = 0x0BA6;
const unsigned int GL_QUADS                = 0x0007;

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

bool CheckExact(unsigned char actual, unsigned char expected, const char* what) {
    bool ok = actual == expected;
    printf("%s: %s (got %d, expected exactly %d)\n", ok ? "PASS" : "FAIL", what, actual, expected);
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

unsigned char ReadRed(const GlComputeApi& gl, unsigned int readFbo, unsigned int tex, int x, int y) {
    unsigned char px[4] = {0, 0, 0, 0};
    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    gl.glReadPixels(x, y, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, px);
    return px[0];
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxSsrTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int width = 128, height = 128;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "ssr_test", WS_OVERLAPPEDWINDOW,
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
    typedef void (__stdcall *PFNGLROTATEFPROC)(float, float, float, float);
    typedef void (__stdcall *PFNGLGETFLOATVPROC)(unsigned int, float*);
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
    auto pGlRotatef = (PFNGLROTATEFPROC)GetProcAddress(realGl, "glRotatef");
    auto pGlGetFloatv = (PFNGLGETFLOATVPROC)GetProcAddress(realGl, "glGetFloatv");

    // These exact numbers go into ProjectionParams below, so the stage unprojects with precisely
    // the projection the scene was drawn under - the same discipline as ssao_test.cpp.
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

    // Dark floor: the horizontal plane y=-20, from just in front of the camera out to z=-600.
    // Horizontal in VIEW space, which is what the up-facing gate actually measures.
    pGlColor3f(0.1f, 0.1f, 0.1f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-600.0f, -20.0f,  -10.0f);
    pGlVertex3f( 600.0f, -20.0f,  -10.0f);
    pGlVertex3f( 600.0f, -20.0f, -600.0f);
    pGlVertex3f(-600.0f, -20.0f, -600.0f);
    pGlEnd();

    // Bright wall standing on the floor at z=-150, filling the top half of the screen. This is
    // the only bright thing in the scene, so any brightening of a floor pixel came from here.
    pGlColor3f(0.9f, 0.9f, 0.9f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-200.0f,   0.0f, -150.0f);
    pGlVertex3f( 200.0f,   0.0f, -150.0f);
    pGlVertex3f( 200.0f, 300.0f, -150.0f);
    pGlVertex3f(-200.0f, 300.0f, -150.0f);
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

    const int kFloorX = 64, kFloorY = 30;   // on the floor, ~38 world units out
    const int kWallX = 64, kWallY = 100;    // on the bright wall

    unsigned char floorSrc = ReadRed(gl, readFbo, srcTex, kFloorX, kFloorY);
    unsigned char wallSrc = ReadRed(gl, readFbo, srcTex, kWallX, kWallY);
    printf("source: floor = %d, wall = %d\n", (int)floorSrc, (int)wallSrc);
    Check(floorSrc > 0 && floorSrc < 60, "source: the floor sample is dark as drawn");
    Check(wallSrc > 200, "source: the wall sample is bright as drawn");

    const float kMaxDistance = 512.0f, kThickness = 16.0f, kUpThreshold = 0.7f;

    // intensity=0 is an exact no-op.
    {
        bool wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, projection, nullptr,
                              0.0f, kMaxDistance, kThickness, kUpThreshold);
        Check(wrote && gl.glGetError() == 0, "ApplySsr(intensity=0) runs without a GL error");
        CheckExact(ReadRed(gl, readFbo, dstTex, kFloorX, kFloorY), floorSrc,
                   "intensity=0 leaves the floor pixel bit-exact");
    }

    // The real pass: the floor must pick up the bright wall standing on it.
    unsigned char floorLit = 0;
    {
        bool wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, projection, nullptr,
                              1.0f, kMaxDistance, kThickness, kUpThreshold);
        Check(wrote && gl.glGetError() == 0, "ApplySsr(intensity=1) runs without a GL error");
        floorLit = ReadRed(gl, readFbo, dstTex, kFloorX, kFloorY);
        printf("floor with reflection = %d (source was %d)\n", (int)floorLit, (int)floorSrc);
        Check(floorLit > floorSrc + 40, "the floor reflects the bright wall above it");

        // A vertical surface is not something this stage claims to reflect, and the wall's own
        // reflection ray would leave the screen anyway - so it must come back untouched.
        CheckExact(ReadRed(gl, readFbo, dstTex, kWallX, kWallY), wallSrc,
                   "the vertical wall is left bit-exact");
    }

    // The gate, stated as a test. Identical call except upThreshold, which no floor normal can
    // exceed - so the same pixel that just brightened must come back exactly as it started. This
    // is what distinguishes "the gate decides what reflects" from "everything reflects".
    {
        bool wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, projection, nullptr,
                              1.0f, kMaxDistance, kThickness, 1.0f);
        Check(wrote, "ApplySsr(upThreshold=1) runs");
        CheckExact(ReadRed(gl, readFbo, dstTex, kFloorX, kFloorY), floorSrc,
                   "upThreshold=1 gates the floor off entirely");
    }

    // --- The gate reads WORLD up, not view-space up.
    //
    // Everything above measures the gate against view-space +Y, which is what this stage used to
    // do and what ssr.h documented as its worst limitation: pitch the camera and the gate swings
    // off true, because "up" was up from the camera's point of view rather than up in the world.
    //
    // The rendered image is held FIXED here and only the claimed camera changes. That isolates
    // the one thing this change is about - the gate now consults the camera - without the framing
    // shifts a re-render would bring. The floor in this image has a view-space normal of (0,1,0),
    // so: a camera that says world up IS view +Y must reflect it, and a camera pitched 45 degrees
    // (world up 45 degrees off view +Y, dot = 0.707) must gate it off at upThreshold=0.8.
    //
    // Pitch INVARIANCE - the same physical floor staying reflective as the camera tilts - is a
    // separate claim and needs a second render; it is tested further down.
    const float kWorldUpThreshold = 0.8f;
    {
        // Build both camera matrices with real GL rather than by hand, so the driver is the
        // oracle for the layout, exactly as modelview_capture_test.cpp does. Safe to run now:
        // colour and depth were already copied into textures above, so touching the matrix
        // stack can no longer affect what the stage reads.
        CameraMatrix levelCamera;
        CameraMatrix pitchedCamera;
        pGlMatrixMode(GL_MODELVIEW);
        pGlLoadIdentity();
        pGlGetFloatv(GL_MODELVIEW_MATRIX, levelCamera.m);
        pGlRotatef(45.0f, 1.0f, 0.0f, 0.0f);
        pGlGetFloatv(GL_MODELVIEW_MATRIX, pitchedCamera.m);
        pGlLoadIdentity();

        // World up is +Y in THIS scene: the test authors its geometry directly in view space
        // with a level camera, so world and view axes coincide. A Quake II game's world up is
        // +Z, which is why the axis is a parameter rather than a constant.
        float levelUp[3] = {0.0f, 0.0f, 0.0f};
        float pitchedUp[3] = {0.0f, 0.0f, 0.0f};
        SsrViewSpaceWorldUp(levelCamera, SsrWorldUpAxis::Y, levelUp);
        SsrViewSpaceWorldUp(pitchedCamera, SsrWorldUpAxis::Y, pitchedUp);
        printf("world up in view space: level = %.3f/%.3f/%.3f, pitched = %.3f/%.3f/%.3f\n",
               levelUp[0], levelUp[1], levelUp[2], pitchedUp[0], pitchedUp[1], pitchedUp[2]);
        Check(levelUp[1] > 0.999f, "SsrViewSpaceWorldUp: a level camera puts world up along view +Y");
        Check(pitchedUp[1] > 0.69f && pitchedUp[1] < 0.72f,
              "SsrViewSpaceWorldUp: a 45-degree pitch tilts world up off view +Y by cos(45)");

        // The axis reaches this function as an int from the config file, and it indexes a
        // 16-float array - so a value outside 0..2 would read past the end of the matrix. The
        // parser cannot currently produce one, which is exactly why this is worth pinning: the
        // function must be safe on its own terms rather than on its caller's good behaviour.
        float strayUp[3] = {9.0f, 9.0f, 9.0f};
        SsrViewSpaceWorldUp(levelCamera, (SsrWorldUpAxis)7, strayUp);
        Check(strayUp[0] == 0.0f && strayUp[1] == 0.0f && strayUp[2] == 1.0f,
              "SsrViewSpaceWorldUp: an out-of-range axis falls back to Z, reading nothing stray");

        bool wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, projection, levelUp,
                              1.0f, kMaxDistance, kThickness, kWorldUpThreshold);
        unsigned char floorLevel = ReadRed(gl, readFbo, dstTex, kFloorX, kFloorY);
        Check(wrote && floorLevel > floorSrc + 40,
              "world up along view +Y: the floor still reflects");

        // The decisive one. Same image, same threshold, same everything - only the camera says
        // the world is tilted relative to the view. With the old view-space gate this call is
        // indistinguishable from the one above and the floor reflects identically.
        wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, projection, pitchedUp,
                         1.0f, kMaxDistance, kThickness, kWorldUpThreshold);
        Check(wrote, "ApplySsr runs with a pitched camera");
        CheckExact(ReadRed(gl, readFbo, dstTex, kFloorX, kFloorY), floorSrc,
                   "a camera pitched 45 degrees gates this floor off: the gate reads the camera");

        // No camera captured at all. The stage must degrade to exactly what it did before this
        // change - not decline, and not guess - so a game whose view matrix is never captured
        // keeps the reflections it has today, bit for bit.
        wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, projection, nullptr,
                         1.0f, kMaxDistance, kThickness, kWorldUpThreshold);
        unsigned char floorNoCamera = ReadRed(gl, readFbo, dstTex, kFloorX, kFloorY);
        Check(wrote, "ApplySsr runs with no camera");
        CheckExact(floorNoCamera, floorLevel,
                   "no camera falls back to view-space up, matching the level camera bit-exactly");
    }

    // Without a captured projection raw depth cannot be unprojected at all, so the stage must
    // decline rather than reconstruct nonsense from a guessed near/far.
    {
        ProjectionParams empty;
        bool wrote = ApplySsr(srcTex, dstTex, depthTex, width, height, empty, nullptr,
                              1.0f, kMaxDistance, kThickness, kUpThreshold);
        Check(!wrote, "ApplySsr declines when no projection has been captured");
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
