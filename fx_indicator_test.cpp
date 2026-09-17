// Creates a real OpenGL context and runs DrawFxIndicator() directly against a texture. See
// gl_loader_test.cpp for why <windows.h> is safe here and for the window/context pattern.
//
// Four properties:
//   1. A texture too small to fit the badge with its corner margin is left completely
//      unchanged - no clipped/garbled partial badge.
//   2. A point far from the badge's corner footprint is untouched by a badge draw elsewhere on
//      the same (large enough) texture.
//   3. A known "on" bit of the "F" glyph's top bar comes back white - the badge is actually
//      drawing text, not just a blank plate.
//   4. A point inside the plate's footprint but outside either glyph darkens toward the plate
//      color - the semi-transparent background plate is there for contrast against any scene.
#include <windows.h>
#include <cstdio>
#include <cstdlib>

#include "fx_indicator.h"
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

void FillFlat(const GlComputeApi& gl, unsigned int tex, int width, int height,
              unsigned char r, unsigned char g, unsigned char b) {
    size_t texelCount = (size_t)width * (size_t)height;
    unsigned char* flat = new unsigned char[texelCount * 4];
    for (size_t i = 0; i < texelCount; ++i) {
        flat[i * 4 + 0] = r; flat[i * 4 + 1] = g; flat[i * 4 + 2] = b; flat[i * 4 + 3] = 255;
    }
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, flat);
    delete[] flat;
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
    wc.lpszClassName = "AnaxFxIndicatorTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "fx_indicator_test", WS_OVERLAPPEDWINDOW,
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

    // --- 1. Too small: must be left completely untouched. ---
    {
        const int width = 32;
        const int height = 32;
        unsigned int tex = MakeTexture(gl, width, height);
        FillFlat(gl, tex, width, height, 60, 120, 180);

        DrawFxIndicator(tex, width, height);
        Check(gl.glGetError() == GL_NO_ERROR, "too-small texture: no GL error");

        unsigned char* out = new unsigned char[(size_t)width * height * 4];
        ReadBack(gl, tex, width, height, out);
        size_t center = ((size_t)(height / 2) * width + (width / 2)) * 4;
        printf("  too-small center: r=%d g=%d b=%d (expected 60/120/180, untouched)\n",
               out[center + 0], out[center + 1], out[center + 2]);
        Check(abs((int)out[center + 0] - 60) <= 1 &&
              abs((int)out[center + 1] - 120) <= 1 &&
              abs((int)out[center + 2] - 180) <= 1,
              "too-small texture: left completely untouched, no clipped badge");
        delete[] out;
        gl.glDeleteTextures(1, &tex);
    }

    // --- 2-4: a texture large enough to fit the badge with margin to spare. ---
    {
        const int width = 128;
        const int height = 64;
        unsigned int tex = MakeTexture(gl, width, height);
        FillFlat(gl, tex, width, height, 60, 120, 180);

        DrawFxIndicator(tex, width, height);
        Check(gl.glGetError() == GL_NO_ERROR, "large texture: no GL error after DrawFxIndicator");

        unsigned char* out = new unsigned char[(size_t)width * height * 4];
        ReadBack(gl, tex, width, height, out);

        // Badge footprint (kPlateWidth=45, kPlateHeight=33, kMargin=8 - see fx_indicator.cpp):
        // origin = (128-8-45, 8) = (75, 8).

        // Point 2: far from the badge (bottom-left corner) - untouched.
        size_t farPoint = ((size_t)(height - 2) * width + 2) * 4;
        printf("  far point: r=%d g=%d b=%d (expected 60/120/180, untouched)\n",
               out[farPoint + 0], out[farPoint + 1], out[farPoint + 2]);
        Check(abs((int)out[farPoint + 0] - 60) <= 1 &&
              abs((int)out[farPoint + 1] - 120) <= 1 &&
              abs((int)out[farPoint + 2] - 180) <= 1,
              "large texture: a point far from the badge is untouched");

        // Point 3: inside the "F" glyph's top bar (local (7,7), global (82,15)) - white.
        size_t glyphPoint = ((size_t)15 * width + 82) * 4;
        printf("  glyph point: r=%d g=%d b=%d (expected near-white)\n",
               out[glyphPoint + 0], out[glyphPoint + 1], out[glyphPoint + 2]);
        Check(out[glyphPoint + 0] > 200 && out[glyphPoint + 1] > 200 && out[glyphPoint + 2] > 200,
              "large texture: a known 'on' bit of the F glyph is drawn white");

        // Point 4: inside the plate but outside both glyphs (local (40,30), global (115,38)) -
        // darkened toward the plate color, not the original fill.
        size_t platePoint = ((size_t)38 * width + 115) * 4;
        printf("  plate point: r=%d g=%d b=%d (expected darker than 60/120/180)\n",
               out[platePoint + 0], out[platePoint + 1], out[platePoint + 2]);
        Check(out[platePoint + 0] < 40 && out[platePoint + 1] < 70 && out[platePoint + 2] < 100,
              "large texture: a non-glyph point inside the plate darkened toward the backing color");

        delete[] out;
        gl.glDeleteTextures(1, &tex);
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
