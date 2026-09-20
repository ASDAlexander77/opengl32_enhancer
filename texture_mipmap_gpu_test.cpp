// Creates a real OpenGL context and checks that texture_mipmap.h's draw-time hook actually
// builds a mip chain on the real, currently-bound texture object. See gl_loader_test.cpp for
// why <windows.h> is safe here and for the window/context pattern.
//
// texture_mipmap_test.cpp already pins the decision rule on the CPU. What can only be checked
// against a driver is the other half: that a granted claim results in levels above 0 actually
// existing, and that the min filter ends up sampling them. Reads opengl32_enhancer.ini next to
// this .exe like config.cpp always does - CMakeLists.txt copies texture_mipmap_test.ini there
// (renamed), a fixture with autoMipmap=1.
#include <windows.h>
#include <GL/gl.h>
#include <cstdio>

#include "config.h"
#include "gl_loader.h"
#include "texture_mipmap.h"

namespace {

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

// Level 1 of a 64x64 texture is 32x32 if a chain exists, and querying a level that was never
// supplied reads back width 0. That makes this a direct question to the driver rather than a
// restatement of what our own code believes.
int LevelWidth(int level) {
    int width = -1;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &width);
    return width;
}

unsigned int MinFilter() {
    int filter = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    return (unsigned int)filter;
}

// One 64x64 RGBA texture with level 0 supplied and nothing else - exactly what the census found
// 12 of in the world pass.
unsigned int MakeLevelZeroOnlyTexture() {
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char pixels[64 * 64 * 4];
    for (int i = 0; i < 64 * 64 * 4; ++i) {
        pixels[i] = (unsigned char)(i * 7);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);
    // What the wrapper's glBindTexture/glTexImage2D hooks would have recorded.
    NotifyTextureBound(tex);
    NotifyBoundTextureLevelUploaded(0);
    return tex;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxTextureMipmapTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "texture_mipmap_gpu_test",
                                WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                                wc.hInstance, nullptr);
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
    if (!Check(GetAnaxConfig().autoMipmap, "the fixture ini turned autoMipmap on")) {
        return 1;
    }

    // --- A world-pass draw builds the chain and switches the filter to sample it ---
    {
        ResetTextureMipmapState();
        NotifyMipmapWorldPass();
        unsigned int tex = MakeLevelZeroOnlyTexture();
        Check(LevelWidth(1) == 0, "before: level 1 does not exist");

        ApplyAutoMipmapForDraw();

        Check(LevelWidth(1) == 32, "after a world-pass draw: level 1 is 32x32");
        Check(MinFilter() == GL_LINEAR_MIPMAP_LINEAR,
              "after a world-pass draw: the min filter samples the new levels");
        glDeleteTextures(1, &tex);
    }

    // --- A 2D-pass draw leaves the texture exactly as the game uploaded it ---
    // The safety property, checked against the driver rather than against our own bookkeeping.
    {
        ResetTextureMipmapState();
        NotifyMipmapTwoDPass();
        unsigned int tex = MakeLevelZeroOnlyTexture();

        ApplyAutoMipmapForDraw();

        Check(LevelWidth(1) == 0, "after a 2D-pass draw: level 1 STILL does not exist");
        Check(MinFilter() == GL_LINEAR,
              "after a 2D-pass draw: the game's own min filter is untouched");
        glDeleteTextures(1, &tex);
    }

    // --- A texture that carries the game's own chain is never regenerated ---
    {
        ResetTextureMipmapState();
        NotifyMipmapWorldPass();
        unsigned int tex = MakeLevelZeroOnlyTexture();
        NotifyBoundTextureLevelUploaded(1);   // the game declaring it owns the chain

        ApplyAutoMipmapForDraw();

        Check(MinFilter() == GL_LINEAR,
              "a texture the game mipped itself keeps the filter the game chose");
        glDeleteTextures(1, &tex);
    }

    // --- A binding left over from the 2D pass is not mipped by the world pass's first draw ---
    {
        ResetTextureMipmapState();
        NotifyMipmapTwoDPass();
        unsigned int tex = MakeLevelZeroOnlyTexture();
        NotifyMipmapWorldPass();              // new frame, no rebind yet
        ApplyAutoMipmapForDraw();

        Check(LevelWidth(1) == 0,
              "a binding left over from the 2D pass is not mipped by a world-pass draw");
        glDeleteTextures(1, &tex);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
