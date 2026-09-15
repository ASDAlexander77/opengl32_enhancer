// Creates a real OpenGL context against a hidden window and checks that gl_loader.cpp can
// resolve every GL 4.3 compute-shader entry point it needs. This is the only place in the
// new post-effects code that creates a context from scratch - everywhere else
// (gl_loader.cpp, post_effects.cpp, ...) runs inside a context the host app already made
// current via wglSwapBuffers. Free to use <windows.h> here: unlike wrapper.cpp, this file
// is a standalone .exe and doesn't itself export dllexport definitions of any wgl*/gl*
// names, so there's no collision (see Global Constraints in the plan).
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxGlLoaderTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "gl_loader_test", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
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

    typedef const unsigned char* (__stdcall *PFNGLGETSTRINGPROC)(unsigned int);
    PFNGLGETSTRINGPROC pGlGetString =
        (PFNGLGETSTRINGPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetString");
    const unsigned int GL_VERSION_ENUM = 0x1F02;
    const unsigned char* version = pGlGetString ? pGlGetString(GL_VERSION_ENUM) : (const unsigned char*)"<unknown>";
    printf("Real GL context created. GL_VERSION = %s\n", version);

    GlComputeApi api;
    bool ok = LoadGlComputeApi(api);
    printf("LoadGlComputeApi -> %s\n", ok ? "PASS (all entry points resolved)" : "FAIL (see FAILED lines above)");

    // Sanity check for GetGlComputeApi()'s retry-until-success caching (gl_loader.cpp): with
    // a real context already current, it should resolve successfully on this first call too.
    const GlComputeApi& cachedApi = GetGlComputeApi();
    printf("GetGlComputeApi -> %s\n", cachedApi.loaded ? "PASS (loaded=true)" : "FAIL (loaded=false)");
    ok = ok && cachedApi.loaded;

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
