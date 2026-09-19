// Creates a real OpenGL context and drives the glFrustum/glOrtho sequence a Quake II-family
// engine makes, checking that the world-only frame latches exactly once per frame and never
// survives into the next one - see world_capture.h. Window/context boilerplate modeled on
// modelview_capture_test.cpp; reading a texture back through an FBO modeled on
// nis_effect_test.cpp's RunOnce.
#include <windows.h>
#include <cstdio>

#include "config.h"
#include "gl_loader.h"
#include "world_capture.h"

namespace {

const int kWidth = 128;
const int kHeight = 128;

const unsigned int GL_COLOR_BUFFER_BIT  = 0x00004000;
const unsigned int GL_TEXTURE_2D        = 0x0DE1;
const unsigned int GL_FRAMEBUFFER       = 0x8D40;
const unsigned int GL_COLOR_ATTACHMENT0 = 0x8CE0;
const unsigned int GL_RGBA              = 0x1908;
const unsigned int GL_UNSIGNED_BYTE     = 0x1401;

typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float, float, float, float);
typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int);
typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);

PFNGLCLEARCOLORPROC pGlClearColor = nullptr;
PFNGLCLEARPROC      pGlClear      = nullptr;
PFNGLVIEWPORTPROC   pGlViewport   = nullptr;

unsigned int g_readFbo = 0;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

// glClearColor/glClear resolved from the real opengl32.dll, the way nis_effect_test.cpp does -
// this is the game's own fixed-function GL state being driven, not the GL 4.3 compute API
// gl_loader.h resolves.
void ClearBackBufferTo(float r, float g, float b) {
    pGlClearColor(r, g, b, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);
}

// Attaches `tex` to an FBO and reads the center pixel back - the same shape as
// nis_effect_test.cpp's RunOnce reading its dst texture. Proves what the LATCHED TEXTURE
// actually holds, not just that a handle came back.
void ReadCenterPixelOf(unsigned int tex, unsigned char outPixel[4]) {
    const GlComputeApi& gl = GetGlComputeApi();
    gl.glBindFramebuffer(GL_FRAMEBUFFER, g_readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    gl.glReadPixels(kWidth / 2, kHeight / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outPixel);
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxWorldCaptureTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "world_capture_test", WS_OVERLAPPEDWINDOW,
        0, 0, kWidth, kHeight, nullptr, nullptr, wc.hInstance, nullptr);
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

    HMODULE realGl = GetModuleHandleA("opengl32.dll");
    pGlClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(realGl, "glClearColor");
    pGlClear      = (PFNGLCLEARPROC)GetProcAddress(realGl, "glClear");
    pGlViewport   = (PFNGLVIEWPORTPROC)GetProcAddress(realGl, "glViewport");
    if (pGlClearColor == nullptr || pGlClear == nullptr || pGlViewport == nullptr) {
        printf("FAIL: could not resolve glClearColor/glClear/glViewport\n");
        return 1;
    }

    bool ok = true;

    // LatchWorldFrame() now gates on motionblur actually being listed (see world_capture.h/.cpp)
    // - a config with no stages at all, like GetMutableAnaxConfig()'s default, would make every
    // case below fail before it ever reached the GL calls it means to exercise. Rather than
    // shipping an ini fixture just for this test, GetMutableAnaxConfig() exists precisely for a
    // non-DLL caller like this one to set the config directly (see its comment in config.h) - so
    // set it here, once, before any case runs. Restored to "no motionblur" for the dedicated gate
    // case near the end, then put back so nothing after it is affected.
    AnaxConfig& config = GetMutableAnaxConfig();
    config.stages[0] = EffectKind::MotionBlur;
    config.stageCount = 1;

    // --- Case 1: nothing captured before any call. MUST RUN FIRST: the module keeps
    // process-global state and exports no test-only reset, so this is the only point at which
    // "not captured yet" can be observed.
    {
        unsigned int tex = 123; int w = 0, h = 0;
        ok = Check(!GetWorldOnlyFrame(tex, w, h),
                   "no world frame before any pass has begun") && ok;
    }

    // world_capture.h's contract says nothing about the viewport - LatchWorldFrame reads it via
    // glGetIntegerv(GL_VIEWPORT), and a freshly created GL window does not default it to the
    // window size on every driver - so the test sets it explicitly before anything can latch,
    // the way nis_effect_test.cpp does for its own GL state.
    pGlViewport(0, 0, kWidth, kHeight);

    const GlComputeApi& gl = GetGlComputeApi();
    gl.glGenFramebuffers(1, &g_readFbo);

    // Before Case 2: red, so Case 5's pixel assertion has something to distinguish from the
    // green written later.
    ClearBackBufferTo(1.0f, 0.0f, 0.0f);

    // --- Case 2: glFrustum then glOrtho latches.
    {
        NotifyWorldPassBegan();
        NotifyTwoDPassBegan();
        unsigned int tex = 0; int w = 0, h = 0;
        ok = Check(GetWorldOnlyFrame(tex, w, h) && tex != 0 && w == kWidth && h == kHeight,
                   "glFrustum then glOrtho latches a world frame at viewport size") && ok;
    }

    // --- Case 3: invalidation at swap makes it stale-free.
    {
        InvalidateWorldFrame();
        unsigned int tex = 0; int w = 0, h = 0;
        ok = Check(!GetWorldOnlyFrame(tex, w, h),
                   "the world frame does not survive into the next frame") && ok;
    }

    // --- Case 4: glOrtho with no preceding glFrustum does not latch. This is the case that
    // stops a menu-only frame - which never calls glFrustum - from being treated as a world
    // pass.
    {
        NotifyTwoDPassBegan();
        unsigned int tex = 0; int w = 0, h = 0;
        ok = Check(!GetWorldOnlyFrame(tex, w, h),
                   "glOrtho alone does not latch - the arm is load-bearing") && ok;
        InvalidateWorldFrame();
    }

    // --- Case 5: a second glOrtho in the same frame does not re-latch. A Quake II frame issues
    // several ortho passes; only the first one happens before the HUD.
    {
        NotifyWorldPassBegan();
        NotifyTwoDPassBegan();
        unsigned int firstTex = 0; int w = 0, h = 0;
        GetWorldOnlyFrame(firstTex, w, h);

        // Change the back buffer, then fire a second glOrtho. If it re-latched, the texture's
        // contents would follow - so assert against the PIXELS, not just the handle, since the
        // module legitimately reuses one texture object across frames.
        ClearBackBufferTo(0.0f, 1.0f, 0.0f);   // green; the first latch captured red
        NotifyTwoDPassBegan();

        unsigned char px[4] = {0, 0, 0, 0};
        ReadCenterPixelOf(firstTex, px);
        ok = Check(px[0] > 200 && px[1] < 60,
                   "a second glOrtho in the same frame does not re-latch") && ok;
        InvalidateWorldFrame();
    }

    // --- Case 6: with motionblur NOT in the stage list, LatchWorldFrame() returns before
    // touching GL at all, so no world frame ever latches even with the exact same glFrustum/
    // glOrtho sequence that latched one in Case 2. This is CRITICAL 1's gate - see
    // world_capture.h/.cpp.
    {
        config.stageCount = 0;
        NotifyWorldPassBegan();
        NotifyTwoDPassBegan();
        unsigned int tex = 0; int w = 0, h = 0;
        ok = Check(!GetWorldOnlyFrame(tex, w, h),
                   "no motionblur stage listed means no world frame latches") && ok;
        InvalidateWorldFrame();

        // Restored so nothing after this point (there is nothing today, but a later case added
        // here should not have to rediscover this) runs against a config with no stages.
        config.stages[0] = EffectKind::MotionBlur;
        config.stageCount = 1;
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    if (ok) {
        printf("ALL PASS\n");
    } else {
        printf("FAILURE(S)\n");
    }
    return ok ? 0 : 1;
}
