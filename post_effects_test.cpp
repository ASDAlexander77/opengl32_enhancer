// Integration test for the CURRENT opengl32_enhancer.ini, run through the real entry point
// (ApplySelectedEffect(), exactly what wrapper.cpp calls from wglSwapBuffers) rather than
// through any one stage's Apply*() function directly. Where every other *_test.cpp in this
// project unit-tests one stage in isolation against a flat clear color, this one exercises
// "does today's config, in the order it's written, actually do something sane to a real
// image" - the question a config edit alone (no C++ change) can't be caught by any of those.
//
// This links config.cpp itself (not the built DLL), so GetAnaxConfig() resolves
// opengl32_enhancer.ini the normal way - next to whichever module config.cpp is compiled into,
// which here is this test's own .exe (see config.cpp's GetIniPathNextToThisModule and its
// AddressAnchor trick). CMakeLists.txt copies the real opengl32_enhancer.ini and cyberpunk.cube
// next to the built post_effects_test.exe after each build specifically so this test reads
// the SAME config file the DLL ships - not a fixture - so a config-only edit (reorder a
// stage, retune a strength) is covered by re-running this test, no rebuild of anything else
// required.
//
// What this can and can't prove:
//   - CAN prove: the configured pipeline runs end to end with no GL error, and it actually
//     touches the image (catches an empty/no-op pipeline, a stage silently failing to write
//     its dstTexture across the whole chain, or a capture/present wired up wrong).
//   - CAN'T prove: that the result *looks good*. Vignette strength, LUT choice, bloom
//     intensity etc. are aesthetic judgment calls; there is no correct pixel value to assert
//     against for a stylized multi-stage grade. So this test also writes the before/after
//     frames out as .ppm files (see WritePpm) for a human to actually look at - open them in
//     anything that reads PPM (most image viewers, or GIMP/Photoshop) after running this
//     test.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "config.h"
#include "gl_loader.h"
#include "post_effects.h"

namespace {

const unsigned int GL_TEXTURE_2D         = 0x0DE1;
const unsigned int GL_RGBA8              = 0x8058;
const unsigned int GL_TEXTURE_MIN_FILTER = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S     = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T     = 0x2803;
const unsigned int GL_NEAREST            = 0x2600;
const unsigned int GL_CLAMP_TO_EDGE      = 0x812F;
const unsigned int GL_FRAMEBUFFER        = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER   = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER   = 0x8CA9;
const unsigned int GL_COLOR_ATTACHMENT0  = 0x8CE0;
const unsigned int GL_COLOR_BUFFER_BIT   = 0x00004000;
const unsigned int GL_RGBA               = 0x1908;
const unsigned int GL_UNSIGNED_BYTE      = 0x1401;
const unsigned int GL_BACK               = 0x0405;

// A synthetic test pattern, generated on the CPU rather than loaded from an image file (this
// project has no image-decoding dependency and adding one just for a test isn't worth it).
// Deliberately varied so every stage in a typical pipeline has something to react to:
//   - a smooth R/G gradient with a fixed B, so tone-mapping/LUT/vignette changes are visible
//     as a smooth shift rather than hidden in a flat color
//   - a bright near-white patch in one corner, well above the default bloom threshold (0.8),
//     so bloom has something to bloom
//   - a checkerboard patch of hard black/white edges, so sharpen/chromatic aberration/TAA's
//     neighborhood clamp all have high-frequency detail to act on instead of only flat areas
std::vector<unsigned char> BuildTestPattern(int width, int height) {
    std::vector<unsigned char> pixels((size_t)width * height * 4);
    int checkerX0 = width / 2;
    int checkerY0 = height / 2;
    int highlightSize = width / 8;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t i = ((size_t)y * width + x) * 4;
            unsigned char r, g, b;
            if (x < highlightSize && y < highlightSize) {
                // Bright corner patch to trigger bloom.
                r = 250; g = 248; b = 240;
            } else if (x >= checkerX0 && y >= checkerY0) {
                // 8px checkerboard for high-frequency detail.
                bool white = (((x - checkerX0) / 8) + ((y - checkerY0) / 8)) % 2 == 0;
                r = g = b = white ? 235 : 10;
            } else {
                r = (unsigned char)(x * 255 / (width - 1));
                g = (unsigned char)(y * 255 / (height - 1));
                b = 128;
            }
            pixels[i + 0] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

// Writes a binary PPM (P6) - the simplest image format with a human-readable header, needing
// no library. `rgba` is read GL_RGBA/GL_UNSIGNED_BYTE data as glReadPixels returns it: row 0
// is the bottom of the image (OpenGL's convention), so rows are written back to front to come
// out right-side-up in the file.
void WritePpm(const char* path, const unsigned char* rgba, int width, int height) {
    FILE* f = fopen(path, "wb");
    if (f == nullptr) {
        printf("FAIL: could not open '%s' for writing\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    std::vector<unsigned char> row(width * 3);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char* src = rgba + ((size_t)y * width + x) * 4;
            row[x * 3 + 0] = src[0];
            row[x * 3 + 1] = src[1];
            row[x * 3 + 2] = src[2];
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    printf("wrote %s (%dx%d)\n", path, width, height);
}

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    return condition;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxPostEffectsTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    // CreateWindowExA's size is the OUTER window (title bar + borders included), not the
    // client area every GL call below actually renders into - passing width/height directly
    // would silently under-size the real back buffer, leaving glViewport/glCopyTexSubImage2D/
    // glReadPixels reading past its edge into undefined driver memory (NaN/garbage) for
    // however many pixels of title bar/border got shortchanged. AdjustWindowRect asks Windows
    // for the outer size that yields exactly this CLIENT size instead.
    const int width = 256, height = 256;
    RECT windowRect = {0, 0, width, height};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "post_effects_test", WS_OVERLAPPEDWINDOW,
        0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    // Confirm AdjustWindowRect actually got us the client size every GL call below assumes -
    // DPI scaling or a theme with unusual chrome could still throw this off, and a silent
    // mismatch here means every readback past the real edge picks up undefined memory.
    RECT clientRect = {};
    GetClientRect(hwnd, &clientRect);
    bool clientSizeOk = Check(clientRect.right - clientRect.left == width && clientRect.bottom - clientRect.top == height,
                               "window client area matches the requested width/height");
    if (!clientSizeOk) {
        printf("FAIL: client area is %dx%d, expected %dx%d\n",
               (int)(clientRect.right - clientRect.left), (int)(clientRect.bottom - clientRect.top),
               width, height);
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

    typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);
    PFNGLVIEWPORTPROC pGlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glViewport");
    pGlViewport(0, 0, width, height);

    bool ok = true;
    const GlComputeApi& gl = GetGlComputeApi();
    ok = Check(gl.loaded, "GL 4.3 compute support available on this context") && ok;
    if (!gl.loaded) {
        // Every stage no-ops without this, so there's nothing meaningful left to test here -
        // same "log and bail" the individual stage tests do, rather than a hard failure for
        // an environment limitation outside this test's control.
        printf("Skipping pipeline checks: no compute support on this context/driver.\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        return ok ? 0 : 1;
    }

    // Log exactly what pipeline is about to run, from the real config loader - this is the
    // same summary line GetAnaxConfig() already prints, surfaced again here so it's easy to
    // spot in this test's own output without scrolling.
    const AnaxConfig& config = GetAnaxConfig();
    printf("Pipeline under test: %d stage(s)\n", config.stageCount);
    for (int i = 0; i < config.stageCount; ++i) {
        printf("  %d. %s\n", i + 1, EffectNameFor(config.stages[i]));
    }

    // Upload the synthetic test pattern into a texture, then blit it onto the REAL back
    // buffer via an FBO - putting it exactly where ApplySelectedEffect() expects to capture
    // its input from (GL_READ_FRAMEBUFFER 0 / GL_BACK, same as post_effects.cpp's capture).
    std::vector<unsigned char> inputPixels = BuildTestPattern(width, height);
    unsigned int patternTex = 0;
    gl.glGenTextures(1, &patternTex);
    gl.glBindTexture(GL_TEXTURE_2D, patternTex);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    // glReadPixels/PPM both treat row 0 as the bottom (OpenGL convention) - upload the CPU
    // pattern as-is so BuildTestPattern's row 0 also lands at the bottom, keeping the pattern
    // and its later readback consistent with each other.
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, inputPixels.data());

    unsigned int patternFbo = 0;
    gl.glGenFramebuffers(1, &patternFbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, patternFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, patternTex, 0);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, patternFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    unsigned int blitErr = gl.glGetError();
    ok = Check(blitErr == 0, "test pattern blitted onto the real back buffer with no GL error") && ok;

    // This is the actual call under test: the same zero-argument entry point wrapper.cpp
    // calls from wglSwapBuffers, reading whatever opengl32_enhancer.ini next to this .exe says.
    ApplySelectedEffect();
    unsigned int applyErr = gl.glGetError();
    ok = Check(applyErr == 0, "ApplySelectedEffect() left no GL error") && ok;

    // Read back the real back buffer - post_effects.cpp always presents to it, so this is
    // what a player would actually see on screen.
    std::vector<unsigned char> outputPixels((size_t)width * height * 4);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outputPixels.data());
    unsigned int readErr = gl.glGetError();
    ok = Check(readErr == 0, "readback of the final back buffer left no GL error") && ok;

    // A pipeline with stages in it should visibly touch the image; an empty pipeline should
    // leave it untouched (ApplySelectedEffect() returns before even capturing). Either way
    // this only checks "did something happen", not "was it tasteful" - see this file's header
    // comment for why the PPM dump below is the actual verdict on the latter.
    long long totalAbsDiff = 0;
    for (size_t i = 0; i < inputPixels.size(); ++i) {
        totalAbsDiff += (long long)abs((int)inputPixels[i] - (int)outputPixels[i]);
    }
    double avgAbsDiffPerChannel = (double)totalAbsDiff / (double)inputPixels.size();
    printf("Average per-channel |output - input| = %.3f\n", avgAbsDiffPerChannel);
    if (config.stageCount > 0) {
        ok = Check(avgAbsDiffPerChannel > 0.5,
                   "non-empty pipeline visibly changed the image (pipeline isn't silently a no-op)") && ok;
    } else {
        ok = Check(avgAbsDiffPerChannel == 0.0,
                   "empty pipeline left the image byte-for-byte unchanged") && ok;
    }

    WritePpm("post_effects_test_input.ppm", inputPixels.data(), width, height);
    WritePpm("post_effects_test_output.ppm", outputPixels.data(), width, height);
    printf("Look at post_effects_test_input.ppm / post_effects_test_output.ppm to judge "
           "whether the current config actually looks right - that part is a human call, "
           "not something this test can assert.\n");

    // --- A second, config-independent scenario: the `gamma` stage is actually WIRED UP. ---
    //
    // Everything above deliberately tests whatever the shipped ini happens to say, which means
    // it says nothing about a stage the ini doesn't currently list. A stage that exists in
    // EffectKind and parses fine but was never given a case in ApplySelectedEffect()'s switch
    // fails completely silently: it writes nothing, `cur` never flips, and the frame is
    // presented untouched. No compiler diagnostic, no GL error, no log line. So drive one
    // through the mutable config the way the editor does and check the image really moved.
    //
    // fxIndicator has to go off first: the badge is drawn whenever stageCount > 0, regardless
    // of whether any stage ran, so leaving it on would supply a pixel difference all by itself
    // and turn this into a check that always passes.
    {
        AnaxConfig& mutableConfig = GetMutableAnaxConfig();
        mutableConfig.fxIndicator = false;
        mutableConfig.stageCount = 1;
        mutableConfig.stages[0] = EffectKind::Gamma;
        mutableConfig.gamma = 2.2f;
        mutableConfig.brightness = 1.0f;

        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, patternFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        ApplySelectedEffect();

        std::vector<unsigned char> gammaPixels((size_t)width * height * 4);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.glReadBuffer(GL_BACK);
        gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, gammaPixels.data());
        ok = Check(gl.glGetError() == 0, "effect=gamma: ApplySelectedEffect() left no GL error") && ok;

        // gamma=2.2 is a brightening curve, so the mean must rise, not merely differ - a
        // difference alone would also be satisfied by a stage wired to the wrong function.
        double inputMean = 0.0, gammaMean = 0.0;
        size_t sampled = 0;
        for (size_t i = 0; i < inputPixels.size(); i += 4) {
            for (int ch = 0; ch < 3; ++ch) {
                inputMean += inputPixels[i + ch];
                gammaMean += gammaPixels[i + ch];
                ++sampled;
            }
        }
        inputMean /= (double)sampled;
        gammaMean /= (double)sampled;
        printf("effect=gamma (2.2): mean channel %.2f -> %.2f\n", inputMean, gammaMean);
        ok = Check(gammaMean > inputMean + 10.0,
                   "effect=gamma is wired into ApplySelectedEffect() and brightened the frame") && ok;
    }

    gl.glDeleteFramebuffers(1, &patternFbo);
    gl.glDeleteTextures(1, &patternTex);

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf("\n%s\n", ok ? "All checks passed." : "Some checks FAILED.");
    return ok ? 0 : 1;
}
