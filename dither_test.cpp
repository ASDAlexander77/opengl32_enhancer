// Creates a real OpenGL context, clears the back buffer to a known color, and checks
// ApplyDither()'s GPU pipeline (capture -> compute dispatch -> blit) both at strength=0 (must
// reproduce the input unchanged - offset is always exactly 0.0 regardless of Bayer cell) and
// strength=1.0 (must run with no GL error and stay within +-1 LSB of the input, since the
// dither offset is bounded to +-0.5 of one 8-bit step) - see gl_loader_test.cpp's header
// comment for why <windows.h> is safe here.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "dither.h"

namespace {

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: %s channel = %d, expected ~%d (tolerance %d)\n", channel, actual, expected, tolerance);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxDitherTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "dither_test", WS_OVERLAPPEDWINDOW,
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

    ApplyDither(0.0f);
    unsigned int err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyDither(0.0) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after ApplyDither(0.0): r=%d g=%d b=%d a=%d\n", pixel[0], pixel[1], pixel[2], pixel[3]);
        ok = CheckClose(pixel[0], 204, 0, "r") && ok;
        ok = CheckClose(pixel[1], 51, 0, "g") && ok;
        ok = CheckClose(pixel[2], 25, 0, "b") && ok;
        if (ok) printf("PASS: strength=0.0 reproduced the cleared color exactly\n");
    }

    ApplyDither(1.0f);
    err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyDither(1.0) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after ApplyDither(1.0): r=%d g=%d b=%d a=%d\n", pixel[0], pixel[1], pixel[2], pixel[3]);
        ok = CheckClose(pixel[0], 204, 1, "r") && ok;
        ok = CheckClose(pixel[1], 51, 1, "g") && ok;
        ok = CheckClose(pixel[2], 25, 1, "b") && ok;
        if (ok) printf("PASS: strength=1.0 ran with no GL error and stayed within +-1 LSB\n");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
