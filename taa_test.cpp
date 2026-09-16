// Creates a real OpenGL context, clears the back buffer to a known color, captures it into a
// src RGBA16F texture once (mimicking what post_effects.cpp's shared pipeline now does once
// per frame), and calls ApplyTaa() four times in a row against that unchanging src texture -
// exercising the historyValid=0 passthrough path (call 1) and the ping-pong blend/clamp path
// across multiple successful frames (calls 2-4). Since the scene never changes, every call
// should still reproduce the clear color (the neighborhood clamp box degenerates to a single
// value when the frame is static, so blending never drifts) - this also functions as the
// ghosting-mitigation check: if the ping-pong role-flip or clamp logic were wrong, a static
// scene would still visibly drift or corrupt after a few frames, which this test would catch.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "taa.h"

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

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel, int callNum) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: call %d, %s channel = %d, expected ~%d (tolerance %d)\n", callNum, channel, actual, expected, tolerance);
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

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxTaaTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "taa_test", WS_OVERLAPPEDWINDOW,
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

    for (int call = 1; call <= 4; ++call) {
        bool wrote = ApplyTaa(srcTex, dstTex, width, height, 0.85f);
        unsigned int err = gl.glGetError();
        if (!wrote || err != 0) {
            printf("FAIL: call %d left glGetError() = 0x%04X (wrote=%d)\n", call, err, wrote ? 1 : 0);
            ok = false;
            continue;
        }
        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after call %d: r=%d g=%d b=%d a=%d\n", call, pixel[0], pixel[1], pixel[2], pixel[3]);
        bool callOk = CheckClose(pixel[0], 204, 2, "r", call);
        callOk = CheckClose(pixel[1], 51, 2, "g", call) && callOk;
        callOk = CheckClose(pixel[2], 25, 2, "b", call) && callOk;
        if (callOk) {
            printf("PASS: call %d reproduced the cleared color\n", call);
        }
        ok = callOk && ok;
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
