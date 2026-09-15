// Reads back the current back buffer, inverts every pixel's RGB channels, and draws it
// back over the same buffer - called from wglSwapBuffers (see wrapper32.cpp) just before
// the real swap, so the inverted image is what actually reaches the screen.
#include <cstdio>
#include <cstdlib>

#include "pixel_invert.h"

// GL constants used here, defined by hand rather than pulling in <gl/gl.h> - wrapper32.cpp
// avoids windows.h/gl.h entirely because those headers declare the same wgl*/gl* names our
// dllexport definitions provide, and the two declarations would collide.
static const unsigned int GL_VIEWPORT       = 0x0BA2;
static const unsigned int GL_BACK           = 0x0405;
static const unsigned int GL_RGBA           = 0x1908;
static const unsigned int GL_UNSIGNED_BYTE  = 0x1401;
static const unsigned int GL_PACK_ALIGNMENT = 0x0D05;

// The real entry points, defined (and forwarded to the system opengl32.dll) elsewhere in
// this same DLL by wrapper32.cpp. Declaring and calling them directly here reuses that
// existing forwarding instead of resolving the real opengl32.dll a second time.
extern "C" {
    void __stdcall glGetIntegerv(unsigned int pname, void* params);
    void __stdcall glPixelStorei(unsigned int pname, int param);
    void __stdcall glReadBuffer(unsigned int mode);
    void __stdcall glDrawBuffer(unsigned int mode);
    void __stdcall glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void* pixels);
    void __stdcall glDrawPixels(int width, int height, unsigned int format, unsigned int type, void* pixels);
    void __stdcall glRasterPos2i(int x, int y);
    void __stdcall glPixelZoom(float xfactor, float yfactor);
}

void InvertBackBufferColors() {
    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    if (width <= 0 || height <= 0) {
        return;
    }

    unsigned char* pixels = (unsigned char*)malloc((size_t)width * (size_t)height * 4);
    if (pixels == nullptr) {
        printf("[opengl32_enh_cpp] InvertBackBufferColors: FAILED to allocate %dx%d pixel buffer\n", width, height);
        return;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    size_t pixelCount = (size_t)width * (size_t)height;
    for (size_t i = 0; i < pixelCount; ++i) {
        unsigned char* p = pixels + i * 4;
        p[0] = 255 - p[0];
        p[1] = 255 - p[1];
        p[2] = 255 - p[2];
        // alpha (p[3]) is left untouched
    }

    glRasterPos2i(0, 0);
    glPixelZoom(1.0f, 1.0f);
    glDrawBuffer(GL_BACK);
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    free(pixels);
}
