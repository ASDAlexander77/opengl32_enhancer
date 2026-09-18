// Creates a real OpenGL context and runs ApplyGamma() directly against src/dst textures. See
// gl_loader_test.cpp for why <windows.h> is safe here and for the window/context pattern.
//
// One fixture throughout: a four-block step wedge (0, 64, 128, 255) so every check reads black,
// two interior tones and white at once - which is what separates "the curve moved something"
// from "the curve moved the RIGHT things". The properties:
//   1. gamma=1, brightness=1 is an exact passthrough on all four blocks. This is the identity
//      the ini documents, and it is only exact because the shader skips pow() entirely at
//      gamma=1 rather than trusting pow(x, 1.0) to be bit-exact on every driver.
//   2. brightness alone (gamma=1) is a straight linear gain: halving it halves every block,
//      black included (0 * 0.5 is still 0).
//   3. gamma alone (brightness=1) lifts the midtones while leaving BOTH endpoints pinned -
//      0 stays 0 and 255 stays 255. That endpoint behaviour is the difference between a gamma
//      curve and an additive brightness lift, which would raise black off the floor and wash
//      the image out. 128 is checked against the arithmetic value, not just "brighter".
//   4. The wedge stays monotonically increasing afterwards - a gamma curve reorders nothing.
//   5. Black survives the worst combination on offer (gamma=0.5 with brightness=2.0, i.e. the
//      steepest exponent this accepts) as exactly 0, not NaN. pow() on a negative or a
//      malformed zero is the obvious way for this stage to spray NaN across a frame.
#include <windows.h>
#include <cstdio>
#include <cstdlib>

#include "gamma.h"
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
const unsigned int GL_NEAREST            = 0x2600;
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
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

// The step wedge: four equal-width blocks of flat grey, dark to light.
const int kBlockWidth = 16;
const int kBlockCount = 4;
const int kWidth = kBlockWidth * kBlockCount;
const int kHeight = 8;
const unsigned char kSteps[kBlockCount] = {0, 64, 128, 255};

void BuildWedge(unsigned char* pixels) {
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            size_t i = ((size_t)y * kWidth + x) * 4;
            unsigned char v = kSteps[x / kBlockWidth];
            pixels[i + 0] = v;
            pixels[i + 1] = v;
            pixels[i + 2] = v;
            pixels[i + 3] = 255;
        }
    }
}

// The red channel at the middle of block `block`, which is flat, so one texel represents it.
unsigned char BlockValue(const unsigned char* pixels, int block) {
    int x = block * kBlockWidth + kBlockWidth / 2;
    int y = kHeight / 2;
    return pixels[((size_t)y * kWidth + x) * 4];
}

// Runs one ApplyGamma() call over a fresh wedge and reads the result back. Returns false if the
// stage declined to write or left a GL error behind.
bool RunGamma(const GlComputeApi& gl, float gamma, float brightness, unsigned char* out,
              const char* label) {
    unsigned int srcTex = MakeTexture(gl, kWidth, kHeight);
    unsigned int dstTex = MakeTexture(gl, kWidth, kHeight);

    unsigned char wedge[kWidth * kHeight * 4];
    BuildWedge(wedge);
    UploadRgba(gl, srcTex, kWidth, kHeight, wedge);

    bool wrote = ApplyGamma(srcTex, dstTex, kWidth, kHeight, gamma, brightness);
    ReadBack(gl, dstTex, kWidth, kHeight, out);
    unsigned int err = gl.glGetError();

    gl.glDeleteTextures(1, &srcTex);
    gl.glDeleteTextures(1, &dstTex);

    if (!wrote) {
        printf("FAIL: %s: ApplyGamma() reported it did not write dstTexture\n", label);
        ++g_failures;
        return false;
    }
    if (err != GL_NO_ERROR) {
        printf("FAIL: %s: glGetError() = 0x%04X after ApplyGamma() and readback\n", label, err);
        ++g_failures;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxGammaTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "gamma_test", WS_OVERLAPPEDWINDOW,
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

    unsigned char out[kWidth * kHeight * 4];

    // --- 1. gamma=1, brightness=1: exact passthrough, every block. ---
    if (RunGamma(gl, 1.0f, 1.0f, out, "gamma=1 brightness=1")) {
        bool same = true;
        for (int b = 0; b < kBlockCount; ++b) {
            int got = (int)BlockValue(out, b);
            printf("  identity block %d: %d (expected %d)\n", b, got, (int)kSteps[b]);
            same = same && abs(got - (int)kSteps[b]) <= 1;
        }
        Check(same, "gamma=1, brightness=1 reproduced the wedge unchanged");
    }

    // --- 2. brightness alone is a linear gain. ---
    if (RunGamma(gl, 1.0f, 0.5f, out, "gamma=1 brightness=0.5")) {
        bool halved = true;
        for (int b = 0; b < kBlockCount; ++b) {
            int expected = (int)kSteps[b] / 2;
            int got = (int)BlockValue(out, b);
            printf("  brightness=0.5 block %d: %d (expected ~%d)\n", b, got, expected);
            halved = halved && abs(got - expected) <= 2;
        }
        Check(halved, "brightness=0.5 at gamma=1 halved every block, black included");
    }

    // --- 3. gamma alone lifts midtones and pins both endpoints. ---
    if (RunGamma(gl, 2.2f, 1.0f, out, "gamma=2.2 brightness=1")) {
        int black = (int)BlockValue(out, 0);
        int dark = (int)BlockValue(out, 1);
        int mid = (int)BlockValue(out, 2);
        int white = (int)BlockValue(out, 3);
        // pow(64/255, 1/2.2) = 0.5371 -> 137; pow(128/255, 1/2.2) = 0.7311 -> 186.
        printf("  gamma=2.2 wedge: %d %d %d %d (expected 0 ~137 ~186 255)\n",
               black, dark, mid, white);
        Check(black == 0, "gamma=2.2 left black at exactly 0 (a curve, not an additive lift)");
        Check(abs(white - 255) <= 1, "gamma=2.2 left white pinned at 255");
        Check(abs(dark - 137) <= 2, "gamma=2.2 lifted the dark step to its arithmetic value");
        Check(abs(mid - 186) <= 2, "gamma=2.2 lifted the mid step to its arithmetic value");

        Check(black < dark && dark < mid && mid < white,
              "gamma=2.2 left the wedge monotonically increasing");
    }

    // --- 4. Black survives the steepest settings as 0, not NaN. ---
    if (RunGamma(gl, 0.5f, 2.0f, out, "gamma=0.5 brightness=2")) {
        int black = (int)BlockValue(out, 0);
        printf("  gamma=0.5 brightness=2.0 black block: %d (expected 0)\n", black);
        Check(black == 0, "gamma=0.5 with brightness=2.0 kept black at 0 without producing NaN");
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
