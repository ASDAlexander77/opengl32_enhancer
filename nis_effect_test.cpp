// Creates a real OpenGL context, clears the back buffer to a known color, and runs both
// ApplyNVScaler and ApplyNVSharpen through it, checking for GL errors and a crash-free run -
// see gl_loader_test.cpp's header comment for why <windows.h> is safe to use here (a
// standalone .exe, not linked into the proxy DLL). This does not assert the output pixel
// color against NIS's exact expected output: NVScaler/NVSharpen's edge-adaptive sharpening
// intentionally changes pixel values near "edges" (including the clear color's boundary with
// whatever undefined memory was in the newly-created window), so a flat clear color is not a
// meaningful precise correctness oracle for this shader. It does, however, assert the center
// pixel is not fully black after each call - a weak but load-bearing check that catches the
// pipeline silently producing no output (e.g. a texture-unit binding mismatch between the
// C++ dispatch code and the shaders' layout(binding=N) declarations), which a plain
// glGetError()-only check cannot detect.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "nis_effect.h"

namespace {

bool RunOnce(void (*applyFn)(float), const char* name) {
    applyFn(0.5f);
    const GlComputeApi& gl = GetGlComputeApi();
    unsigned int err = gl.glGetError();
    if (err != 0) {
        printf("FAIL: %s left glGetError() = 0x%04X\n", name, err);
        return false;
    }

    // Weak pixel check: this doesn't try to model NIS's exact output, it just catches "the
    // whole pipeline silently no-op'd into black" (exactly how the binding-mismatch bug that
    // made both effects render black slipped past a plain glGetError()-only check). The test
    // window is cleared to a non-black color (0.8, 0.2, 0.1) before any effect runs, so a
    // fully-black readback at the center pixel means the effect's output never made it to the
    // back buffer.
    const unsigned int GL_RGBA = 0x1908;
    const unsigned int GL_UNSIGNED_BYTE = 0x1401;
    unsigned char pixel[4] = {0, 0, 0, 0};
    gl.glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    unsigned int readErr = gl.glGetError();
    if (readErr != 0) {
        printf("FAIL: %s glReadPixels left glGetError() = 0x%04X\n", name, readErr);
        return false;
    }
    if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
        printf("FAIL: %s produced a black pixel at (64,64): (%u,%u,%u,%u)\n",
               name, pixel[0], pixel[1], pixel[2], pixel[3]);
        return false;
    }

    printf("PASS: %s ran with no GL error, center pixel = (%u,%u,%u,%u)\n",
           name, pixel[0], pixel[1], pixel[2], pixel[3]);
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxNisEffectTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "nis_effect_test", WS_OVERLAPPEDWINDOW,
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
    ok = RunOnce(&ApplyNVScaler, "ApplyNVScaler") && ok;
    ok = RunOnce(&ApplyNVSharpen, "ApplyNVSharpen") && ok;
    // Run each a second time to exercise EnsureResources'/EnsureStaticResources' reuse
    // (not-first-call) path, same as bilinear_upscale_test.cpp does for ApplyBilinearUpscale.
    ok = RunOnce(&ApplyNVScaler, "ApplyNVScaler (second call)") && ok;
    ok = RunOnce(&ApplyNVSharpen, "ApplyNVSharpen (second call)") && ok;

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
