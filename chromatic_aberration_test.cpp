// Creates a real OpenGL context, clears the back buffer to a known color, captures it into a
// src RGBA16F texture (mimicking what post_effects.cpp's shared pipeline now does once per
// frame), and checks ApplyChromaticAberration()'s GPU pipeline against src/dst textures at
// strength=0 (must reproduce the input exactly - all three channels sample the same texel)
// and at strength=1.0 - see gl_loader_test.cpp's header comment for why <windows.h> is safe
// here.
//
// This deliberately does NOT assert visible color fringing, because a flat test frame cannot
// show any: the effect only separates channels where the image has an edge, and the offsets
// are defined in UV space (resolution-independent by design), so at this test's 128px width
// they are sub-pixel anyway. What the flat frame DOES pin down is everything structural, and
// it does so precisely because the clear color's three channels are all different (204/51/25):
// the strength=1.0 readback must still come back as exactly that color, so a shader that
// swapped red and blue, sampled the wrong texture for a channel, or ran the offsets off the
// edge into garbage would fail here rather than slip through a no-GL-error-only check.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "chromatic_aberration.h"

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

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: %s channel = %d, expected ~%d (tolerance %d)\n", channel, actual, expected, tolerance);
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

bool CheckReadback(const GlComputeApi& gl, unsigned int readFbo, unsigned int tex,
                   int x, int y, int tolerance, const char* label) {
    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    unsigned char pixel[4] = {0, 0, 0, 0};
    gl.glReadPixels(x, y, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
    printf("%s at (%d,%d): r=%d g=%d b=%d a=%d\n", label, x, y, pixel[0], pixel[1], pixel[2], pixel[3]);
    bool ok = true;
    ok = CheckClose(pixel[0], 204, tolerance, "r") && ok;
    ok = CheckClose(pixel[1], 51, tolerance, "g") && ok;
    ok = CheckClose(pixel[2], 25, tolerance, "b") && ok;
    return ok;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxChromaticAberrationTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "chromatic_aberration_test", WS_OVERLAPPEDWINDOW,
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

    bool wrote1 = ApplyChromaticAberration(srcTex, dstTex, width, height, 0.0f);
    unsigned int err = gl.glGetError();
    if (!wrote1 || err != 0) {
        printf("FAIL: ApplyChromaticAberration(0.0) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote1 ? 1 : 0);
        ok = false;
    } else {
        ok = CheckReadback(gl, readFbo, dstTex, 64, 64, 0, "strength=0.0") && ok;
        if (ok) printf("PASS: strength=0.0 reproduced the cleared color exactly\n");
    }

    // strength=1.0 read near a corner, where the radial offset is at its largest - the point
    // where an out-of-bounds sample or a mixed-up channel would show up most clearly.
    bool wrote2 = ApplyChromaticAberration(srcTex, dstTex, width, height, 1.0f);
    err = gl.glGetError();
    if (!wrote2 || err != 0) {
        printf("FAIL: ApplyChromaticAberration(1.0) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote2 ? 1 : 0);
        ok = false;
    } else {
        bool cornerOk = CheckReadback(gl, readFbo, dstTex, 3, 3, 1, "strength=1.0 corner");
        bool centerOk = CheckReadback(gl, readFbo, dstTex, 64, 64, 1, "strength=1.0 center");
        ok = cornerOk && centerOk && ok;
        if (cornerOk && centerOk) {
            printf("PASS: strength=1.0 kept every channel on a flat frame (no channel swap, "
                   "no out-of-bounds sampling)\n");
        }
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
