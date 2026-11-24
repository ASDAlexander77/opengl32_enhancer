// Creates a real OpenGL context and runs ApplySrgbDecode()/ApplySrgbEncode() directly against
// src/dst textures. See gl_loader_test.cpp for why <windows.h> is safe here and for the
// window/context pattern; gamma_test.cpp is the closest existing single-pass stage test and this
// copies its preamble.
#include <windows.h>
#include <cmath>
#include <cstdio>

#include "srgb_convert.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_RGBA               = 0x1908;
const unsigned int GL_FLOAT              = 0x1406;
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

// The transfer function itself, pinned at the value that separates the real curve from the
// pow(2.2) approximation people reach for. sRGB 0.5 is linear 0.21404; pow(0.5, 2.2) is
// 0.21764. Those differ by ~0.9 of an 8-bit step, so a +-1/255 tolerance would NOT catch the
// approximation - this asserts against the float value directly, which does.
const float kMidSrgb   = 0.5f;
const float kMidLinear = 0.21404f;

// A four-step wedge covering both segments of the piecewise curve. 0.02 is BELOW the 0.04045
// knee, so it exercises the linear segment that a single pow() gets wrong by a wide margin;
// the rest are on the power segment. 0 and 1 are fixed points of both directions.
const float kWedge[4] = {0.0f, 0.02f, 0.5f, 1.0f};

const int kWidth = 16;
const int kHeight = 16;

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

// Fills every pixel of tex with (v, v, v, 1). RGBA16F plus a float upload/readback keeps this
// test off the 8-bit grid, which the below-the-knee case needs: on the 8-bit grid the expected
// value 0.001548 and the pow(2.2) value 0.000158 both quantise to 0, and the mutation would
// survive.
void UploadFloat(const GlComputeApi& gl, unsigned int tex, float v) {
    float pixels[kWidth * kHeight * 4];
    for (int i = 0; i < kWidth * kHeight; ++i) {
        pixels[i * 4 + 0] = v;
        pixels[i * 4 + 1] = v;
        pixels[i * 4 + 2] = v;
        pixels[i * 4 + 3] = 1.0f;
    }
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight, GL_RGBA, GL_FLOAT, pixels);
}

// Reads back one pixel's red channel as a float via an FBO attach + glReadPixels.
float ReadBackFloat(const GlComputeApi& gl, unsigned int tex) {
    unsigned int fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    gl.glReadBuffer(GL_COLOR_ATTACHMENT0);
    float pixel[4] = {0, 0, 0, 0};
    gl.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);
    gl.glDeleteFramebuffers(1, &fbo);
    return pixel[0];
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxSrgbConvertTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "srgb_convert_test", WS_OVERLAPPEDWINDOW,
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

    unsigned int srcTex = MakeTexture(gl, kWidth, kHeight);
    unsigned int midTex = MakeTexture(gl, kWidth, kHeight);
    unsigned int dstTex = MakeTexture(gl, kWidth, kHeight);

    // --- decode maps sRGB to linear at the pinned midpoint ---
    {
        UploadFloat(gl, srcTex, kMidSrgb);                 // fills the source with 0.5 in rgb
        bool wrote = ApplySrgbDecode(srcTex, dstTex, kWidth, kHeight);
        Check(wrote, "ApplySrgbDecode reported that it wrote");
        float got = ReadBackFloat(gl, dstTex);
        printf("decode: sRGB %.4f -> linear %.5f (exact curve gives %.5f, pow(2.2) gives %.5f)\n",
               kMidSrgb, got, kMidLinear, 0.21764f);
        Check(fabsf(got - kMidLinear) < 0.001f,
              "decode uses the exact piecewise curve, not a pow(2.2) approximation");
    }

    // --- encode is decode's inverse across both segments of the curve ---
    {
        for (int i = 0; i < 4; ++i) {
            UploadFloat(gl, srcTex, kWedge[i]);
            ApplySrgbDecode(srcTex, midTex, kWidth, kHeight);
            ApplySrgbEncode(midTex, dstTex, kWidth, kHeight);
            float got = ReadBackFloat(gl, dstTex);
            char what[160];
            snprintf(what, sizeof(what),
                     "encode(decode(%.4f)) == %.4f, round-tripping through both segments",
                     kWedge[i], kWedge[i]);
            Check(fabsf(got - kWedge[i]) < 0.002f, what);
        }
    }

    // --- the linear segment below the knee is a straight 12.92 gain, not a power curve ---
    // This is the case a pow(2.2) approximation fails hardest and the one the round trip above
    // cannot see on its own, since an approximation that is its own inverse still round-trips.
    {
        UploadFloat(gl, srcTex, 0.02f);
        ApplySrgbDecode(srcTex, dstTex, kWidth, kHeight);
        float got = ReadBackFloat(gl, dstTex);
        float expected = 0.02f / 12.92f;                   // 0.001548
        printf("decode below the knee: sRGB 0.02 -> linear %.6f (expected %.6f, "
               "pow(2.2) would give %.6f)\n", got, expected, powf(0.02f, 2.2f));
        Check(fabsf(got - expected) < 0.0002f,
              "below the 0.04045 knee decode is a straight 1/12.92 gain, not a power curve");
    }

    unsigned int err = gl.glGetError();
    Check(err == GL_NO_ERROR, "no GL error left behind after the pipeline ran");

    gl.glDeleteTextures(1, &srcTex);
    gl.glDeleteTextures(1, &midTex);
    gl.glDeleteTextures(1, &dstTex);

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
