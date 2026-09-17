// Creates a real OpenGL context and runs ApplyTextureFilterOverride()/F() directly. See
// gl_loader_test.cpp for why <windows.h> is safe here and for the window/context pattern.
// Reads opengl32_enhancer.ini next to this .exe like config.cpp always does - CMakeLists.txt
// copies texture_filter_test.ini there (renamed), a fixture with anisotropy=8, not the
// project's real shipped ini (see that file's header comment for why).
//
// Two tiers:
//   1. Selectivity, via a recording stub instead of the real GL call - cheap to run many
//      combinations through: does ApplyTextureFilterOverride[F]() upgrade ONLY a mipmap-
//      capable GL_TEXTURE_MIN_FILTER on GL_TEXTURE_2D, and forward every other pname/target/
//      filter-value combination completely unchanged?
//   2. The real GL truth, via an actual texture and glGetTexParameter* readback: does an
//      upgraded call actually land GL_LINEAR_MIPMAP_LINEAR and the configured (hardware-capped)
//      anisotropy on the real, currently-bound texture object?
#include <windows.h>
#include <cstdio>
#include <cmath>

#include "texture_filter.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                     = 0x0DE1;
const unsigned int GL_TEXTURE_1D                     = 0x0DE0;
const unsigned int GL_TEXTURE_MIN_FILTER             = 0x2801;
const unsigned int GL_TEXTURE_WRAP_S                 = 0x2802;
const unsigned int GL_NEAREST                        = 0x2600;
const unsigned int GL_LINEAR                         = 0x2601;
const unsigned int GL_NEAREST_MIPMAP_NEAREST         = 0x2700;
const unsigned int GL_LINEAR_MIPMAP_NEAREST          = 0x2701;
const unsigned int GL_LINEAR_MIPMAP_LINEAR           = 0x2703;
const unsigned int GL_CLAMP_TO_EDGE                  = 0x812F;
const unsigned int GL_RGBA8                          = 0x8058;
const unsigned int GL_TEXTURE_MAX_ANISOTROPY_EXT     = 0x84FE;
const unsigned int GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

// --- Tier 1: selectivity, via a recording stub. ---

struct LastCall {
    unsigned int target = 0;
    unsigned int pname = 0;
    int paramI = 0;
    float paramF = 0.0f;
    bool calledI = false;
    bool calledF = false;
};
LastCall g_lastCall;

void __stdcall StubTexParameteri(unsigned int target, unsigned int pname, int param) {
    g_lastCall.target = target;
    g_lastCall.pname = pname;
    g_lastCall.paramI = param;
    g_lastCall.calledI = true;
}

void __stdcall StubTexParameterf(unsigned int target, unsigned int pname, float param) {
    g_lastCall.target = target;
    g_lastCall.pname = pname;
    g_lastCall.paramF = param;
    g_lastCall.calledF = true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxTextureFilterTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "texture_filter_test", WS_OVERLAPPEDWINDOW,
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

    const GlComputeApi& gl = GetGlComputeApi();
    if (!Check(gl.loaded, "GetGlComputeApi() resolved on this context")) {
        return 1;
    }

    // --- Tier 1: selectivity ---
    {
        g_lastCall = LastCall{};
        ApplyTextureFilterOverride(StubTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                    (int)GL_NEAREST_MIPMAP_NEAREST);
        Check(g_lastCall.calledI && g_lastCall.paramI == (int)GL_LINEAR_MIPMAP_LINEAR,
              "mipmap-capable min filter on GL_TEXTURE_2D (i) is upgraded to GL_LINEAR_MIPMAP_LINEAR");
    }
    {
        g_lastCall = LastCall{};
        ApplyTextureFilterOverrideF(StubTexParameterf, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                     (float)GL_LINEAR_MIPMAP_NEAREST);
        Check(g_lastCall.calledF && g_lastCall.paramF == (float)GL_LINEAR_MIPMAP_LINEAR,
              "mipmap-capable min filter on GL_TEXTURE_2D (f) is upgraded to GL_LINEAR_MIPMAP_LINEAR");
    }
    {
        g_lastCall = LastCall{};
        ApplyTextureFilterOverride(StubTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                    (int)GL_LINEAR);
        Check(g_lastCall.calledI && g_lastCall.paramI == (int)GL_LINEAR,
              "non-mipmap min filter (GL_LINEAR) on GL_TEXTURE_2D passes through unchanged");
    }
    {
        g_lastCall = LastCall{};
        ApplyTextureFilterOverride(StubTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                    (int)GL_CLAMP_TO_EDGE);
        Check(g_lastCall.calledI && g_lastCall.pname == GL_TEXTURE_WRAP_S &&
              g_lastCall.paramI == (int)GL_CLAMP_TO_EDGE,
              "a different pname (GL_TEXTURE_WRAP_S) passes through unchanged");
    }
    {
        g_lastCall = LastCall{};
        ApplyTextureFilterOverride(StubTexParameteri, GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER,
                                    (int)GL_NEAREST_MIPMAP_NEAREST);
        Check(g_lastCall.calledI && g_lastCall.target == GL_TEXTURE_1D &&
              g_lastCall.paramI == (int)GL_NEAREST_MIPMAP_NEAREST,
              "a mipmap filter on a non-GL_TEXTURE_2D target passes through unchanged");
    }

    // --- Tier 2: real GL truth - does an upgraded call actually land on the real texture? ---
    typedef void (__stdcall *PFNGLGETTEXPARAMETERIVPROC)(unsigned int target, unsigned int pname, int* params);
    typedef void (__stdcall *PFNGLGETTEXPARAMETERFVPROC)(unsigned int target, unsigned int pname, float* params);
    HMODULE realGl = GetModuleHandleA("opengl32.dll");
    PFNGLGETTEXPARAMETERIVPROC pGetTexParameteriv =
        (PFNGLGETTEXPARAMETERIVPROC)GetProcAddress(realGl, "glGetTexParameteriv");
    PFNGLGETTEXPARAMETERFVPROC pGetTexParameterfv =
        (PFNGLGETTEXPARAMETERFVPROC)GetProcAddress(realGl, "glGetTexParameterfv");
    if (!Check(pGetTexParameteriv != nullptr && pGetTexParameterfv != nullptr,
               "resolved glGetTexParameteriv/fv for readback")) {
        return 1;
    }

    int hardwareMaxInt = 1;
    gl.glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &hardwareMaxInt);
    float hardwareMax = (hardwareMaxInt >= 1) ? (float)hardwareMaxInt : 1.0f;
    float expectedAniso = (8.0f < hardwareMax) ? 8.0f : hardwareMax;  // fixture ini: anisotropy=8
    printf("  hardware max anisotropy: %.1f, expecting %.1f applied\n", hardwareMax, expectedAniso);

    unsigned int tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    ApplyTextureFilterOverride(gl.glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                (int)GL_NEAREST_MIPMAP_NEAREST);
    Check(gl.glGetError() == 0, "real texture: no GL error after the upgraded call");

    int actualMinFilter = 0;
    pGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &actualMinFilter);
    printf("  real texture GL_TEXTURE_MIN_FILTER = 0x%04X (expected 0x%04X)\n",
           actualMinFilter, GL_LINEAR_MIPMAP_LINEAR);
    Check(actualMinFilter == (int)GL_LINEAR_MIPMAP_LINEAR,
          "real texture: min filter actually landed as GL_LINEAR_MIPMAP_LINEAR on the driver");

    float actualAniso = 0.0f;
    pGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &actualAniso);
    printf("  real texture GL_TEXTURE_MAX_ANISOTROPY_EXT = %.2f (expected %.2f)\n",
           actualAniso, expectedAniso);
    Check(fabsf(actualAniso - expectedAniso) < 0.01f,
          "real texture: anisotropy actually landed at the configured (hardware-capped) level");

    gl.glDeleteTextures(1, &tex);

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
