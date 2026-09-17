// Creates a real OpenGL context and runs ApplyNr() directly against src/dst textures. See
// gl_loader_test.cpp for why <windows.h> is safe here and for the window/context pattern.
//
// Four properties, each isolating one of nr.h's five parameters:
//   1. A flat (edgeless, chroma-less) image has nothing for either the bilateral filter or the
//      combine pass's luma/chroma split to change - this must hold at every parameter
//      combination, not just the defaults, since the combine pass's math should be an identity
//      whenever filtered == original (see nr.cpp's header comment for why that's true by
//      construction, not by luck).
//   2. A grayscale checkerboard-noise pattern (alternating luma, no chroma) must be visibly
//      smoothed - lower per-pixel variance after NR than before - at nrIntensity=1.0.
//   3. The same noisy image with nrTonePreservation=1.0 must come back essentially unchanged:
//      tonePreservation=1 disables NR on luma specifically, and this image has no chroma for
//      colorStrength to act on instead, so the whole pipeline should be a near no-op.
//   4. A colored checkerboard-noise pattern (alternating CHROMA, constant luma) must show the
//      opposite split: nrColorStrength=0 leaves the noisy chroma untouched, while
//      nrColorStrength=1 on the same input visibly smooths it.
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "nr.h"
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

// Standard deviation of channel `channel` over a row window [xStart, xEnd) at row `y`.
double RowStdDev(const unsigned char* pixels, int width, int y, int xStart, int xEnd, int channel) {
    int count = xEnd - xStart;
    double sum = 0.0;
    for (int x = xStart; x < xEnd; ++x) {
        sum += pixels[((size_t)y * width + x) * 4 + channel];
    }
    double mean = sum / count;
    double variance = 0.0;
    for (int x = xStart; x < xEnd; ++x) {
        double d = pixels[((size_t)y * width + x) * 4 + channel] - mean;
        variance += d * d;
    }
    return sqrt(variance / count);
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxNrTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "nr_test", WS_OVERLAPPEDWINDOW,
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

    // --- 1. Flat image: NR must reproduce it, at strong (not default) parameters. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);

        unsigned char* flat = new unsigned char[texelCount * 4];
        for (size_t i = 0; i < texelCount; ++i) {
            flat[i * 4 + 0] = 100; flat[i * 4 + 1] = 150;
            flat[i * 4 + 2] = 200; flat[i * 4 + 3] = 255;
        }
        UploadRgba(gl, srcTex, width, height, flat);
        delete[] flat;

        bool wrote = ApplyNr(srcTex, dstTex, width, height, /*intensity=*/1.0f, /*passes=*/3,
                              /*colorStrength=*/1.0f, /*tonePreservation=*/0.3f,
                              /*grainPreservation=*/0.5f);
        Check(wrote, "flat image: ApplyNr() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "flat image: no GL error after NR and readback");

        size_t center = ((size_t)(height / 2) * width + (width / 2)) * 4;
        printf("  flat center: r=%d g=%d b=%d (expected ~100/150/200)\n",
               out[center + 0], out[center + 1], out[center + 2]);
        Check(abs((int)out[center + 0] - 100) <= 3 &&
              abs((int)out[center + 1] - 150) <= 3 &&
              abs((int)out[center + 2] - 200) <= 3,
              "flat image: NR reproduced an edgeless image at strong parameters");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // Shared fixture for tests 2-4: a grayscale checkerboard alternating +-24 around a base of
    // 128 - simulates dithered/high-frequency noise a bilateral filter should treat as "the
    // same surface" and smooth, without any hard edge to preserve.
    unsigned char* noisyGray = new unsigned char[texelCount * 4];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            unsigned char v = ((x + y) % 2 == 0) ? 152 : 104;
            noisyGray[i + 0] = v; noisyGray[i + 1] = v; noisyGray[i + 2] = v; noisyGray[i + 3] = 255;
        }
    }

    // --- 2. Grayscale noise: default color/tone parameters must visibly smooth it. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTex, width, height, noisyGray);

        bool wrote = ApplyNr(srcTex, dstTex, width, height, /*intensity=*/1.0f, /*passes=*/2,
                              /*colorStrength=*/1.0f, /*tonePreservation=*/0.0f,
                              /*grainPreservation=*/0.0f);
        Check(wrote, "noisy image: ApplyNr() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "noisy image: no GL error after NR and readback");

        double inputStdDev = RowStdDev(noisyGray, width, height / 2, 8, width - 8, 0);
        double outputStdDev = RowStdDev(out, width, height / 2, 8, width - 8, 0);
        printf("  noisy row stddev: input=%.2f, output=%.2f (expect output well below input)\n",
               inputStdDev, outputStdDev);
        Check(outputStdDev < inputStdDev * 0.5, "noisy image: NR reduced per-pixel variance by more than half");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }

    // --- 3. Same noisy image, tonePreservation=1.0: luma must be left essentially untouched. ---
    {
        unsigned int srcTex = MakeTexture(gl, width, height);
        unsigned int dstTex = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTex, width, height, noisyGray);

        bool wrote = ApplyNr(srcTex, dstTex, width, height, /*intensity=*/1.0f, /*passes=*/2,
                              /*colorStrength=*/1.0f, /*tonePreservation=*/1.0f,
                              /*grainPreservation=*/0.0f);
        Check(wrote, "tonePreservation=1.0: ApplyNr() reported it wrote dstTexture");

        unsigned char* out = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTex, width, height, out);
        Check(gl.glGetError() == GL_NO_ERROR, "tonePreservation=1.0: no GL error after NR and readback");

        double outputStdDev = RowStdDev(out, width, height / 2, 8, width - 8, 0);
        printf("  tonePreservation=1.0 row stddev: output=%.2f (expect close to the ~24 input amplitude)\n",
               outputStdDev);
        Check(outputStdDev > 15.0, "tonePreservation=1.0: luma noise survived (tone protection worked)");
        delete[] out;

        gl.glDeleteTextures(1, &srcTex);
        gl.glDeleteTextures(1, &dstTex);
    }
    delete[] noisyGray;

    // --- 4. Colored chroma noise: colorStrength isolates whether chroma gets smoothed. ---
    {
        unsigned char* noisyColor = new unsigned char[texelCount * 4];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t i = ((size_t)y * width + x) * 4;
                // Alternates between a warm and cool tint at roughly the same luma, so this is
                // chroma noise, not luma noise - constant brightness, oscillating color.
                bool warm = (x + y) % 2 == 0;
                noisyColor[i + 0] = warm ? 170 : 90;
                noisyColor[i + 1] = 130;
                noisyColor[i + 2] = warm ? 90 : 170;
                noisyColor[i + 3] = 255;
            }
        }

        unsigned int srcTexA = MakeTexture(gl, width, height);
        unsigned int dstTexA = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTexA, width, height, noisyColor);
        bool wroteA = ApplyNr(srcTexA, dstTexA, width, height, /*intensity=*/1.0f, /*passes=*/2,
                               /*colorStrength=*/0.0f, /*tonePreservation=*/0.0f,
                               /*grainPreservation=*/0.0f);
        Check(wroteA, "colorStrength=0.0: ApplyNr() reported it wrote dstTexture");
        unsigned char* outA = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTexA, width, height, outA);

        unsigned int srcTexB = MakeTexture(gl, width, height);
        unsigned int dstTexB = MakeTexture(gl, width, height);
        UploadRgba(gl, srcTexB, width, height, noisyColor);
        bool wroteB = ApplyNr(srcTexB, dstTexB, width, height, /*intensity=*/1.0f, /*passes=*/2,
                               /*colorStrength=*/1.0f, /*tonePreservation=*/0.0f,
                               /*grainPreservation=*/0.0f);
        Check(wroteB, "colorStrength=1.0: ApplyNr() reported it wrote dstTexture");
        unsigned char* outB = new unsigned char[texelCount * 4];
        ReadBack(gl, dstTexB, width, height, outB);

        Check(gl.glGetError() == GL_NO_ERROR, "chroma noise: no GL error after either NR run");

        double stdDevInput = RowStdDev(noisyColor, width, height / 2, 8, width - 8, 0);
        double stdDevColorOff = RowStdDev(outA, width, height / 2, 8, width - 8, 0);
        double stdDevColorOn = RowStdDev(outB, width, height / 2, 8, width - 8, 0);
        printf("  chroma (red channel) row stddev: input=%.2f, colorStrength=0 -> %.2f, "
               "colorStrength=1 -> %.2f\n", stdDevInput, stdDevColorOff, stdDevColorOn);
        Check(stdDevColorOff > stdDevInput * 0.7,
              "colorStrength=0.0: chroma noise survived (color left untouched)");
        Check(stdDevColorOn < stdDevInput * 0.5,
              "colorStrength=1.0: chroma noise was smoothed on the same input");

        delete[] noisyColor;
        delete[] outA;
        delete[] outB;
        gl.glDeleteTextures(1, &srcTexA);
        gl.glDeleteTextures(1, &dstTexA);
        gl.glDeleteTextures(1, &srcTexB);
        gl.glDeleteTextures(1, &dstTexB);
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
