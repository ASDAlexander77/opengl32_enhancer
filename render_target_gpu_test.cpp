// Creates a real OpenGL context and checks that render_target.h's offscreen framebuffer is
// actually created, complete, sized as configured, and captures the game's drawing instead of
// the real back buffer. render_target_test.cpp already pins the scaling/arming rules on the
// CPU; what can only be checked against a driver is the other half: that EnsureRenderTarget()
// produces a real, complete FBO and that BindRenderTarget()/GetGameFramebuffer()/
// GetGameReadBuffer() actually route drawing into it. Reads opengl32_enhancer.ini next to this
// .exe like config.cpp always does - CMakeLists.txt copies render_target_test.ini there
// (renamed), a fixture with renderWidth=1280/renderHeight=960.
#include <windows.h>
#include <GL/gl.h>
#include <cstdio>

#include "config.h"
#include "gl_loader.h"
#include "render_target.h"

namespace {

// Not in <GL/gl.h> - framebuffer objects are a later extension than the GL 1.1 headers Windows
// ships. Everything else this file touches (GL_TEXTURE_2D, GL_BACK, GL_MAX_TEXTURE_SIZE,
// GL_RGBA, GL_UNSIGNED_BYTE, GL_COLOR_BUFFER_BIT, ...) already comes from <GL/gl.h>.
const unsigned int GL_FRAMEBUFFER               = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER          = 0x8CA8;
const unsigned int GL_COLOR_ATTACHMENT0         = 0x8CE0;
const unsigned int GL_DRAW_FRAMEBUFFER_BINDING  = 0x8CA6;
// GL_TEXTURE_BINDING_2D is already in <GL/gl.h> (core GL 1.1) - not redefined here.

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxRenderTargetTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "render_target_gpu_test",
                                WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                                wc.hInstance, nullptr);
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
    if (!Check(GetAnaxConfig().renderWidth == 1280 && GetAnaxConfig().renderHeight == 960,
               "the fixture ini configured a 1280x960 render target")) {
        return 1;
    }

    // --- the framebuffer is created, complete, and armed ---
    {
        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, 640, 480);
        bool ready = EnsureRenderTarget();
        ArmSupersampleForFrame(ready);
        Check(ready, "EnsureRenderTarget() succeeded");
        Check(IsSupersampleActive(), "the frame armed once the target was ready");
        Check(GetGameFramebuffer() != 0, "the game's framebuffer is the offscreen one");
        Check(GetGameReadBuffer() == GL_COLOR_ATTACHMENT0,
              "the read buffer is the colour attachment, not GL_BACK");
        int w = 0, h = 0;
        GetRenderTargetSize(w, h);
        Check(w == 1280 && h == 960, "the target is the configured size");
    }

    // --- EnsureRenderTarget's rebuild path does not clobber the game's texture binding ---
    // Ruling 11's fail-safe. This runs inside the game's frame, from the swap hook, after
    // post_effects.cpp has already restored its own state - so anything left bound here is what
    // the game's next frame starts with. Uses a size different from the fixture's 1280x960 so
    // the "already built" fast path is skipped and the actual glGenTextures/glBindTexture
    // rebuild sequence runs.
    {
        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, 640, 480);
        GetMutableAnaxConfig().renderWidth = 800;
        GetMutableAnaxConfig().renderHeight = 600;

        unsigned int gameTex = 0;
        gl.glGenTextures(1, &gameTex);
        gl.glBindTexture(GL_TEXTURE_2D, gameTex);

        bool ready = EnsureRenderTarget();
        Check(ready, "the rebuild for the binding-restore case succeeded");

        int boundAfter = 0;
        gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundAfter);
        Check((unsigned int)boundAfter == gameTex,
              "EnsureRenderTarget restores the game's GL_TEXTURE_2D binding after rebuilding");

        gl.glDeleteTextures(1, &gameTex);
        GetMutableAnaxConfig().renderWidth = 1280;
        GetMutableAnaxConfig().renderHeight = 960;
    }

    // --- drawing lands in the offscreen buffer and NOT in the back buffer ---
    {
        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, 640, 480);
        ArmSupersampleForFrame(EnsureRenderTarget());

        // Paint the real back buffer black first, so a green reading from it later could only
        // mean the draw leaked out of the offscreen target.
        gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        BindRenderTarget();
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        unsigned char offscreen[4] = {0, 0, 0, 0};
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, GetGameFramebuffer());
        gl.glReadBuffer(GetGameReadBuffer());
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, offscreen);

        unsigned char backBuffer[4] = {0, 0, 0, 0};
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.glReadBuffer(GL_BACK);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, backBuffer);

        Check(offscreen[1] > 200, "the draw landed in the offscreen target");
        Check(backBuffer[1] < 50, "the draw did NOT reach the real back buffer");
    }

    // --- BindRenderTarget does nothing when the frame is not armed ---
    // The fail-safe. Without it an unarmed frame would still bind the offscreen target, and the
    // game would render into a buffer nothing presents - a black window rather than a degraded
    // one, which is the opposite of this feature's stated failure direction.
    {
        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, 640, 480);
        EnsureRenderTarget();
        ArmSupersampleForFrame(false);          // target may be fine; the frame is NOT armed

        gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        BindRenderTarget();

        int boundAfter = -1;
        gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundAfter);
        Check(boundAfter == 0,
              "BindRenderTarget leaves the binding alone when the frame is not armed");
    }

    // --- a size the driver cannot allocate degrades to today's rendering ---
    {
        ResetRenderTargetState();
        int maxTextureSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        GetMutableAnaxConfig().renderWidth = maxTextureSize + 1024;
        GetMutableAnaxConfig().renderHeight = maxTextureSize + 1024;
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, 640, 480);
        bool ready = EnsureRenderTarget();
        ArmSupersampleForFrame(ready);
        Check(!ready, "an impossible size is refused");
        Check(!IsSupersampleActive(), "and nothing is armed");
        Check(GetGameFramebuffer() == 0, "so the game keeps rendering into framebuffer 0");
        Check(GetGameReadBuffer() == GL_BACK, "and the read buffer is GL_BACK again");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
