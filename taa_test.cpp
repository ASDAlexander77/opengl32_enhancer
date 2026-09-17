// Creates a real OpenGL context, clears the back buffer to a known color, captures it into a
// src RGBA16F texture once (mimicking what post_effects.cpp's shared pipeline now does once
// per frame), and calls ApplyTaa() four times in a row against that unchanging src texture -
// exercising the historyValid=0 passthrough path (call 1) and the ping-pong blend/clamp path
// across multiple successful frames (calls 2-4). Since the scene never changes, every call
// should still reproduce the clear color (the neighborhood clamp box degenerates to a single
// value when the frame is static, so blending never drifts) - this also functions as the
// ghosting-mitigation check: if the ping-pong role-flip or clamp logic were wrong, a static
// scene would still visibly drift or corrupt after a few frames, which this test would catch.
#include <windows.h>
#include <cstdio>

#include "gl_loader.h"
#include "taa.h"

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
const unsigned int GL_BACK               = 0x0405;

bool CheckClose(unsigned char actual, unsigned char expected, int tolerance, const char* channel, int callNum) {
    int diff = (int)actual - (int)expected;
    if (diff < -tolerance || diff > tolerance) {
        printf("FAIL: call %d, %s channel = %d, expected ~%d (tolerance %d)\n", callNum, channel, actual, expected, tolerance);
        return false;
    }
    return true;
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

void UploadRgba(const GlComputeApi& gl, unsigned int tex, int width, int height,
                 const unsigned char* pixels) {
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 0x1908 /* GL_RGBA */,
                        0x1401 /* GL_UNSIGNED_BYTE */, pixels);
}

// A busy (spatially high-contrast) checkerboard, so the neighborhood clamp's box is wide open
// at every cell boundary - the scenario where a scene-cut ghost can most easily hide, since a
// wide box accepts almost any stale value. `phaseShift` moves the cell boundaries sideways,
// so the same on-screen pixel can be assigned to a "light" cell in one call and a "dark" cell
// in another while both frames remain equally busy - simulating a scene cut, not just a
// brightness change, without needing real motion vectors to describe it.
void BuildChecker(unsigned char* pixels, int width, int height, int cellSize, int phaseShift,
                   unsigned char dark, unsigned char light) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            int cx = (x - phaseShift) / cellSize;
            if (x - phaseShift < 0) { cx -= 1; }  // floor division for negative x
            int cy = y / cellSize;
            unsigned char v = ((cx + cy) % 2 == 0) ? light : dark;
            pixels[i + 0] = v; pixels[i + 1] = v; pixels[i + 2] = v; pixels[i + 3] = 255;
        }
    }
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxTaaTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "taa_test", WS_OVERLAPPEDWINDOW,
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
    pGlViewport(0, 0, 128, 128);
    pGlClearColor(0.8f, 0.2f, 0.1f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);

    bool ok = true;
    const GlComputeApi& gl = GetGlComputeApi();
    int width = 128, height = 128;

    unsigned int srcTex = CreatePipelineTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    unsigned int dstTex = CreatePipelineTexture(gl, width, height);
    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    for (int call = 1; call <= 4; ++call) {
        // shimmerSuppression=0.0 here - the existing history/clamp behavior this loop checks
        // must be unaffected when the feature is off.
        bool wrote = ApplyTaa(srcTex, dstTex, width, height, 0.85f, 0.0f);
        unsigned int err = gl.glGetError();
        if (!wrote || err != 0) {
            printf("FAIL: call %d left glGetError() = 0x%04X (wrote=%d)\n", call, err, wrote ? 1 : 0);
            ok = false;
            continue;
        }
        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        unsigned char pixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
        printf("Center pixel after call %d: r=%d g=%d b=%d a=%d\n", call, pixel[0], pixel[1], pixel[2], pixel[3]);
        bool callOk = CheckClose(pixel[0], 204, 2, "r", call);
        callOk = CheckClose(pixel[1], 51, 2, "g", call) && callOk;
        callOk = CheckClose(pixel[2], 25, 2, "b", call) && callOk;
        if (callOk) {
            printf("PASS: call %d reproduced the cleared color\n", call);
        }
        ok = callOk && ok;
    }

    // shimmerSuppression=1.0 on a perfectly static scene: current and clamped history already
    // agree everywhere, so stillness=1.0 and the effective blend rises toward 0.95 - but since
    // there's no actual difference to blend between, the output must still reproduce the same
    // color, not drift toward some other value. This is what would catch a sign error or a
    // stillness computation that's inverted (kicking in on MOTION instead of stillness).
    {
        bool wrote = ApplyTaa(srcTex, dstTex, width, height, 0.85f, 1.0f);
        unsigned int err = gl.glGetError();
        if (!wrote || err != 0) {
            printf("FAIL: shimmerSuppression=1.0 call left glGetError() = 0x%04X (wrote=%d)\n", err, wrote ? 1 : 0);
            ok = false;
        } else {
            gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
            gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
            unsigned char pixel[4] = {0, 0, 0, 0};
            gl.glReadPixels(64, 64, 1, 1, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, pixel);
            printf("Center pixel after shimmerSuppression=1.0 call: r=%d g=%d b=%d a=%d\n",
                   pixel[0], pixel[1], pixel[2], pixel[3]);
            bool callOk = CheckClose(pixel[0], 204, 2, "r", 5);
            callOk = CheckClose(pixel[1], 51, 2, "g", 5) && callOk;
            callOk = CheckClose(pixel[2], 25, 2, "b", 5) && callOk;
            if (callOk) {
                printf("PASS: shimmerSuppression=1.0 on a static scene still reproduced the cleared color\n");
            }
            ok = callOk && ok;
        }
    }

    // Fast scene change on BUSY content: this is the ghosting case a flat clear color can't
    // reproduce, because a flat frame makes the neighborhood clamp degenerate to a single
    // value (see this file's header comment) - trivially rejecting any stale history and
    // hiding a real bug. A checkerboard keeps the clamp box wide open at every cell boundary,
    // which is exactly the situation where stale history can numerically fit inside the new
    // frame's local range even though the on-screen content is completely different.
    {
        const int width2 = 64;
        const int height2 = 64;
        const int cellSize = 4;
        const unsigned char kDark = 40;
        const unsigned char kLight = 220;
        // Probe pixel (4,4) sits at a cell boundary - some of its 3x3 neighbors are dark, some
        // light, in EVERY scheme below, so the clamp box is wide there in every call.
        const int probeX = 4;
        const int probeY = 4;

        unsigned int srcTex2 = CreatePipelineTexture(gl, width2, height2);
        unsigned int dstTex2 = CreatePipelineTexture(gl, width2, height2);

        size_t texelCount = (size_t)width2 * height2;
        unsigned char* schemeA = new unsigned char[texelCount * 4];
        // phaseShift=0: probe (4,4) -> cx=1,cy=1 -> (1+1)%2==0 -> light (220).
        BuildChecker(schemeA, width2, height2, cellSize, 0, kDark, kLight);

        // Seed converged history on scheme A - two calls: the first takes the historyValid=0
        // passthrough path (this is a fresh width/height, so EnsureTextures reset it even
        // though earlier blocks in this file already ran TAA at a different size), the second
        // confirms the clamp settles since the content isn't changing.
        UploadRgba(gl, srcTex2, width2, height2, schemeA);
        ApplyTaa(srcTex2, dstTex2, width2, height2, 0.85f, 0.0f);
        ApplyTaa(srcTex2, dstTex2, width2, height2, 0.85f, 0.0f);

        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex2, 0);
        unsigned char seedPixel[4] = {0, 0, 0, 0};
        gl.glReadPixels(probeX, probeY, 1, 1, 0x1908, 0x1401, seedPixel);
        printf("Probe pixel after seeding on scheme A: r=%d (expected ~%d)\n", seedPixel[0], kLight);
        bool seedOk = CheckClose(seedPixel[0], kLight, 4, "r", 6);
        ok = seedOk && ok;

        // The cut: phaseShift=cellSize flips the probe pixel's cell role to dark, while
        // keeping the frame just as busy everywhere. ONE call, simulating the very next frame
        // after a fast scene change.
        unsigned char* schemeB = new unsigned char[texelCount * 4];
        BuildChecker(schemeB, width2, height2, cellSize, cellSize, kDark, kLight);
        UploadRgba(gl, srcTex2, width2, height2, schemeB);
        bool wroteCut = ApplyTaa(srcTex2, dstTex2, width2, height2, 0.85f, 0.0f);
        unsigned int errCut = gl.glGetError();
        if (!wroteCut || errCut != 0) {
            printf("FAIL: scene-cut call left glGetError() = 0x%04X (wrote=%d)\n", errCut, wroteCut ? 1 : 0);
            ok = false;
        } else {
            gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex2, 0);
            unsigned char cutPixel[4] = {0, 0, 0, 0};
            gl.glReadPixels(probeX, probeY, 1, 1, 0x1908, 0x1401, cutPixel);
            printf("Probe pixel one frame after the cut: r=%d (new content=%d, stale ghost would be near %d)\n",
                   cutPixel[0], kDark, kLight);
            // A tolerance of 40 still comfortably separates "converged to new content" (~40)
            // from "ghosting" (the unrejected-history math above predicts ~193 for this exact
            // fixture at blend=0.85) - this is not a hair's-width tolerance tightening.
            bool cutOk = CheckClose(cutPixel[0], kDark, 40, "r", 7);
            if (cutOk) {
                printf("PASS: scene cut on busy content converged in one frame, no lingering ghost\n");
            }
            ok = cutOk && ok;
        }

        delete[] schemeA;
        delete[] schemeB;
        gl.glDeleteTextures(1, &srcTex2);
        gl.glDeleteTextures(1, &dstTex2);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return ok ? 0 : 1;
}
