// Creates a real OpenGL context, clears the back buffer to a known color, writes a small
// identity ".cube" LUT fixture to disk, and checks ApplyLutGrading()'s GPU pipeline (capture
// -> parse .cube -> upload 3D texture -> compute dispatch -> blit): an identity LUT at
// strength=1 must reproduce the input (trilinear interpolation of an identity 2x2x2 cube maps
// any point to itself), strength=0 must be an exact passthrough regardless of LUT content, and
// a missing LUT file must no-op with no GL error - see gl_loader_test.cpp's header comment for
// why <windows.h> is safe here.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "lut_grading.h"

namespace {

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: %s channel = %d, expected ~%d (tolerance %d)\n", channel, actual, expected, tolerance);
        return false;
    }
    return true;
}

bool WriteIdentityCube(const char* path) {
    FILE* f = fopen(path, "w");
    if (f == nullptr) {
        printf("FAIL: could not create fixture '%s'\n", path);
        return false;
    }
    fputs("TITLE \"identity\"\n", f);
    fputs("LUT_3D_SIZE 2\n", f);
    // r fastest, then g, then b - the same order lut_grading.cpp expects.
    fputs("0.0 0.0 0.0\n", f);
    fputs("1.0 0.0 0.0\n", f);
    fputs("0.0 1.0 0.0\n", f);
    fputs("1.0 1.0 0.0\n", f);
    fputs("0.0 0.0 1.0\n", f);
    fputs("1.0 0.0 1.0\n", f);
    fputs("0.0 1.0 1.0\n", f);
    fputs("1.0 1.0 1.0\n", f);
    fclose(f);
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxLutGradingTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "lut_grading_test", WS_OVERLAPPEDWINDOW,
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
    pGlClearColor(0.6f, 0.3f, 0.8f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);

    bool ok = true;

    if (!WriteIdentityCube("lut_grading_test_identity.cube")) {
        return 1;
    }

    const GlComputeApi& gl = GetGlComputeApi();

    ApplyLutGrading("lut_grading_test_identity.cube", 1.0f);
    unsigned int err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyLutGrading(identity, 1.0) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after ApplyLutGrading(identity, 1.0): r=%d g=%d b=%d a=%d\n", pixel[0], pixel[1], pixel[2], pixel[3]);
        ok = CheckClose(pixel[0], 153, 3, "r") && ok;
        ok = CheckClose(pixel[1], 77, 3, "g") && ok;
        ok = CheckClose(pixel[2], 204, 3, "b") && ok;
        if (ok) printf("PASS: identity LUT at strength=1.0 reproduced the cleared color\n");
    }

    ApplyLutGrading("lut_grading_test_identity.cube", 0.0f);
    err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyLutGrading(identity, 0.0) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        ok = CheckClose(pixel[0], 153, 3, "r") && ok;
        ok = CheckClose(pixel[1], 77, 3, "g") && ok;
        ok = CheckClose(pixel[2], 204, 3, "b") && ok;
        if (ok) printf("PASS: strength=0.0 is an exact passthrough\n");
    }

    ApplyLutGrading("lut_grading_test_does_not_exist.cube", 1.0f);
    err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: ApplyLutGrading(missing file) left glGetError() = 0x%04X\n", err);
        ok = false;
    } else {
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        ok = CheckClose(pixel[0], 153, 3, "r") && ok;
        ok = CheckClose(pixel[1], 77, 3, "g") && ok;
        ok = CheckClose(pixel[2], 204, 3, "b") && ok;
        if (ok) printf("PASS: missing LUT file left the back buffer untouched, no GL error\n");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
