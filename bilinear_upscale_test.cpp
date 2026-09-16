// Creates a real OpenGL context, clears the back buffer to a known solid color, captures it
// into a src RGBA16F texture (mimicking what post_effects.cpp's shared pipeline now does
// once per frame - see post_effects.cpp's header comment), runs ApplyBilinearUpscale()
// directly against src/dst textures, and reads back dst to confirm the pipeline reproduced
// the color without a GL error. See gl_loader_test.cpp for why <windows.h> is safe here and
// for the window/context creation pattern reused below.
#include <windows.h>
#include <cmath>
#include <cstdio>

#include "bilinear_upscale.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_VIEWPORT           = 0x0BA2;
const unsigned int GL_BACK               = 0x0405;
const unsigned int GL_COLOR_BUFFER_BIT   = 0x00004000;
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
const unsigned int GL_READ_FRAMEBUFFER   = 0x8CA8;
const unsigned int GL_COLOR_ATTACHMENT0  = 0x8CE0;

typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float r, float g, float b, float a);
typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int mask);
typedef void (__stdcall *PFNGLREADPIXELSPROC)(int x, int y, int width, int height,
    unsigned int format, unsigned int type, void* pixels);

template <typename T>
T Resolve(const char* name) {
    return (T)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
}

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    return condition;
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
    wc.lpszClassName = "AnaxBilinearUpscaleTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "bilinear_upscale_test", WS_OVERLAPPEDWINDOW,
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

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || !wglMakeCurrent(hdc, hglrc)) {
        printf("FAIL: wglCreateContext/wglMakeCurrent, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    bool ok = true;

    PFNGLCLEARCOLORPROC pGlClearColor = Resolve<PFNGLCLEARCOLORPROC>("glClearColor");
    PFNGLCLEARPROC pGlClear = Resolve<PFNGLCLEARPROC>("glClear");
    PFNGLREADPIXELSPROC pGlReadPixels = Resolve<PFNGLREADPIXELSPROC>("glReadPixels");
    ok &= Check(pGlClearColor && pGlClear && pGlReadPixels,
                "resolved glClearColor/glClear/glReadPixels");

    const GlComputeApi& gl = GetGlComputeApi();
    ok &= Check(gl.loaded, "GetGlComputeApi() resolved on this context");

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    ok &= Check(width > 0 && height > 0, "context reports a nonzero viewport");
    printf("Viewport: %dx%d\n", width, height);

    // Clear to a known solid color and capture it into srcTex, mimicking post_effects.cpp's
    // one-time-per-frame capture.
    pGlClearColor(0.8f, 0.2f, 0.1f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);
    unsigned int srcTex = CreatePipelineTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    unsigned int dstTex = CreatePipelineTexture(gl, width, height);

    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    // Run the pipeline twice (the second call also exercises the "already compiled" reuse
    // path), then read back dstTex via readFbo.
    bool wrote1 = ApplyBilinearUpscale(srcTex, dstTex, width, height);
    unsigned int errAfterFirst = gl.glGetError();
    ok &= Check(wrote1 && errAfterFirst == 0, "no GL error after first ApplyBilinearUpscale() call");

    bool wrote2 = ApplyBilinearUpscale(srcTex, dstTex, width, height);
    unsigned int errAfterSecond = gl.glGetError();
    ok &= Check(wrote2 && errAfterSecond == 0,
                "no GL error after second ApplyBilinearUpscale() call (same size, texture reuse)");

    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
    unsigned char pixel[4] = {0, 0, 0, 0};
    pGlReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    printf("Center pixel after bilinear pass: r=%d g=%d b=%d a=%d\n",
           pixel[0], pixel[1], pixel[2], pixel[3]);
    // Expect close to (204, 51, 26) = (0.8, 0.2, 0.1) in 8-bit. Generous tolerance: this
    // isn't testing color precision, just that the pipeline reproduced approximately the
    // right color instead of corrupting or zeroing it.
    bool colorPlausible = std::abs((int)pixel[0] - 204) < 20
                        && std::abs((int)pixel[1] - 51) < 20
                        && std::abs((int)pixel[2] - 26) < 20;
    ok &= Check(colorPlausible, "bilinear pass reproduced the cleared color at scale 1.0");

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("\n%s\n", ok ? "All checks passed." : "Some checks FAILED.");
    return ok ? 0 : 1;
}
