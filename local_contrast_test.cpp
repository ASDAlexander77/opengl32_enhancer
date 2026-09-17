// Creates a real OpenGL context and runs ApplyLocalContrast() directly against src/dst
// textures. See gl_loader_test.cpp for why <windows.h> is safe here and for the window/context
// pattern.
//
// Three properties, all built from ONE fixture (a single hard edge: a dark block next to a
// light block) so the structure/tone comparison is apples-to-apples:
//   1. A flat (edgeless) image has no detail at either scale, so both layers are zero and the
//      output must reproduce the input exactly - at strong, nonzero strengths.
//   2. Both strengths at 0.0 must be an exact passthrough on the EDGE fixture too - proves the
//      strength gate itself, independent of what the blur passes compute.
//   3. The edge fixture with toneStrength>0 (structureStrength=0) must shift a point 15 texels
//      from the edge - inside the tone layer's blur reach (4 taps * radiusScale=8 = 32 texels)
//      but well outside the structure layer's (4*2=8 texels) - in the classic unsharp/"clarity"
//      overshoot direction: darker on the dark side. The SAME point with structureStrength>0
//      (toneStrength=0) instead must come back essentially unchanged, since it's outside that
//      layer's reach - directly demonstrating the two controls act at different, independent
//      spatial scales, which is the whole point of having both.
#include <windows.h>
#include <cstdio>
#include <cstdlib>

#include "local_contrast.h"
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

void UploadRgba(const GlComputeApi& gl, unsigned int tex, int width, int height,
                 const unsigned char* pixels) {
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void ReadBack(const GlComputeApi& gl, unsigned int tex, int width, int height,
              unsigned char* out) {
    unsigned int fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    gl.glReadBuffer(GL_COLOR_ATTACHMENT0);
    gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out);
    gl.glDeleteFramebuffers(1, &fbo);
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxLocalContrastTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "local_contrast_test", WS_OVERLAPPEDWINDOW,
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

    const GlComputeApi& gl = GetGlComputeApi();
    if (!Check(gl.loaded, "GetGlComputeApi() resolved on this context")) {
        return 1;
    }

    const int width = 96;
    const int height = 32;
    const size_t texelCount = (size_t)width * (size_t)height;

    // --- 1. Flat image: local contrast must reproduce it, at strong parameters. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);

        unsigned char* flat = new unsigned char[texelCount * 4];
        for (size_t i = 0; i < texelCount; ++i) {
            flat[i * 4 + 0] = 90; flat[i * 4 + 1] = 130;
            flat[i * 4 + 2] = 170; flat[i * 4 + 3] = 255;
        }
        UploadRgba(gl, srcTex, width, height, flat);
        delete[] flat;

        bool wrote = ApplyLocalContrast(srcTex, dstTex, width, height,
                                         /*structureStrength=*/1.5f, /*toneStrength=*/1.5f);
        Check(wrote, "flat image: ApplyLocalContrast() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "flat image: no GL error after local contrast and readback");

        size_t center = ((size_t)(height / 2) * width + (width / 2)) * 4;
        printf("  flat center: r=%d g=%d b=%d (expected ~90/130/170)\n",
               out[center + 0], out[center + 1], out[center + 2]);
        Check(abs((int)out[center + 0] - 90) <= 2 &&
              abs((int)out[center + 1] - 130) <= 2 &&
              abs((int)out[center + 2] - 170) <= 2,
              "flat image: local contrast reproduced an edgeless image at strong strengths");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // Shared edge fixture for tests 2-4: a single hard vertical edge at x=48, dark (64) on the
    // left, light (192) on the right. The probe point sits at x=33 - 15 texels from the edge:
    // inside the tone layer's ~32-texel blur reach, outside the structure layer's ~8-texel one.
    const unsigned char kDark = 64;
    const unsigned char kLight = 192;
    const int edgeX = 48;
    const int probeX = 33;
    unsigned char* edge = new unsigned char[texelCount * 4];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            unsigned char v = (x < edgeX) ? kDark : kLight;
            edge[i + 0] = v; edge[i + 1] = v; edge[i + 2] = v; edge[i + 3] = 255;
        }
    }
    int row = height / 2;
    size_t probeOffset = ((size_t)row * width + probeX) * 4;

    // --- 2. Both strengths 0.0 on the edge: exact passthrough. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTex, width, height, edge);

        bool wrote = ApplyLocalContrast(srcTex, dstTex, width, height,
                                         /*structureStrength=*/0.0f, /*toneStrength=*/0.0f);
        Check(wrote, "strengths=0: ApplyLocalContrast() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "strengths=0: no GL error after local contrast and readback");

        printf("  strengths=0 probe: r=%d (expected %d)\n", out[probeOffset], kDark);
        Check(abs((int)out[probeOffset] - kDark) <= 1, "strengths=0: edge fixture passed through unchanged");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // --- 3. toneStrength>0 only: the distant probe point must darken (unsharp overshoot). ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTex, width, height, edge);

        bool wrote = ApplyLocalContrast(srcTex, dstTex, width, height,
                                         /*structureStrength=*/0.0f, /*toneStrength=*/1.0f);
        Check(wrote, "toneStrength=1.0: ApplyLocalContrast() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "toneStrength=1.0: no GL error after local contrast and readback");

        printf("  toneStrength=1.0 probe (15 texels from edge): r=%d (expected below %d)\n",
               out[probeOffset], kDark);
        Check(out[probeOffset] < kDark, "toneStrength=1.0: distant probe point darkened (tone layer reached it)");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // --- 4. structureStrength>0 only: the SAME distant probe point must stay unchanged. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTex, width, height, edge);

        bool wrote = ApplyLocalContrast(srcTex, dstTex, width, height,
                                         /*structureStrength=*/1.0f, /*toneStrength=*/0.0f);
        Check(wrote, "structureStrength=1.0: ApplyLocalContrast() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "structureStrength=1.0: no GL error after local contrast and readback");

        printf("  structureStrength=1.0 probe (15 texels from edge): r=%d (expected ~%d, "
               "outside its ~8-texel reach)\n", out[probeOffset], kDark);
        Check(abs((int)out[probeOffset] - kDark) <= 1,
              "structureStrength=1.0: distant probe point unchanged (structure layer didn't reach it)");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    delete[] edge;

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
