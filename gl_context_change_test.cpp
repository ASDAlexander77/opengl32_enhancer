// Regression test for GL context invalidation (see GetGlContextGeneration() in gl_loader.h).
//
// Every effect module caches its compiled program and GL objects in a process-global. Those
// names belong to the context that created them, so when an app destroys its GL context and
// makes a new one - which Quake II era engines do on a `vid_restart` / video mode change - the
// cached handles are invalid in the new context. Before the generation counter, the modules
// still reported initOk and kept using them, which renders as garbage.
//
// This test drives exactly that sequence: run an effect in one context, destroy it, create a
// second context, and run the same effect again. The second run must rebuild and still produce
// a correct image. Uses ApplyInvert because it is the simplest pass (no config UBO, no scratch
// textures) and its output is trivially checkable. Window/context creation follows the same
// pattern as gl_loader_test.cpp / bilinear_upscale_test.cpp.
#include <windows.h>
#include <cstdio>

#include "pixel_invert.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_RGBA               = 0x1908;
const unsigned int GL_UNSIGNED_BYTE      = 0x1401;
const unsigned int GL_TEXTURE_2D         = 0x0DE1;
const unsigned int GL_RGBA16F            = 0x881A;
const unsigned int GL_TEXTURE_MIN_FILTER = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S     = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T     = 0x2803;
const unsigned int GL_LINEAR             = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE      = 0x812F;
const unsigned int GL_FRAMEBUFFER        = 0x8D40;
const unsigned int GL_COLOR_ATTACHMENT0  = 0x8CE0;
const unsigned int GL_NO_ERROR           = 0;

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

unsigned int MakeTexture(const GlComputeApi& gl, int width, int height) {
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

// Fills src with a known color, runs ApplyInvert into dst, reads dst back and checks the color
// really was inverted. Returns true only if the pass both ran and produced the right pixels -
// a stale program from a previous context shows up here as a GL error or an unchanged image.
bool RunInvertAndVerify(const GlComputeApi& gl, const char* label) {
    const int width = 64;
    const int height = 64;

    unsigned int srcTex = MakeTexture(gl, width, height);
    unsigned int dstTex = MakeTexture(gl, width, height);

    // Known input: r=1, g=0, b=0. Inverting must give r=0, g=1, b=1.
    const size_t texelCount = (size_t)width * (size_t)height;
    unsigned char* srcPixels = new unsigned char[texelCount * 4];
    for (size_t i = 0; i < texelCount; ++i) {
        srcPixels[i * 4 + 0] = 255;
        srcPixels[i * 4 + 1] = 0;
        srcPixels[i * 4 + 2] = 0;
        srcPixels[i * 4 + 3] = 255;
    }
    gl.glBindTexture(GL_TEXTURE_2D, srcTex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, srcPixels);
    delete[] srcPixels;

    char what[160];
    bool wrote = ApplyInvert(srcTex, dstTex, width, height);
    snprintf(what, sizeof(what), "%s: ApplyInvert() reported it wrote dstTexture", label);
    bool ok = Check(wrote, what);

    unsigned int fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
    gl.glReadBuffer(GL_COLOR_ATTACHMENT0);

    unsigned char center[4] = {0, 0, 0, 0};
    gl.glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center);
    unsigned int err = gl.glGetError();

    snprintf(what, sizeof(what), "%s: no GL error after the invert pass and readback", label);
    ok &= Check(err == GL_NO_ERROR, what);

    printf("  %s: center pixel r=%d g=%d b=%d\n", label, center[0], center[1], center[2]);
    // Generous thresholds: the value round-trips through RGBA16F, so exact 0/255 is not
    // guaranteed - but an inverted red texel is unmistakably distinct from the input.
    bool inverted = center[0] < 64 && center[1] > 192 && center[2] > 192;
    snprintf(what, sizeof(what), "%s: red input came back inverted (cyan), not stale or garbage", label);
    ok &= Check(inverted, what);

    gl.glDeleteFramebuffers(1, &fbo);
    gl.glDeleteTextures(1, &srcTex);
    gl.glDeleteTextures(1, &dstTex);
    return ok;
}

HGLRC CreateAndMakeCurrent(HDC hdc) {
    HGLRC context = wglCreateContext(hdc);
    if (context == nullptr || !wglMakeCurrent(hdc, context)) {
        return nullptr;
    }
    return context;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxGlContextChangeTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "gl_context_change_test", WS_OVERLAPPEDWINDOW,
        0, 0, 256, 256, nullptr, nullptr, wc.hInstance, nullptr);
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

    // --- First context: the effect compiles and caches its program here. ---
    HGLRC firstContext = CreateAndMakeCurrent(hdc);
    if (firstContext == nullptr) {
        printf("FAIL: creating the first GL context, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!Check(gl.loaded, "GetGlComputeApi() resolved on the first context")) {
        printf("\nGL 4.3 compute unavailable - cannot exercise context invalidation.\n");
        return 1;
    }
    unsigned int firstGeneration = GetGlContextGeneration();

    RunInvertAndVerify(gl, "context 1");

    // --- Destroy it and make a completely new one, as a vid_restart would. ---
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(firstContext);

    HGLRC secondContext = CreateAndMakeCurrent(hdc);
    if (secondContext == nullptr) {
        printf("FAIL: creating the second GL context, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    // Re-fetching the API is what notices the switch and advances the generation, which is how
    // every module learns its cached objects are stale.
    const GlComputeApi& gl2 = GetGlComputeApi();
    Check(gl2.loaded, "GetGlComputeApi() re-resolved on the second context");

    unsigned int secondGeneration = GetGlContextGeneration();
    printf("  generation: first=%u second=%u\n", firstGeneration, secondGeneration);
    Check(secondGeneration != firstGeneration,
          "context generation advanced after the context was replaced");

    // The real regression: without invalidation this reuses the destroyed context's program.
    RunInvertAndVerify(gl2, "context 2");

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(secondContext);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
