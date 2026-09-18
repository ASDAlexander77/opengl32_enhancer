// Creates a real OpenGL context, renders a real 3D scene under a perspective (glFrustum)
// projection, captures color and depth off the real back buffer the same way post_effects.cpp's
// shared pipeline does, and checks ApplyDof()'s GPU pipeline.
//
// The scene is built so blur is MEASURABLE rather than merely "something changed": a hard
// dark/bright vertical edge appears at the same screen column at two very different depths. A
// pixel beside an in-focus edge must keep the source value bit-exact; a pixel beside an
// out-of-focus edge must be pulled toward the average of the two sides. Checking a sharp edge
// and a blurred edge - not just one - is what makes this more than a smoke test, the same
// reasoning as vignette_test.cpp's header comment.
//
// Geometry (128x128 viewport, glFrustum extent 1 at zNear=1, so eye z=-d spans +/-d):
//   - far wall  at eye z=-400, full screen, dark left of x=0 and bright right of it
//   - near slab at eye z=-20, covering screen rows 0..80, split at x=0 the same way
// So screen column 64 carries the edge at both depths: rows below 80 see it at 20 world units,
// rows above 80 see it at 400. Screen centre (64,64) lands on the near slab, which is what makes
// the focusDistance=0 auto-focus case well defined.
#include <windows.h>
#include <cstdio>

#include "dof.h"
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
    wc.lpszClassName = "AnaxDofTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int width = 128, height = 128;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "dof_test", WS_OVERLAPPEDWINDOW,
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

    // These exact numbers go into ProjectionParams below, so the stage unprojects with precisely
    // the projection the scene was drawn under - the same discipline as ssao_test.cpp.
    const double frustumExtent = 1.0, zNear = 1.0, zFar = 1000.0;
    const float kNearDepth = 20.0f;   // world units to the near slab
    const float kFarDepth = 400.0f;   // world units to the far wall

    pGlViewport(0, 0, width, height);
    pGlEnable(GL_DEPTH_TEST);
    pGlMatrixMode(GL_PROJECTION);
    pGlLoadIdentity();
    pGlFrustum(-frustumExtent, frustumExtent, -frustumExtent, frustumExtent, zNear, zFar);
    pGlMatrixMode(GL_MODELVIEW);
    pGlLoadIdentity();

    pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Far wall, full screen: dark left of centre, bright right of it.
    pGlColor3f(0.2f, 0.2f, 0.2f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-400.0f, -400.0f, -400.0f);
    pGlVertex3f(   0.0f, -400.0f, -400.0f);
    pGlVertex3f(   0.0f,  400.0f, -400.0f);
    pGlVertex3f(-400.0f,  400.0f, -400.0f);
    pGlEnd();
    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(   0.0f, -400.0f, -400.0f);
    pGlVertex3f( 400.0f, -400.0f, -400.0f);
    pGlVertex3f( 400.0f,  400.0f, -400.0f);
    pGlVertex3f(   0.0f,  400.0f, -400.0f);
    pGlEnd();

    // Near slab over screen rows 0..80 (y from -20 to +5 at eye z=-20), same split.
    pGlColor3f(0.2f, 0.2f, 0.2f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-20.0f, -20.0f, -20.0f);
    pGlVertex3f(  0.0f, -20.0f, -20.0f);
    pGlVertex3f(  0.0f,   5.0f, -20.0f);
    pGlVertex3f(-20.0f,   5.0f, -20.0f);
    pGlEnd();
    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(  0.0f, -20.0f, -20.0f);
    pGlVertex3f( 20.0f, -20.0f, -20.0f);
    pGlVertex3f( 20.0f,   5.0f, -20.0f);
    pGlVertex3f(  0.0f,   5.0f, -20.0f);
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

    unsigned int captureErr = gl.glGetError();
    Check(captureErr == 0, "capturing color and depth leaves no GL error");

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

    // Four points: either side of the edge at each depth. 4 px from the edge, so a blur of any
    // real radius must drag them toward each other.
    const int kDarkX = 60, kBrightX = 68;
    const int kNearY = 40;    // on the near slab (rows 0..80)
    const int kFarY = 104;    // on the far wall (rows 80..128)
    const unsigned char kDark = 51;    // 0.2 * 255
    const unsigned char kBright = 204; // 0.8 * 255

    unsigned char px[4] = {0, 0, 0, 0};

    // Sanity: the captured source really does carry the edge at both depths. If this fails the
    // scene is wrong and every blur assertion below would be meaningless.
    ReadColorPixel(gl, readFbo, srcTex, kDarkX, kNearY, px);
    CheckExact(px[0], kDark, "source: dark side of the near edge");
    ReadColorPixel(gl, readFbo, srcTex, kBrightX, kNearY, px);
    CheckExact(px[0], kBright, "source: bright side of the near edge");
    ReadColorPixel(gl, readFbo, srcTex, kDarkX, kFarY, px);
    CheckExact(px[0], kDark, "source: dark side of the far edge");
    ReadColorPixel(gl, readFbo, srcTex, kBrightX, kFarY, px);
    CheckExact(px[0], kBright, "source: bright side of the far edge");

    // blurStrength=0 is an exact no-op at every depth, in or out of focus - the same contract
    // every other stage's zero-strength case holds to.
    {
        bool wrote = ApplyDof(srcTex, dstTex, depthTex, width, height, projection,
                              kNearDepth, 20.0f, 0.0f);
        unsigned int err = gl.glGetError();
        Check(wrote && err == 0, "ApplyDof(blurStrength=0) runs without a GL error");

        ReadColorPixel(gl, readFbo, dstTex, kDarkX, kNearY, px);
        CheckExact(px[0], kDark, "blurStrength=0 reproduces the in-focus pixel exactly");
        ReadColorPixel(gl, readFbo, dstTex, kDarkX, kFarY, px);
        CheckExact(px[0], kDark, "blurStrength=0 reproduces the far pixel exactly too");
    }

    // Focused on the near slab: its edge must stay bit-exact (circle of confusion is zero
    // there, so the gather collapses to the centre tap), while the far wall's edge - 380 world
    // units outside the sharp band - must visibly smear toward the mean of 51 and 204.
    {
        bool wrote = ApplyDof(srcTex, dstTex, depthTex, width, height, projection,
                              kNearDepth, 20.0f, 1.0f);
        unsigned int err = gl.glGetError();
        Check(wrote && err == 0, "ApplyDof(focus on the near slab) runs without a GL error");

        ReadColorPixel(gl, readFbo, dstTex, kDarkX, kNearY, px);
        CheckExact(px[0], kDark, "the in-focus edge stays bit-exact on its dark side");
        ReadColorPixel(gl, readFbo, dstTex, kBrightX, kNearY, px);
        CheckExact(px[0], kBright, "the in-focus edge stays bit-exact on its bright side");

        ReadColorPixel(gl, readFbo, dstTex, kDarkX, kFarY, px);
        unsigned char blurredDark = px[0];
        ReadColorPixel(gl, readFbo, dstTex, kBrightX, kFarY, px);
        unsigned char blurredBright = px[0];
        printf("far edge after blur: dark side %d (was %d), bright side %d (was %d)\n",
               blurredDark, kDark, blurredBright, kBright);
        Check(blurredDark > kDark + 20, "the out-of-focus edge brightens on its dark side");
        Check(blurredBright < kBright - 20, "the out-of-focus edge darkens on its bright side");
    }

    // focusDistance=0 means "focus on whatever is at screen centre". The centre pixel sits on
    // the near slab, so this must behave like an explicit focus of 20 world units.
    {
        bool wrote = ApplyDof(srcTex, dstTex, depthTex, width, height, projection,
                              0.0f, 20.0f, 1.0f);
        unsigned int err = gl.glGetError();
        Check(wrote && err == 0, "ApplyDof(auto focus) runs without a GL error");

        ReadColorPixel(gl, readFbo, dstTex, kDarkX, kNearY, px);
        CheckExact(px[0], kDark, "auto focus locks onto the centre depth and keeps it sharp");
        ReadColorPixel(gl, readFbo, dstTex, kDarkX, kFarY, px);
        Check(px[0] > kDark + 20, "auto focus still blurs what the centre is not focused on");
    }

    // No captured projection (zNear/zFar left at zero): there is no way to turn raw depth into
    // the world units focusDistance is denominated in, so this must no-op rather than blur
    // against a garbage distance - the same contract ApplySsao holds to.
    {
        ProjectionParams empty;
        bool wrote = ApplyDof(srcTex, dstTex, depthTex, width, height, empty,
                              kNearDepth, 20.0f, 1.0f);
        Check(!wrote, "ApplyDof no-ops when no projection was captured");
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
