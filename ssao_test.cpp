// Creates a real OpenGL context, renders a real 3D scene under a perspective (glFrustum)
// projection with depth testing on - a large back wall with a smaller box standing in front of
// it - captures color and depth off the real back buffer the same way post_effects.cpp's shared
// pipeline does, and checks ApplySsao()'s two-pass GPU pipeline:
//   - at intensity=0 it must reproduce the input exactly everywhere (no occlusion term at all),
//   - at a strong intensity the open middle of the wall, far from the box, must stay essentially
//     unoccluded, while the wall right beside the box's silhouette must come back measurably
//     darker - which is the actual claim SSAO makes, and what a stage that darkened uniformly
//     (or not at all) would fail.
// It also checks the no-projection path returns false rather than guessing, since that is the
// graceful-degradation contract post_effects.cpp relies on - see ssao.h.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "ssao.h"

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
    wc.lpszClassName = "AnaxSsaoTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int width = 256, height = 256;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "ssao_test", WS_OVERLAPPEDWINDOW,
        0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    RECT clientRect = {};
    GetClientRect(hwnd, &clientRect);
    if (clientRect.right - clientRect.left != width || clientRect.bottom - clientRect.top != height) {
        printf("FAIL: client area is %dx%d, expected %dx%d\n",
               (int)(clientRect.right - clientRect.left), (int)(clientRect.bottom - clientRect.top),
               width, height);
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

    // A symmetric 90-degree frustum: near=1 gives near-plane extents of +-1, and the same
    // numbers go into ProjectionParams below so the stage unprojects with exactly the projection
    // this scene was rendered with.
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

    // Back wall at z=-60, spanning well past the frustum edges so it fills the frame.
    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-80.0f, -80.0f, -60.0f);
    pGlVertex3f( 80.0f, -80.0f, -60.0f);
    pGlVertex3f( 80.0f,  80.0f, -60.0f);
    pGlVertex3f(-80.0f,  80.0f, -60.0f);
    pGlEnd();

    // A box face standing well in front of the wall, occupying the left portion of the frame.
    // The wall pixels just to its right are the ones that should pick up occlusion.
    pGlColor3f(0.8f, 0.8f, 0.8f);
    pGlBegin(GL_QUADS);
    pGlVertex3f(-40.0f, -20.0f, -40.0f);
    pGlVertex3f(  0.0f, -20.0f, -40.0f);
    pGlVertex3f(  0.0f,  20.0f, -40.0f);
    pGlVertex3f(-40.0f,  20.0f, -40.0f);
    pGlEnd();

    bool ok = true;
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        printf("Skipping: no GL 4.3 compute support on this context/driver.\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        return 0;
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
    if (captureErr != 0) {
        printf("FAIL: capturing color/depth left glGetError() = 0x%04X\n", captureErr);
        ok = false;
    }

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

    // The box face spans x in [-40, 0] at z=-40, which projects to the left half of the frame.
    // Sample the wall just right of that silhouette (should be occluded by the box) against the
    // wall far to the right (open, nothing nearby to occlude it).
    const int creaseX = width / 2 + 6, creaseY = height / 2;
    const int openX = width - 12, openY = height / 2;

    bool wrote1 = ApplySsao(srcTex, dstTex, depthTex, width, height, projection, 24.0f, 0.0f, 0.5f);
    unsigned int err = gl.glGetError();
    if (!wrote1 || err != 0) {
        printf("FAIL: ApplySsao(intensity=0) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote1 ? 1 : 0);
        ok = false;
    } else {
        unsigned char crease[4] = {0, 0, 0, 0};
        unsigned char open[4] = {0, 0, 0, 0};
        ReadColorPixel(gl, readFbo, dstTex, creaseX, creaseY, crease);
        ReadColorPixel(gl, readFbo, dstTex, openX, openY, open);
        printf("intensity=0.0: crease r=%d, open r=%d\n", crease[0], open[0]);
        if (crease[0] != 204 || open[0] != 204) {
            printf("FAIL: intensity=0 must reproduce the source (204) exactly everywhere\n");
            ok = false;
        } else {
            printf("PASS: intensity=0.0 reproduced the source exactly\n");
        }
    }

    bool wrote2 = ApplySsao(srcTex, dstTex, depthTex, width, height, projection, 24.0f, 1.0f, 0.5f);
    err = gl.glGetError();
    if (!wrote2 || err != 0) {
        printf("FAIL: ApplySsao(intensity=1) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote2 ? 1 : 0);
        ok = false;
    } else {
        unsigned char crease[4] = {0, 0, 0, 0};
        unsigned char open[4] = {0, 0, 0, 0};
        ReadColorPixel(gl, readFbo, dstTex, creaseX, creaseY, crease);
        ReadColorPixel(gl, readFbo, dstTex, openX, openY, open);
        printf("intensity=1.0: crease r=%d, open r=%d\n", crease[0], open[0]);
        if (open[0] < 180) {
            printf("FAIL: open wall r = %d, expected near the source's 204 - SSAO is darkening "
                   "flat unoccluded surface (self-occlusion; bias too low?)\n", open[0]);
            ok = false;
        } else if (crease[0] >= open[0] - 5) {
            printf("FAIL: crease r = %d vs open r = %d - expected the wall beside the box to be "
                   "measurably darker than open wall\n", crease[0], open[0]);
            ok = false;
        } else {
            printf("PASS: occlusion appears beside the box and not on the open wall\n");
        }
    }

    // No captured projection (zNear/zFar left at zero) must no-op rather than unprojecting with
    // garbage - the contract post_effects.cpp depends on for menu-only frames. See ssao.h.
    ProjectionParams empty;
    bool wrote3 = ApplySsao(srcTex, dstTex, depthTex, width, height, empty, 24.0f, 1.0f, 0.5f);
    if (wrote3) {
        printf("FAIL: ApplySsao with no captured projection returned true, expected a no-op\n");
        ok = false;
    } else {
        printf("PASS: no captured projection no-ops instead of guessing\n");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("\n%s\n", ok ? "All checks passed." : "Some checks FAILED.");
    return ok ? 0 : 1;
}
