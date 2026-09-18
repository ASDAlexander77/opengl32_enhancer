// Creates a real OpenGL context, renders a bright square on black at a KNOWN off-centre
// position, and checks ApplyLightShafts()'s two-pass GPU pipeline: the detect pass must find
// that square as the light source, and the shaft pass must radiate from it.
//
// Off-centre on purpose. A light at screen centre would pass even if the detect pass were
// broken and the shader simply assumed the middle of the frame, which is exactly the bug most
// worth catching here - the whole point of the detect pass is that a fixed screen position is
// useless in a game where the camera moves.
//
// The scene is black apart from the sun, so any non-zero pixel in the result is a shaft and
// nothing else, and brightness can be compared between points without unpicking scene content.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "light_shafts.h"

namespace {

const unsigned int GL_TEXTURE_2D           = 0x0DE1;
const unsigned int GL_RGBA16F              = 0x881A;
const unsigned int GL_TEXTURE_MIN_FILTER   = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER   = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S       = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T       = 0x2803;
const unsigned int GL_NEAREST              = 0x2600;
const unsigned int GL_CLAMP_TO_EDGE        = 0x812F;
const unsigned int GL_FRAMEBUFFER          = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER     = 0x8CA8;
const unsigned int GL_COLOR_ATTACHMENT0    = 0x8CE0;
const unsigned int GL_BACK                 = 0x0405;
const unsigned int GL_COLOR_BUFFER_BIT     = 0x00004000;
const unsigned int GL_DEPTH_BUFFER_BIT     = 0x00000100;
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
    wc.lpszClassName = "AnaxLightShaftsTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int width = 128, height = 128;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "light_shafts_test", WS_OVERLAPPEDWINDOW,
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
    typedef void (__stdcall *PFNGLMATRIXMODEPROC)(unsigned int);
    typedef void (__stdcall *PFNGLLOADIDENTITYPROC)(void);
    typedef void (__stdcall *PFNGLORTHOPROC)(double, double, double, double, double, double);
    typedef void (__stdcall *PFNGLBEGINPROC)(unsigned int);
    typedef void (__stdcall *PFNGLENDPROC)(void);
    typedef void (__stdcall *PFNGLVERTEX3FPROC)(float, float, float);
    typedef void (__stdcall *PFNGLCOLOR3FPROC)(float, float, float);
    auto pGlClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(realGl, "glClearColor");
    auto pGlClear = (PFNGLCLEARPROC)GetProcAddress(realGl, "glClear");
    auto pGlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(realGl, "glViewport");
    auto pGlMatrixMode = (PFNGLMATRIXMODEPROC)GetProcAddress(realGl, "glMatrixMode");
    auto pGlLoadIdentity = (PFNGLLOADIDENTITYPROC)GetProcAddress(realGl, "glLoadIdentity");
    auto pGlOrtho = (PFNGLORTHOPROC)GetProcAddress(realGl, "glOrtho");
    auto pGlBegin = (PFNGLBEGINPROC)GetProcAddress(realGl, "glBegin");
    auto pGlEnd = (PFNGLENDPROC)GetProcAddress(realGl, "glEnd");
    auto pGlVertex3f = (PFNGLVERTEX3FPROC)GetProcAddress(realGl, "glVertex3f");
    auto pGlColor3f = (PFNGLCOLOR3FPROC)GetProcAddress(realGl, "glColor3f");

    pGlViewport(0, 0, width, height);
    pGlMatrixMode(GL_PROJECTION);
    pGlLoadIdentity();
    pGlOrtho(-1.0, 1.0, -1.0, 1.0, 0.1, 100.0);
    pGlMatrixMode(GL_MODELVIEW);
    pGlLoadIdentity();

    pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // The "sun": a white square centred on pixel (32, 96), i.e. left of centre and high up.
    // NDC x = 2*(px/128)-1, so pixels 24..40 -> -0.625..-0.375 and 88..104 -> 0.375..0.625.
    pGlColor3f(1.0f, 1.0f, 1.0f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-0.625f, 0.375f, -1.0f);
    pGlVertex3f(-0.375f, 0.375f, -1.0f);
    pGlVertex3f(-0.375f, 0.625f, -1.0f);
    pGlVertex3f(-0.625f, 0.625f, -1.0f);
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
    Check(gl.glGetError() == 0, "capturing the scene leaves no GL error");

    unsigned int dstTex = CreateColorTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    // Two black points at very different distances from the sun at (32, 96).
    const int kNearX = 48, kNearY = 80;    // ~23 px away
    const int kFarX = 112, kFarY = 16;     // ~110 px away

    Check(ReadRed(gl, readFbo, srcTex, kNearX, kNearY) == 0, "source: the near sample starts black");
    Check(ReadRed(gl, readFbo, srcTex, kFarX, kFarY) == 0, "source: the far sample starts black");

    // intensity=0 is an exact no-op.
    {
        bool wrote = ApplyLightShafts(srcTex, dstTex, width, height, 0.0f, 0.5f, 0.95f, 0.5f);
        Check(wrote && gl.glGetError() == 0, "ApplyLightShafts(intensity=0) runs without a GL error");
        Check(ReadRed(gl, readFbo, dstTex, kNearX, kNearY) == 0,
              "intensity=0 leaves a black pixel exactly black");
    }

    // The real pass. Both samples should pick up light radiating from the sun, and the nearer
    // one must pick up more - that gradient is what distinguishes a shaft from a flat brighten.
    unsigned char nearLit = 0, farLit = 0;
    {
        bool wrote = ApplyLightShafts(srcTex, dstTex, width, height, 1.0f, 0.6f, 0.96f, 0.5f);
        Check(wrote && gl.glGetError() == 0, "ApplyLightShafts(intensity=1) runs without a GL error");

        nearLit = ReadRed(gl, readFbo, dstTex, kNearX, kNearY);
        farLit = ReadRed(gl, readFbo, dstTex, kFarX, kFarY);
        printf("near sample = %d, far sample = %d\n", (int)nearLit, (int)farLit);

        Check(nearLit > 0, "a black pixel near the light picks up a shaft");
        Check(nearLit > farLit, "the shaft falls off with distance from the light");
    }

    // Detection, stated as a test. The two points below are mirror images about the screen
    // centre (64,64) - same row, same distance from the middle - but the sun is at x=32, so the
    // left one is far closer to it. If the detect pass were ignored and the shader simply
    // assumed the centre of the frame, the two would come out identical.
    {
        // density=1.0 so both marches actually reach the light. At a shorter density a point can
        // simply never get there, which is what made an earlier version of this check compare
        // two zeroes and prove nothing.
        bool wrote = ApplyLightShafts(srcTex, dstTex, width, height, 1.0f, 1.0f, 0.96f, 0.5f);
        Check(wrote, "ApplyLightShafts(density=1) runs");
        unsigned char sunSide = ReadRed(gl, readFbo, dstTex, 32, 64);
        unsigned char awaySide = ReadRed(gl, readFbo, dstTex, 96, 64);
        printf("sun-side = %d, opposite side = %d\n", (int)sunSide, (int)awaySide);
        Check(sunSide > awaySide, "shafts come from the detected light, not from screen centre");
    }

    // Nothing in the frame is brighter than a threshold of 1.0, so no light source is found and
    // the stage must hand back the source untouched instead of dividing by a zero weight.
    {
        bool wrote = ApplyLightShafts(srcTex, dstTex, width, height, 1.0f, 0.6f, 0.96f, 1.0f);
        Check(wrote && gl.glGetError() == 0, "ApplyLightShafts with no light found runs cleanly");
        Check(ReadRed(gl, readFbo, dstTex, kNearX, kNearY) == 0,
              "with no light detected, a black pixel stays exactly black");
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
