// Creates a real OpenGL context, clears the back buffer to a known color, captures it into a
// src RGBA16F texture (mimicking what post_effects.cpp's shared pipeline now does once per
// frame), and runs both ApplyNVScaler and ApplyNVSharpen against src/dst textures, checking
// for GL errors and a crash-free run - see gl_loader_test.cpp's header comment for why
// <windows.h> is safe to use here (a standalone .exe, not linked into the proxy DLL). This
// does not assert the output pixel color against NIS's exact expected output: NVScaler/
// NVSharpen's edge-adaptive sharpening intentionally changes pixel values near "edges"
// (including the clear color's boundary with whatever undefined memory was in the newly-
// created window), so a flat clear color is not a meaningful precise correctness oracle for
// this shader. It does, however, assert the center pixel is not fully black after each call -
// a weak but load-bearing check that catches the pipeline silently producing no output (e.g.
// a texture-unit binding mismatch between the C++ dispatch code and the shaders'
// layout(binding=N) declarations), which a plain glGetError()-only check cannot detect.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "nis_effect.h"

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
const unsigned int GL_RGBA               = 0x1908;
const unsigned int GL_UNSIGNED_BYTE      = 0x1401;

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

typedef bool (*ApplyFn)(unsigned int, unsigned int, int, int, float);

bool RunOnce(ApplyFn applyFn, const char* name, unsigned int srcTex, unsigned int dstTex,
             int width, int height, unsigned int readFbo) {
    bool wrote = applyFn(srcTex, dstTex, width, height, 0.5f);
    const GlComputeApi& gl = GetGlComputeApi();
    unsigned int err = gl.glGetError();
    if (!wrote || err != 0) {
        printf("FAIL: %s left glGetError() = 0x%04X (wrote=%d)\n", name, err, wrote ? 1 : 0);
        return false;
    }

    // Weak pixel check: this doesn't try to model NIS's exact output, it just catches "the
    // whole pipeline silently no-op'd into black" (exactly how the binding-mismatch bug that
    // made both effects render black slipped past a plain glGetError()-only check). The
    // source texture is captured from a window cleared to a non-black color (0.8, 0.2, 0.1)
    // before any effect runs, so a fully-black readback at the center pixel means the
    // effect's output never made it into dstTex.
    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
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
    const unsigned int GL_BACK = 0x0405;
    pGlViewport(0, 0, 128, 128);
    pGlClearColor(0.8f, 0.2f, 0.1f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);

    const GlComputeApi& gl = GetGlComputeApi();
    int width = 128, height = 128;
    unsigned int srcTex = CreatePipelineTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    unsigned int dstTex = CreatePipelineTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    bool ok = true;
    ok = RunOnce(&ApplyNVScaler, "ApplyNVScaler", srcTex, dstTex, width, height, readFbo) && ok;
    ok = RunOnce(&ApplyNVSharpen, "ApplyNVSharpen", srcTex, dstTex, width, height, readFbo) && ok;
    // Run each a second time to exercise EnsureStaticResources' reuse (not-first-call) path,
    // same as bilinear_upscale_test.cpp does for ApplyBilinearUpscale.
    ok = RunOnce(&ApplyNVScaler, "ApplyNVScaler (second call)", srcTex, dstTex, width, height, readFbo) && ok;
    ok = RunOnce(&ApplyNVSharpen, "ApplyNVSharpen (second call)", srcTex, dstTex, width, height, readFbo) && ok;

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
