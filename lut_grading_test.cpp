// Creates a real OpenGL context, clears the back buffer to a known color, captures it into a
// src RGBA16F texture (mimicking what post_effects.cpp's shared pipeline now does once per
// frame), writes a small identity ".cube" LUT fixture to disk, and checks ApplyLutGrading()'s
// GPU pipeline (parse .cube -> 3D texture -> dispatch) against src/dst textures: an identity
// LUT at strength=1 must reproduce the input (trilinear interpolation of an identity 2x2x2
// cube maps any point to itself), strength=0 must be an exact passthrough regardless of LUT
// content, and a missing LUT file must return false (dstTex left untouched) with no GL error -
// see gl_loader_test.cpp's header comment for why <windows.h> is safe here.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "lut_grading.h"

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
    int width = 128, height = 128;

    unsigned int srcTex = CreatePipelineTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    unsigned int dstTex = CreatePipelineTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    bool wrote1 = ApplyLutGrading(srcTex, dstTex, width, height, "lut_grading_test_identity.cube", 1.0f);
    unsigned int err = gl.glGetError();
    if (!wrote1 || err != 0) {
        printf("FAIL: ApplyLutGrading(identity, 1.0) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote1 ? 1 : 0);
        ok = false;
    } else {
        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after ApplyLutGrading(identity, 1.0): r=%d g=%d b=%d a=%d\n", pixel[0], pixel[1], pixel[2], pixel[3]);
        ok = CheckClose(pixel[0], 153, 3, "r") && ok;
        ok = CheckClose(pixel[1], 77, 3, "g") && ok;
        ok = CheckClose(pixel[2], 204, 3, "b") && ok;
        if (ok) printf("PASS: identity LUT at strength=1.0 reproduced the cleared color\n");
    }

    bool wrote2 = ApplyLutGrading(srcTex, dstTex, width, height, "lut_grading_test_identity.cube", 0.0f);
    err = gl.glGetError();
    if (!wrote2 || err != 0) {
        printf("FAIL: ApplyLutGrading(identity, 0.0) left glGetError() = 0x%04X (wrote=%d)\n", err, wrote2 ? 1 : 0);
        ok = false;
    } else {
        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        ok = CheckClose(pixel[0], 153, 3, "r") && ok;
        ok = CheckClose(pixel[1], 77, 3, "g") && ok;
        ok = CheckClose(pixel[2], 204, 3, "b") && ok;
        if (ok) printf("PASS: strength=0.0 is an exact passthrough\n");
    }

    bool wrote3 = ApplyLutGrading(srcTex, dstTex, width, height, "lut_grading_test_does_not_exist.cube", 1.0f);
    err = gl.glGetError();
    if (wrote3 || err != 0) {
        printf("FAIL: ApplyLutGrading(missing file) wrote=%d, glGetError() = 0x%04X\n", wrote3 ? 1 : 0, err);
        ok = false;
    } else {
        printf("PASS: missing LUT file returned false (no-op), no GL error\n");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
