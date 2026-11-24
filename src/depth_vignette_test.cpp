// EXPERIMENTAL (see depth_vignette.h). Creates a real OpenGL context, renders two real quads at
// different eye-space depths with depth testing on (a near quad on the left half of the screen,
// a far quad on the right half), captures both color and depth off the real back buffer the same
// way post_effects.cpp's shared pipeline does, and checks ApplyDepthVignette()'s GPU pipeline at
// intensity=0 (must reproduce the input exactly, near and far both) and at a strong
// intensity/low threshold (the near quad must stay bit-exact, the far quad must come back
// visibly darker) - see vignette_test.cpp's header comment for why checking two points, not
// just one, is what makes this more than a smoke test.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "depth_vignette.h"

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

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* what) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: %s = %d, expected ~%d (tolerance %d)\n", what, actual, expected, tolerance);
        return false;
    }
    return true;
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
    wc.lpszClassName = "AnaxDepthVignetteTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int width = 128, height = 128;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "depth_vignette_test", WS_OVERLAPPEDWINDOW,
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

    // Fixed-function GL 1.1 entry points, resolved the same way post_effects_test.cpp/
    // vignette_test.cpp resolve glClear/glClearColor/glViewport - immediate mode is enough to
    // put real, differently-depthed geometry on screen without a shader.
    HMODULE realGl = GetModuleHandleA("opengl32.dll");
    typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float, float, float, float);
    typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int);
    typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);
    typedef void (__stdcall *PFNGLENABLEPROC)(unsigned int);
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
    auto pGlEnable = (PFNGLENABLEPROC)GetProcAddress(realGl, "glEnable");
    auto pGlMatrixMode = (PFNGLMATRIXMODEPROC)GetProcAddress(realGl, "glMatrixMode");
    auto pGlLoadIdentity = (PFNGLLOADIDENTITYPROC)GetProcAddress(realGl, "glLoadIdentity");
    auto pGlOrtho = (PFNGLORTHOPROC)GetProcAddress(realGl, "glOrtho");
    auto pGlBegin = (PFNGLBEGINPROC)GetProcAddress(realGl, "glBegin");
    auto pGlEnd = (PFNGLENDPROC)GetProcAddress(realGl, "glEnd");
    auto pGlVertex3f = (PFNGLVERTEX3FPROC)GetProcAddress(realGl, "glVertex3f");
    auto pGlColor3f = (PFNGLCOLOR3FPROC)GetProcAddress(realGl, "glColor3f");

    pGlViewport(0, 0, width, height);
    pGlEnable(GL_DEPTH_TEST);
    pGlMatrixMode(GL_PROJECTION);
    pGlLoadIdentity();
    // near=0.1, far=100: eye z=-1 maps to raw depth ~0.009 (near plane side), eye z=-90 maps to
    // raw depth ~0.9 (far plane side) - see depth_vignette_test's header comment.
    pGlOrtho(-1.0, 1.0, -1.0, 1.0, 0.1, 100.0);
    pGlMatrixMode(GL_MODELVIEW);
    pGlLoadIdentity();

    pGlClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);  // Near quad: left half of the screen, eye z = -1.
    pGlVertex3f(-1.0f, -1.0f, -1.0f);
    pGlVertex3f(0.0f, -1.0f, -1.0f);
    pGlVertex3f(0.0f, 1.0f, -1.0f);
    pGlVertex3f(-1.0f, 1.0f, -1.0f);
    pGlEnd();

    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);  // Far quad: right half of the screen, eye z = -90.
    pGlVertex3f(0.0f, -1.0f, -90.0f);
    pGlVertex3f(1.0f, -1.0f, -90.0f);
    pGlVertex3f(1.0f, 1.0f, -90.0f);
    pGlVertex3f(0.0f, 1.0f, -90.0f);
    pGlEnd();

    bool ok = true;
    const GlComputeApi& gl = GetGlComputeApi();
    ok = (gl.loaded) && ok;
    if (!gl.loaded) {
        printf("Skipping: no GL 4.3 compute support on this context/driver.\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        return ok ? 0 : 1;
    }

    // Capture color and depth off the real back buffer exactly like post_effects.cpp's shared
    // pipeline does: glCopyTexSubImage2D for color, a blit into a depth-attached FBO for depth.
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
    ok = (captureErr == 0) && ok;
    if (captureErr != 0) {
        printf("FAIL: capturing color/depth left glGetError() = 0x%04X\n", captureErr);
    }

    unsigned int dstTex = CreateColorTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    const int nearX = width / 4, nearY = height / 2;   // Inside the near quad.
    const int farX = 3 * width / 4, farY = height / 2; // Inside the far quad.

    // intensity=0: every pixel, near and far alike, must come back unchanged - the scale
    // factor is exactly 1.0 regardless of depth.
    bool wrote1 = ApplyDepthVignette(srcTex, dstTex, depthTex, width, height, 0.0f, 0.3f);
    unsigned int err = gl.glGetError();
    if (!wrote1 || err != 0) {
        printf("FAIL: ApplyDepthVignette(0.0, 0.3) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote1 ? 1 : 0);
        ok = false;
    } else {
        unsigned char nearPixel[4] = {0, 0, 0, 0};
        unsigned char farPixel[4] = {0, 0, 0, 0};
        ReadColorPixel(gl, readFbo, dstTex, nearX, nearY, nearPixel);
        ReadColorPixel(gl, readFbo, dstTex, farX, farY, farPixel);
        printf("intensity=0.0: near r=%d, far r=%d\n", nearPixel[0], farPixel[0]);
        ok = CheckClose(nearPixel[0], 204, 0, "near r") && ok;
        ok = CheckClose(farPixel[0], 204, 0, "far r") && ok;
        if (ok) printf("PASS: intensity=0.0 reproduced the source exactly at both depths\n");
    }

    // intensity=0.8, threshold=0.3: the near quad's raw depth (~0.009) sits well below the
    // threshold so it must stay bit-exact, while the far quad's raw depth (~0.9) lands deep in
    // the falloff and should come back far darker than the source.
    bool wrote2 = ApplyDepthVignette(srcTex, dstTex, depthTex, width, height, 0.8f, 0.3f);
    err = gl.glGetError();
    if (!wrote2 || err != 0) {
        printf("FAIL: ApplyDepthVignette(0.8, 0.3) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote2 ? 1 : 0);
        ok = false;
    } else {
        unsigned char nearPixel[4] = {0, 0, 0, 0};
        unsigned char farPixel[4] = {0, 0, 0, 0};
        ReadColorPixel(gl, readFbo, dstTex, nearX, nearY, nearPixel);
        ReadColorPixel(gl, readFbo, dstTex, farX, farY, farPixel);
        printf("intensity=0.8: near r=%d, far r=%d\n", nearPixel[0], farPixel[0]);
        ok = CheckClose(nearPixel[0], 204, 1, "near r") && ok;
        if (farPixel[0] >= 120) {
            printf("FAIL: far r = %d, expected well below the source's 204 (depth vignette did not darken)\n", farPixel[0]);
            ok = false;
        } else {
            printf("PASS: intensity=0.8 left the near quad intact and darkened the far quad\n");
        }
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
