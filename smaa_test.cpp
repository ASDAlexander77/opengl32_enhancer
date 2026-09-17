// Creates a real OpenGL context, clears the back buffer to a known solid color, captures it
// into a src RGBA16F texture, runs ApplySmaa() directly against src/dst textures, and checks
// the three-pass pipeline runs with no GL error. A flat/uniform image has zero luma edges
// anywhere, so SMAA's edge-detection pass should find nothing to antialias and the
// neighborhood-blending pass's "no blending weight found" branch should reproduce the input
// color exactly - a correctness check in the same spirit as bloom_test.cpp/vignette_test.cpp's
// "intensity=0 must exactly reproduce the input" case, except here it falls out of the
// algorithm itself (no edges) rather than a strength parameter. See gl_loader_test.cpp for the
// window/context creation pattern reused below.
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <vector>

#include "smaa.h"
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
const unsigned int GL_NEAREST            = 0x2600;
const unsigned int GL_CLAMP_TO_EDGE      = 0x812F;
const unsigned int GL_FRAMEBUFFER        = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER   = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER   = 0x8CA9;
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
    wc.lpszClassName = "AnaxSmaaTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "smaa_test", WS_OVERLAPPEDWINDOW,
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

    // Clear to a known flat color - zero edges anywhere, so SMAA should reproduce it exactly.
    pGlClearColor(0.3f, 0.6f, 0.9f, 1.0f);
    pGlClear(GL_COLOR_BUFFER_BIT);
    unsigned int srcTex = CreatePipelineTexture(gl, width, height);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    unsigned int dstTex = CreatePipelineTexture(gl, width, height);

    unsigned int readFbo = 0;
    gl.glGenFramebuffers(1, &readFbo);

    bool wrote1 = ApplySmaa(srcTex, dstTex, width, height);
    unsigned int errAfterFirst = gl.glGetError();
    ok &= Check(wrote1 && errAfterFirst == 0, "no GL error after first ApplySmaa() call");

    // Run again to exercise the "already compiled / scratch textures already sized" reuse path.
    bool wrote2 = ApplySmaa(srcTex, dstTex, width, height);
    unsigned int errAfterSecond = gl.glGetError();
    ok &= Check(wrote2 && errAfterSecond == 0,
                "no GL error after second ApplySmaa() call (texture/shader reuse)");

    gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
    unsigned char pixel[4] = {0, 0, 0, 0};
    pGlReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    printf("Center pixel after SMAA on a flat image: r=%d g=%d b=%d a=%d\n",
           pixel[0], pixel[1], pixel[2], pixel[3]);
    // Expect (77, 153, 230) = (0.3, 0.6, 0.9) in 8-bit - a flat image has no edges anywhere,
    // so the pipeline should pass it through unchanged.
    bool colorPlausible = std::abs((int)pixel[0] - 77) < 3
                        && std::abs((int)pixel[1] - 153) < 3
                        && std::abs((int)pixel[2] - 230) < 3;
    ok &= Check(colorPlausible, "SMAA reproduced a flat (edgeless) image without alteration");

    // Second scenario: a white square on black - finite-length edges with real corners nearby
    // (unlike an edge-to-edge line spanning the whole image, which has no crossing edge for
    // the search to find within its window - a degenerate case SMAA's area lookup treats as
    // "no antialiasing data", by design, not a bug). A working implementation should blend a
    // few pixels near the square's boundary toward gray.
    const int squareLeft = width / 2 - 20, squareTop = height / 2 - 20;
    const int squareRight = width / 2 + 20, squareBottom = height / 2 + 20;
    {
        std::vector<unsigned char> edgePixels((size_t)width * height * 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bool inside = x >= squareLeft && x < squareRight && y >= squareTop && y < squareBottom;
                unsigned char v = inside ? 255 : 0;
                size_t i = ((size_t)y * width + x) * 4;
                edgePixels[i + 0] = v;
                edgePixels[i + 1] = v;
                edgePixels[i + 2] = v;
                edgePixels[i + 3] = 255;
            }
        }
        unsigned int edgeSrcTex = CreatePipelineTexture(gl, width, height);
        // CreatePipelineTexture leaves the RGBA16F texture allocated but empty - upload via an
        // RGBA8 staging texture + blit, same conversion path texture_sharpen.cpp already uses.
        unsigned int stagingTex = 0;
        gl.glGenTextures(1, &stagingTex);
        gl.glBindTexture(GL_TEXTURE_2D, stagingTex);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const unsigned int GL_RGBA8 = 0x8058;
        gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, edgePixels.data());
        unsigned int stagingFbo = 0;
        gl.glGenFramebuffers(1, &stagingFbo);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, stagingFbo);
        gl.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, stagingTex, 0);
        // GL_DRAW_FRAMEBUFFER specifically - binding GL_FRAMEBUFFER here would rebind BOTH
        // read and draw targets, clobbering the GL_READ_FRAMEBUFFER binding just set above.
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, edgeSrcTex, 0);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        unsigned int edgeDstTex = CreatePipelineTexture(gl, width, height);
        bool edgeWrote = ApplySmaa(edgeSrcTex, edgeDstTex, width, height);
        unsigned int edgeErr = gl.glGetError();
        ok &= Check(edgeWrote && edgeErr == 0, "no GL error after ApplySmaa() on a square with real corners");

        gl.glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, edgeDstTex, 0);
        int cornerY = squareTop + 3;   // close to the top-left corner, not mid-edge
        int boundaryX = squareLeft;
        printf("Scanline across the square's left edge near its top-left corner (y=%d):\n", cornerY);
        bool sawIntermediate = false;
        for (int dx = -4; dx <= 3; ++dx) {
            unsigned char p[4] = {0, 0, 0, 0};
            pGlReadPixels(boundaryX + dx, cornerY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
            printf("  x=%d -> %d\n", boundaryX + dx, p[0]);
            if (p[0] > 10 && p[0] < 245) {
                sawIntermediate = true;
            }
        }
        ok &= Check(sawIntermediate,
                    "SMAA blended at least one pixel near the corner toward an intermediate value");

        gl.glDeleteFramebuffers(1, &stagingFbo);
        gl.glDeleteTextures(1, &stagingTex);
        unsigned int edgeTextures[2] = {edgeSrcTex, edgeDstTex};
        gl.glDeleteTextures(2, edgeTextures);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("\n%s\n", ok ? "All checks passed." : "Some checks FAILED.");
    return ok ? 0 : 1;
}
