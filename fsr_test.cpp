// Creates a real OpenGL context and runs ApplyFsr() directly against src/dst textures.
//
// Two properties are checked, because FSR 1 is two passes with different failure modes:
//   1. On a flat (edgeless) image, EASU has nothing to reconstruct and RCAS has no contrast to
//      sharpen, so the pipeline must reproduce the color. This catches gross errors in the
//      constants, the tap offsets or the weight normalization - a broken kernel shows up
//      immediately as a wrong or black center pixel.
//   2. On a hard edge, RCAS must actually change something near it while leaving the far-field
//      flat regions alone. That distinguishes "ran and did nothing" from "ran and sharpened".
// See gl_loader_test.cpp for why <windows.h> is safe here and for the window/context pattern.
#include <windows.h>
#include <cstdio>
#include <cstdlib>

#include "fsr.h"
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

// Reads back the whole dst texture as RGBA8.
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
    wc.lpszClassName = "AnaxFsrTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "fsr_test", WS_OVERLAPPEDWINDOW,
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

    const int width = 64;
    const int height = 64;
    const size_t texelCount = (size_t)width * (size_t)height;

    // --- 1. Flat image: FSR must reproduce it. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);

        unsigned char* flat = new unsigned char[texelCount * 4];
        for (size_t i = 0; i < texelCount; ++i) {
            flat[i * 4 + 0] = 128;
            flat[i * 4 + 1] = 64;
            flat[i * 4 + 2] = 192;
            flat[i * 4 + 3] = 255;
        }
        UploadRgba(gl, srcTex, width, height, flat);
        delete[] flat;

        bool wrote = ApplyFsr(srcTex, dstTex, width, height, 1.0f, 0.75f);
        Check(wrote, "flat image: ApplyFsr() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        unsigned int err = gl.glGetError();
        Check(err == GL_NO_ERROR, "flat image: no GL error after the FSR passes and readback");

        size_t center = ((size_t)(height / 2) * width + (width / 2)) * 4;
        printf("  flat center: r=%d g=%d b=%d (expected ~128/64/192)\n",
               out[center + 0], out[center + 1], out[center + 2]);
        bool reproduced = abs((int)out[center + 0] - 128) <= 6 &&
                          abs((int)out[center + 1] - 64)  <= 6 &&
                          abs((int)out[center + 2] - 192) <= 6;
        Check(reproduced, "flat image: FSR reproduced an edgeless image without altering it");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // --- 2. Hard edge: RCAS must respond near it, and leave the far field alone. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);

        // A vertical edge down the middle, deliberately NOT black-to-white: RCAS's limiters
        // evaluate its lobe to exactly zero on a saturated 0<->255 step (min(mn4,e)/(4*mx4) and
        // (1-max(mx4,e))/(4*mn4-4) both vanish), because sharpening there could only overshoot
        // outside [0,1]. That is correct "robust" behavior, so a saturated edge would test
        // nothing. 64 <-> 192 leaves headroom on both sides, so the lobe is non-zero.
        const unsigned char kDark = 64;
        const unsigned char kLight = 192;
        unsigned char* edge = new unsigned char[texelCount * 4];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t i = ((size_t)y * width + x) * 4;
                unsigned char v = (x < width / 2) ? kDark : kLight;
                edge[i + 0] = v;
                edge[i + 1] = v;
                edge[i + 2] = v;
                edge[i + 3] = 255;
            }
        }
        UploadRgba(gl, srcTex, width, height, edge);

        bool wrote = ApplyFsr(srcTex, dstTex, width, height, 1.0f, 1.0f);
        Check(wrote, "edge image: ApplyFsr() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        unsigned int err = gl.glGetError();
        Check(err == GL_NO_ERROR, "edge image: no GL error after the FSR passes and readback");

        // Far field must survive: deep in either half keeps that half's value.
        int row = height / 2;
        size_t farDark = ((size_t)row * width + 4) * 4;
        size_t farLight = ((size_t)row * width + (width - 5)) * 4;
        printf("  edge far field: dark side r=%d (expected ~%d), light side r=%d (expected ~%d)\n",
               out[farDark], kDark, out[farLight], kLight);
        Check(abs((int)out[farDark] - kDark) <= 6 && abs((int)out[farLight] - kLight) <= 6,
              "edge image: flat regions away from the edge are left alone");

        // Something within a few texels of the edge must differ from the input step, which is
        // what sharpening/reconstruction actually doing work looks like.
        int maxDelta = 0;
        for (int x = width / 2 - 3; x <= width / 2 + 2; ++x) {
            size_t i = ((size_t)row * width + x) * 4;
            int expected = (x < width / 2) ? kDark : kLight;
            int delta = abs((int)out[i] - expected);
            if (delta > maxDelta) {
                maxDelta = delta;
            }
        }
        printf("  edge: largest change within 3 texels of the edge = %d\n", maxDelta);
        Check(maxDelta > 3, "edge image: FSR altered pixels near the edge (the pass did work)");

        delete[] edge;
        delete[] out;
        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // --- 3. Reduced scale must still run cleanly (EASU genuinely upscaling). ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);
        unsigned char* flat = new unsigned char[texelCount * 4];
        for (size_t i = 0; i < texelCount; ++i) {
            flat[i * 4 + 0] = 200; flat[i * 4 + 1] = 200;
            flat[i * 4 + 2] = 200; flat[i * 4 + 3] = 255;
        }
        UploadRgba(gl, srcTex, width, height, flat);
        delete[] flat;

        bool wrote = ApplyFsr(srcTex, dstTex, width, height, 0.5f, 0.5f);
        unsigned int err = gl.glGetError();
        Check(wrote && err == GL_NO_ERROR, "scale=0.5: EASU upscaling path ran with no GL error");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        size_t center = ((size_t)(height / 2) * width + (width / 2)) * 4;
        printf("  scale=0.5 center: r=%d (expected ~200)\n", out[center]);
        Check(abs((int)out[center] - 200) <= 8,
              "scale=0.5: a flat image survives the upscale path intact");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

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
