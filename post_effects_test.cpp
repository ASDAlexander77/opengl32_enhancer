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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "config.h"
#include "gl_loader.h"
#include "post_effects.h"
#include "render_target.h"

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
const unsigned int GL_SCISSOR_TEST       = 0x0C11;

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

// Pins the set of stages that read the game's depth buffer - see StageNeedsDepth() in
// post_effects.h. This is a table, not an integration run, and it is worth being exact about
// what that buys: it proves the predicate names every stage that passes g_pipeline.depthTex to
// its Apply*() in ApplySelectedEffect()'s switch, which is the half that went wrong. It does
// NOT prove the switch only passes depth to stages named here - nothing automated can, and
// that direction stays a reading of the code.
//
// The failure it exists to catch has no symptom to look for. A depth stage missing from the
// predicate does not error, does not log and does not change the frame: with `effect=ssr`
// alone the depth texture is never allocated, so the stage no-ops, and the only thing the
// player sees is reflections that are absent for no stated reason. `ssr` itself was missing
// here, and went unnoticed precisely because a plausible effect= list pairs it with `ssao`,
// which requests the depth that `ssr` then quietly relies on.
//
// Needs no GL context, so it runs before main() builds one and is unaffected by the
// no-compute-support bail-out further down.
bool CheckDepthStageSet() {
    struct Expectation { EffectKind stage; bool needsDepth; const char* name; };
    const Expectation kExpected[] = {
        {EffectKind::DepthVignette,       true,  "depthvignette"},
        {EffectKind::Ssao,                true,  "ssao"},
        {EffectKind::Dof,                 true,  "dof"},
        {EffectKind::Fog,                 true,  "fog"},
        {EffectKind::Ssr,                 true,  "ssr"},
        {EffectKind::MotionBlur,          true,  "motionblur"},
        {EffectKind::Taa,                 true,  "taa"},
        // A sample of stages that read colour only. If one of these ever starts reporting true
        // the pipeline pays for a depth blit every frame that does not need one.
        {EffectKind::Bloom,               false, "bloom"},
        {EffectKind::Gamma,               false, "gamma"},
        {EffectKind::LightShafts,         false, "lightshafts"},
        {EffectKind::NVScaler,            false, "nvscaler"},
        {EffectKind::None,                false, "none"},
    };

    bool allMatched = true;
    for (size_t i = 0; i < sizeof(kExpected) / sizeof(kExpected[0]); ++i) {
        bool actual = StageNeedsDepth(kExpected[i].stage);
        char what[160];
        snprintf(what, sizeof(what), "StageNeedsDepth(%s) == %s",
                 kExpected[i].name, kExpected[i].needsDepth ? "true" : "false");
        allMatched = Check(actual == kExpected[i].needsDepth, what) && allMatched;
    }
    return allMatched;
}

// Pins which depth stages are dropped outright when the chain has already passed a real upscale
// - see StageIsSkippedWhenDepthUnavailable() in post_effects.h. The table half of the same
// question CheckDepthStageSet() above pins, and it exists for a failure with the same shape and
// the same absence of symptoms: `taa` reads depth on its real path, so registering it with
// StageNeedsDepth() (correct, for the allocation gate) also handed it to the chain's skip gate,
// and `effect=..., bilinear, ..., taa, ...` with windowWidth/windowHeight set - the shipped
// layout before the reprojected path existed - went from working TAA-lite to no stage at all.
//
// What this proves and what it does not, stated plainly because the distinction matters here:
//   - PROVES: `taa` is excluded from the skip set and every other depth stage is still in it,
//     so the exemption cannot silently widen to ssao/ssr/dof/fog/motionblur/depthvignette.
//   - DOES NOT PROVE: that ApplySelectedEffect()'s loop actually falls through to ApplyTaa()
//     and reaches NotifyTaaRealPathRan(false) at that position. Exercising that needs a real
//     GL context, a window-size override, and an effect= list this test is not allowed to
//     invent (it reads the SHIPPED opengl32_enhancer.ini on purpose - see the file header). It
//     stays a reading of the code: post_effects.cpp's gate sets taaRealPathAvailable=false and
//     does NOT `continue`, the Taa case ANDs that into taaHaveInputs so the real path declines,
//     and NotifyTaaRealPathRan(ranReal) sits after the `if (!ranReal)` fallback on the one path
//     out of the case.
//
// Needs no GL context, so it runs before main() builds one, same as CheckDepthStageSet().
bool CheckDepthStageSkipSet() {
    struct Expectation { EffectKind stage; bool skipped; const char* name; };
    const Expectation kExpected[] = {
        // Every depth stage but taa: nothing to run without depth, so dropping it and saying
        // so in the log is all there is to do.
        {EffectKind::DepthVignette,       true,  "depthvignette"},
        {EffectKind::Ssao,                true,  "ssao"},
        {EffectKind::Dof,                 true,  "dof"},
        {EffectKind::Fog,                 true,  "fog"},
        {EffectKind::Ssr,                 true,  "ssr"},
        {EffectKind::MotionBlur,          true,  "motionblur"},
        // The exemption, and the whole point of this table.
        {EffectKind::Taa,                 false, "taa"},
        // Stages that never wanted depth are not in the skip set either - they never reach the
        // gate at all, and a true here would mean the predicate had stopped agreeing with
        // StageNeedsDepth().
        {EffectKind::Bloom,               false, "bloom"},
        {EffectKind::Gamma,               false, "gamma"},
        {EffectKind::NVScaler,            false, "nvscaler"},
        {EffectKind::None,                false, "none"},
    };

    bool allMatched = true;
    for (size_t i = 0; i < sizeof(kExpected) / sizeof(kExpected[0]); ++i) {
        bool actual = StageIsSkippedWhenDepthUnavailable(kExpected[i].stage);
        char what[160];
        snprintf(what, sizeof(what), "StageIsSkippedWhenDepthUnavailable(%s) == %s",
                 kExpected[i].name, kExpected[i].skipped ? "true" : "false");
        allMatched = Check(actual == kExpected[i].skipped, what) && allMatched;
    }
    return allMatched;
}

// Pins the set of stages that need the pre-HUD world-only colour capture - see
// StageNeedsWorldCapture() in config.h. Same shape and same honesty as CheckDepthStageSet()
// above, and the same limits apply: this is a table, not an integration run. It proves the
// predicate names every stage that actually wants the capture, which is the half that went
// wrong before (two hand-written copies of the same check, only one of which got updated when
// a second consumer showed up). It does NOT prove that AnyStageNeedsWorldCapture's own loop -
// or either of its two call sites - actually calls this predicate; that stays a reading of the
// code (post_effects.cpp's needCapture line and world_capture.cpp's own gate).
//
// Needs no GL context, so it runs before main() builds one, same as CheckDepthStageSet().
bool CheckWorldCaptureStageSet() {
    struct Expectation { EffectKind stage; bool needsCapture; const char* name; };
    const Expectation kExpected[] = {
        {EffectKind::MotionBlur,          true,  "motionblur"},
        {EffectKind::Taa,                 true,  "taa"},
        // A sample of stages that don't need it. If one of these ever starts reporting true the
        // pipeline pays for a full-resolution capture and a permanent texture every frame that
        // does not need one.
        {EffectKind::Bloom,               false, "bloom"},
        {EffectKind::Gamma,               false, "gamma"},
        {EffectKind::None,                false, "none"},
    };

    bool allMatched = true;
    for (size_t i = 0; i < sizeof(kExpected) / sizeof(kExpected[0]); ++i) {
        bool actual = StageNeedsWorldCapture(kExpected[i].stage);
        char what[160];
        snprintf(what, sizeof(what), "StageNeedsWorldCapture(%s) == %s",
                 kExpected[i].name, kExpected[i].needsCapture ? "true" : "false");
        allMatched = Check(actual == kExpected[i].needsCapture, what) && allMatched;
    }
    return allMatched;
}

// Pins which stages AnyUpscaleStageListed() treats as an upscale stage - see post_effects.h.
// Same shape and same honesty as CheckDepthStageSet() above, except the predicate takes a whole
// config rather than a single stage (a chain either has one of these listed or it doesn't), so
// each case builds a minimal one-stage config to ask the question of. This proves the predicate
// - and therefore the once-only log in ApplySelectedEffect() that supersampling makes an
// upscale stage's reconstruction redundant - fires for exactly bilinear/nvscaler/fsr and no
// other stage, including a stage that resizes without reconstructing (the implicit stretch
// fallback isn't a listed stage at all, so it can't appear here either way).
//
// Needs no GL context, so it runs before main() builds one, same as CheckDepthStageSet().
bool CheckAnyUpscaleStageListed() {
    struct Expectation { EffectKind stage; bool isUpscale; const char* name; };
    const Expectation kExpected[] = {
        {EffectKind::Bilinear,            true,  "bilinear"},
        {EffectKind::NVScaler,            true,  "nvscaler"},
        {EffectKind::Fsr,                 true,  "fsr"},
        // A sample of stages that resize without reconstructing, or don't resize at all.
        {EffectKind::Bloom,               false, "bloom"},
        {EffectKind::Taa,                 false, "taa"},
        {EffectKind::None,                false, "none"},
    };

    bool allMatched = true;
    for (size_t i = 0; i < sizeof(kExpected) / sizeof(kExpected[0]); ++i) {
        AnaxConfig config;
        config.stageCount = 1;
        config.stages[0] = kExpected[i].stage;
        bool actual = AnyUpscaleStageListed(config);
        char what[160];
        snprintf(what, sizeof(what), "AnyUpscaleStageListed({%s}) == %s",
                 kExpected[i].name, kExpected[i].isUpscale ? "true" : "false");
        allMatched = Check(actual == kExpected[i].isUpscale, what) && allMatched;
    }

    AnaxConfig empty;
    empty.stageCount = 0;
    allMatched = Check(!AnyUpscaleStageListed(empty),
                        "AnyUpscaleStageListed({}) == false") && allMatched;

    return allMatched;
}

// Pins ShouldSkipEffectChain() (post_effects.h) against all eight combinations of its three
// booleans - the truth table IS the specification here, so every row is written out rather than
// sampled. This exists because the inline version of this decision regressed silently: deleting
// the supersampling term left the suite 100% green, since nothing exercised
// ApplySelectedEffect() with an empty effect= list AND supersampling armed (post_effects_test's
// own integration run below reads the real shipped ini, which has neither). Extracting the
// three inputs as plain booleans makes that combination reachable from a fast, no-context test.
//
// stageCount is represented as "zero" rather than an int, since the predicate only ever asks
// whether it's zero - covering 0 and 1 is exactly as complete as covering 0 and 1000000.
//
// Needs no GL context, so it runs before main() builds one, same as CheckDepthStageSet().
bool CheckShouldSkipEffectChain() {
    struct Expectation {
        bool stageCountZero;
        bool hasRealUpscale;
        bool supersampleActive;
        bool skip;
        const char* name;
    };
    const Expectation kExpected[] = {
        // The two rows that matter most - see post_effects.h's comment on
        // ShouldSkipEffectChain() for why the second one is the whole reason this predicate
        // exists.
        {true,  false, false, true,  "stageCount 0, no upscale, not supersampling (today's behaviour - must not change)"},
        {true,  false, true,  false, "stageCount 0, no upscale, supersampling (the black-screen case)"},
        // The remaining six: any one of a non-empty chain, a real upscale already running, or
        // supersampling being active is independently enough to mean there IS something for
        // the present blit to show, so nothing here should ever skip.
        {true,  true,  false, false, "stageCount 0, real upscale, not supersampling"},
        {true,  true,  true,  false, "stageCount 0, real upscale, supersampling"},
        {false, false, false, false, "stageCount >0, no upscale, not supersampling"},
        {false, false, true,  false, "stageCount >0, no upscale, supersampling"},
        {false, true,  false, false, "stageCount >0, real upscale, not supersampling"},
        {false, true,  true,  false, "stageCount >0, real upscale, supersampling"},
    };

    bool allMatched = true;
    for (size_t i = 0; i < sizeof(kExpected) / sizeof(kExpected[0]); ++i) {
        int stageCount = kExpected[i].stageCountZero ? 0 : 1;
        bool actual = ShouldSkipEffectChain(stageCount, kExpected[i].hasRealUpscale,
                                             kExpected[i].supersampleActive);
        char what[192];
        snprintf(what, sizeof(what), "ShouldSkipEffectChain(%s) == %s",
                 kExpected[i].name, kExpected[i].skip ? "true" : "false");
        allMatched = Check(actual == kExpected[i].skip, what) && allMatched;
    }
    return allMatched;
}

// The same treatment for ShouldSkipAllWork() (post_effects.h), the EARLIER of ApplySelectedEffect's
// two early returns - and the one that still had no supersampling term at all after
// ShouldSkipEffectChain got one, so effect=none + frameDumpKey=0 + renderWidth/renderHeight was
// still a black screen. Sixteen combinations of four booleans, all written out.
//
// Only the all-off row skips: a frame dump key, a window size override, a non-empty chain and an
// armed render target are each independently a reason to go on.
bool CheckShouldSkipAllWork() {
    struct Expectation {
        bool stageCountZero;
        bool frameDumpKeyZero;
        bool windowSizeOverrideActive;
        bool supersampleActive;
        bool skip;
    };
    const Expectation kExpected[] = {
        {true,  true,  false, false, true},   // nothing configured at all - the only skip
        {true,  true,  false, true,  false},  // the black-screen case this fix is about
        {true,  true,  true,  false, false},
        {true,  true,  true,  true,  false},
        {true,  false, false, false, false},
        {true,  false, false, true,  false},
        {true,  false, true,  false, false},
        {true,  false, true,  true,  false},
        {false, true,  false, false, false},
        {false, true,  false, true,  false},
        {false, true,  true,  false, false},
        {false, true,  true,  true,  false},
        {false, false, false, false, false},
        {false, false, false, true,  false},
        {false, false, true,  false, false},
        {false, false, true,  true,  false},
    };

    bool allMatched = true;
    for (size_t i = 0; i < sizeof(kExpected) / sizeof(kExpected[0]); ++i) {
        int stageCount = kExpected[i].stageCountZero ? 0 : 1;
        int frameDumpKey = kExpected[i].frameDumpKeyZero ? 0 : 0x7B;
        bool actual = ShouldSkipAllWork(stageCount, frameDumpKey,
                                        kExpected[i].windowSizeOverrideActive,
                                        kExpected[i].supersampleActive);
        char what[192];
        snprintf(what, sizeof(what),
                 "ShouldSkipAllWork(stageCount %d, frameDumpKey 0x%02X, windowOverride %s, "
                 "supersampling %s) == %s",
                 stageCount, frameDumpKey,
                 kExpected[i].windowSizeOverrideActive ? "on" : "off",
                 kExpected[i].supersampleActive ? "on" : "off",
                 kExpected[i].skip ? "true" : "false");
        allMatched = Check(actual == kExpected[i].skip, what) && allMatched;
    }
    return allMatched;
}

// The present resolve's step rule - see ResolveHalvingSteps in post_effects.h. A table, because
// the thing that went wrong here was not a mis-typed comparison but a missing case: the shipped
// resolve had exactly one step for every ratio, which is correct at 2x and nowhere else, and
// the only GPU coverage it had used a 2x ratio and so could not tell.
bool CheckResolveHalvingSteps() {
    struct Row {
        int srcWidth, srcHeight, dstWidth, dstHeight, expected;
        const char* why;
    };
    const Row rows[] = {
        {640, 480, 640, 480, 0, "1:1 - supersampling off, the present blit is unchanged"},
        {1280, 960, 640, 480, 1, "2x resolves in one exact halving"},
        {1920, 1440, 640, 480, 1, "3x halves once, then the final blit covers the rest"},
        {2560, 1920, 640, 480, 2, "4x resolves in two exact halvings"},
        {3200, 2400, 640, 480, 2, "5x halves twice, then the final blit covers the rest"},
        {5120, 3840, 640, 480, 3, "8x resolves in three exact halvings"},
        // The axes are not independent: halving a source already within 2x on ONE axis would
        // shrink the other past the destination and change the aspect ratio, which the final
        // blit would then have to stretch back.
        {2560, 480, 640, 480, 0, "an axis already at 1:1 stops the other halving past it"},
        {1280, 240, 640, 480, 0, "and that holds when the source is SMALLER on that axis"},
        // Degenerate inputs reach this from a frame whose viewport has collapsed - a minimised
        // window, an engine mid-mode-change. Zero steps means the present blit behaves exactly
        // as it did before this rule existed, which is the safe direction.
        {1280, 960, 0, 0, 0, "a zero destination asks for no halvings rather than looping"},
        {0, 0, 640, 480, 0, "and so does a zero source"},
    };

    bool ok = true;
    for (const Row& row : rows) {
        int actual = ResolveHalvingSteps(row.srcWidth, row.srcHeight, row.dstWidth, row.dstHeight);
        char what[256];
        snprintf(what, sizeof(what), "resolve steps %dx%d -> %dx%d == %d (%s)",
                 row.srcWidth, row.srcHeight, row.dstWidth, row.dstHeight, row.expected, row.why);
        ok = Check(actual == row.expected, what) && ok;
    }
    return ok;
}

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

    // glViewport/glClear*/glEnable/glDisable/glScissor are all GL 1.1 core, exported directly
    // from opengl32.dll, so a plain GetProcAddress on the module resolves them - GlComputeApi
    // only carries the 4.3-era entry points the effects themselves need.
    typedef void (__stdcall *PFNGLVIEWPORTPROC)(int, int, int, int);
    typedef void (__stdcall *PFNGLCLEARCOLORPROC)(float, float, float, float);
    typedef void (__stdcall *PFNGLCLEARPROC)(unsigned int);
    typedef void (__stdcall *PFNGLENABLEPROC)(unsigned int);
    typedef void (__stdcall *PFNGLDISABLEPROC)(unsigned int);
    typedef void (__stdcall *PFNGLSCISSORPROC)(int, int, int, int);
    HMODULE glModule = GetModuleHandleA("opengl32.dll");
    PFNGLVIEWPORTPROC pGlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(glModule, "glViewport");
    PFNGLCLEARCOLORPROC pGlClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(glModule, "glClearColor");
    PFNGLCLEARPROC pGlClear = (PFNGLCLEARPROC)GetProcAddress(glModule, "glClear");
    PFNGLENABLEPROC pGlEnable = (PFNGLENABLEPROC)GetProcAddress(glModule, "glEnable");
    PFNGLDISABLEPROC pGlDisable = (PFNGLDISABLEPROC)GetProcAddress(glModule, "glDisable");
    PFNGLSCISSORPROC pGlScissor = (PFNGLSCISSORPROC)GetProcAddress(glModule, "glScissor");
    pGlViewport(0, 0, width, height);

    bool ok = CheckDepthStageSet();
    ok = CheckDepthStageSkipSet() && ok;
    ok = CheckWorldCaptureStageSet() && ok;
    ok = CheckAnyUpscaleStageListed() && ok;
    ok = CheckShouldSkipEffectChain() && ok;
    ok = CheckShouldSkipAllWork() && ok;
    ok = CheckResolveHalvingSteps() && ok;
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

    // This is the actual call under test: the same entry point wrapper.cpp calls from
    // wglSwapBuffers, reading whatever opengl32_enhancer.ini next to this .exe says.
    ApplySelectedEffect(hdc);
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

    // --- An end-to-end scenario: the present blit is a REAL resolve when supersampling is
    // armed, proven through the actual entry point rather than by driving glBlitFramebuffer
    // directly. ---
    //
    // render_target_gpu_test.cpp's averaging case proves a shrinking GL_LINEAR blit averages
    // rather than point-samples, but it drives glBlitFramebuffer itself - it says nothing about
    // whether ApplySelectedEffect() (post_effects.cpp, the actual code wrapper.cpp calls from
    // wglSwapBuffers) ever reaches that blit with the right filter and the right destination
    // rectangle when supersampling is armed the way the wrapper's own hooks arm it. This test
    // already builds a window, an HDC and a GL context, blits a known pattern onto the back
    // buffer, calls ApplySelectedEffect(hdc), and reads framebuffer 0 back - that machinery is
    // reused here rather than duplicated, right after the primary integration run above (whose
    // own PPM dumps and assertions have already completed) so mutating the config here cannot
    // disturb it, and before every scenario below so this block's own state is fully restored
    // and cannot disturb THEM either.
    //
    // stageCount = 0 (an empty chain) is also the black-screen combination
    // CheckShouldSkipEffectChain() above pins as a truth table ("stageCount 0, no upscale,
    // supersampling") - reusing it here means one case confirms, end to end, that Task 4's fix
    // actually reaches the screen, while also pinning the two properties that are this task's
    // own subject:
    //   - PRESENCE: something reaches the screen at all (the black-screen regression).
    //   - DESTINATION RECTANGLE: the image fills the WHOLE window (dstWidth/dstHeight), not
    //     just a curWidth/curHeight-sized corner of it.
    //   - FILTER: the shrink is GL_LINEAR (a real resolve), not GL_NEAREST (a point sample).
    {
        AnaxConfig& mutableConfig = GetMutableAnaxConfig();
        int savedStageCount = mutableConfig.stageCount;
        int savedRenderWidth = mutableConfig.renderWidth;
        int savedRenderHeight = mutableConfig.renderHeight;
        int savedFrameDumpKey = mutableConfig.frameDumpKey;

        mutableConfig.stageCount = 0;
        mutableConfig.renderWidth = width * 2;
        mutableConfig.renderHeight = height * 2;
        // Explicitly zero, and this is the point of the case rather than an incidental tidy-up.
        // The shipped ini leaves frameDumpKey at 0x7B, and inheriting that non-zero value was
        // enough on its own to carry this test past ApplySelectedEffect's FIRST early return -
        // so the return that had no supersampling term at all stayed green while shipping a
        // black screen for exactly this configuration (effect=none, no frame-dump key, no
        // window override, a render target armed). With it zeroed, the presence assertion below
        // is what stands between that term and a regression.
        mutableConfig.frameDumpKey = 0;

        // The same arming sequence wrapper.cpp's hooks perform every frame - see
        // render_target.h's header comment. NotifyGameViewport records the game's own
        // (pre-scale) viewport; ArmSupersampleForFrame latches whether the offscreen target is
        // actually usable this frame.
        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, width, height);
        ArmSupersampleForFrame(EnsureRenderTarget());
        bool armed = Check(IsSupersampleActive(),
                            "end-to-end resolve: supersampling armed for this frame "
                            "(if this fails, nothing below proves anything)");
        ok = armed && ok;

        if (armed) {
            // BindRenderTarget() routes subsequent drawing into the offscreen target; the
            // glViewport call is what ApplySelectedEffect() reads back via GL_VIEWPORT to learn
            // the "native" size - in production this is the wrapper's own glViewport hook,
            // already scaled by ScaleGameRect before it ever reaches real GL.
            BindRenderTarget();
            pGlViewport(0, 0, mutableConfig.renderWidth, mutableConfig.renderHeight);

            // Two vertical halves, black and white, exactly like render_target_gpu_test.cpp's
            // averaging case - so a shrinking GL_LINEAR blit must bring the seam back grey. The
            // split is renderWidth/2 MINUS ONE texel, not an exact renderWidth/2: a split sitting
            // precisely on a multiple of the downsample ratio (2, here) lands exactly on the
            // boundary between two destination pixels' non-overlapping source windows, so
            // neither destination pixel ever samples both colours and the seam stays perfectly
            // sharp even after a genuine GL_LINEAR shrink - confirmed empirically by scanning
            // pixels around the seam with the aligned split before adding this offset. Shifting
            // by one texel puts the split inside a single destination pixel's source window, so
            // that pixel is guaranteed to see both colours regardless of the exact interpolation
            // convention (nearest-pair average or half-texel-centred bilinear).
            int splitX = mutableConfig.renderWidth / 2 - 1;
            pGlEnable(GL_SCISSOR_TEST);
            pGlScissor(0, 0, splitX, mutableConfig.renderHeight);
            pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);
            pGlScissor(splitX, 0, mutableConfig.renderWidth - splitX, mutableConfig.renderHeight);
            pGlClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);
            pGlDisable(GL_SCISSOR_TEST);

            // Framebuffer 0 (the real back buffer) starts at a colour neither tone can produce
            // by averaging, nor is confused with black or white on its own - a mid-blue,
            // distinct in every channel from black (0,0,0), white (255,255,255), and their
            // average (~127,127,127). If ApplySelectedEffect() presents nothing this frame,
            // this is what a readback below would still show.
            gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            pGlClearColor(40.0f / 255.0f, 40.0f / 255.0f, 180.0f / 255.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);

            // The frame's LAST viewport is deliberately not the full-frame one. Real engines end
            // a frame on whatever sub-rectangle they drew last (a HUD element, a status bar, a
            // pillarboxed view), and render_target.h only ever claimed that the FIRST viewport of
            // a frame is the full-frame one. ApplySelectedEffect used to take the size of what
            // the game drew from GL_VIEWPORT at swap, which quietly assumed the last one as well,
            // so it would capture, size its pipeline for and present this quarter-sized rectangle
            // as though it were the whole image. It takes that size from GetRenderTargetSize()
            // instead - known rather than inferred - so all three assertions below hold with this
            // sub-viewport in force exactly as they did without it.
            pGlViewport(0, 0, mutableConfig.renderWidth / 4, mutableConfig.renderHeight / 4);

            ApplySelectedEffect(hdc);
            ok = Check(gl.glGetError() == 0,
                       "end-to-end resolve: ApplySelectedEffect() left no GL error") && ok;

            std::vector<unsigned char> resolvedPixels((size_t)width * height * 4);
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            gl.glReadBuffer(GL_BACK);
            gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, resolvedPixels.data());
            ok = Check(gl.glGetError() == 0,
                       "end-to-end resolve: readback of the real back buffer left no GL error") && ok;

            auto sampleAt = [&](int x, int y, unsigned char out[4]) {
                size_t i = ((size_t)y * width + x) * 4;
                out[0] = resolvedPixels[i + 0];
                out[1] = resolvedPixels[i + 1];
                out[2] = resolvedPixels[i + 2];
                out[3] = resolvedPixels[i + 3];
            };

            // PRESENCE: well inside the left (black) half. The black-screen regression would
            // leave this at the mid-blue clear colour.
            unsigned char leftPixel[4];
            sampleAt(width / 4, height / 2, leftPixel);
            bool presented = leftPixel[0] < 50 && leftPixel[1] < 50 && leftPixel[2] < 50;
            printf("end-to-end resolve: presence pixel rgb = %d,%d,%d (expected black-ish)\n",
                   leftPixel[0], leftPixel[1], leftPixel[2]);
            ok = Check(presented,
                       "end-to-end resolve: presence - something was presented, not the "
                       "black-screen regression") && ok;

            // DESTINATION RECTANGLE: a few pixels in from the far (high x, high y) corner,
            // deep in the white half once the WHOLE window is correctly filled at the right
            // scale. Deliberately checked as "specifically white", not "black or white": if the
            // destination rectangle were curWidth/curHeight (the native/offscreen size, here
            // bigger than the window) instead of dstWidth/dstHeight, glBlitFramebuffer's
            // oversized destination rect gets implicitly clipped to the real (smaller)
            // framebuffer, which does not just shrink the image into a corner and leave the
            // rest at the clear colour - it changes the EFFECTIVE scale of the whole blit to
            // curWidth/curWidth (i.e. 1:1), so the visible window ends up showing an unscaled
            // crop of the SOURCE's own top-left corner. With this test's black-left/white-right
            // source layout that crop is still almost entirely black at this coordinate -
            // confirmed empirically by running this exact mutation - so a loose "black or white"
            // check would have passed for the wrong reason. Requiring white specifically catches
            // it: only the correctly-scaled destination rectangle can put white this deep into
            // the far corner.
            unsigned char farPixel[4];
            sampleAt(width - 5, height - 5, farPixel);
            bool farIsWhite = farPixel[0] > 200 && farPixel[1] > 200 && farPixel[2] > 200;
            printf("end-to-end resolve: far-corner pixel rgb = %d,%d,%d (expected white, not "
                   "the clear colour or a wrongly-scaled crop)\n", farPixel[0], farPixel[1], farPixel[2]);
            ok = Check(farIsWhite,
                       "end-to-end resolve: destination rectangle - the image fills the whole "
                       "window, not just a corner of it") && ok;

            // FILTER: straddling the seam. GL_NEAREST can only ever return one tone or the
            // other; GL_LINEAR must bring back something in between - same 60..195 band as
            // render_target_gpu_test.cpp's averaging case, for the same reason (drivers differ
            // in exactly how many source texels a shrinking GL_LINEAR blit weighs). The seam in
            // DESTINATION space lands at splitX / 2 (the same one-texel-off-multiple split
            // above, scaled by the 2:1 ratio) - verified empirically by scanning pixels around
            // width/2 with the aligned split first: destination pixel width/2 itself is one
            // pixel short of the actual straddling pixel here (splitX/2 == width/2 - 1), because
            // splitX is renderWidth/2 - 1, not renderWidth/2.
            int seamX = splitX / 2;
            unsigned char seamPixel[4];
            sampleAt(seamX, height / 2, seamPixel);
            bool seamIsIntermediate = seamPixel[0] > 60 && seamPixel[0] < 195;
            printf("end-to-end resolve: seam pixel rgb = %d,%d,%d (expected an intermediate "
                   "grey)\n", seamPixel[0], seamPixel[1], seamPixel[2]);
            ok = Check(seamIsIntermediate,
                       "end-to-end resolve: filter - the shrink averaged the seam rather than "
                       "point-sampling one side of it") && ok;

            // FALLBACK DESTINATION: the same frame again with a null HDC, which is the one way
            // to make GetWindowClientSize() fail from here (WindowFromDC(nullptr) is NULL) -
            // the real-world equivalent being a window that has gone away between the game's
            // draw and this swap. hdc is the only thing ApplySelectedEffect uses it for, so
            // nothing else about the frame changes.
            //
            // The fallback used to be nativeWidth/nativeHeight, which on a supersampled frame is
            // the RENDER TARGET's size - larger than the window - so the present blit became a
            // 1:1 copy of an oversized image and the window showed an unscaled crop of its
            // top-left corner. That is the same cropped-corner failure the destination-rectangle
            // assertion above describes, arrived at from the other direction. The game's own
            // full-frame viewport is the right stand-in, and it is what this asserts: the far
            // corner is still white.
            gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            pGlClearColor(40.0f / 255.0f, 40.0f / 255.0f, 180.0f / 255.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);

            ApplySelectedEffect(nullptr);
            ok = Check(gl.glGetError() == 0,
                       "end-to-end resolve (no client size): ApplySelectedEffect() left no GL "
                       "error") && ok;

            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            gl.glReadBuffer(GL_BACK);
            gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, resolvedPixels.data());

            unsigned char fallbackFarPixel[4];
            sampleAt(width - 5, height - 5, fallbackFarPixel);
            bool fallbackFarIsWhite = fallbackFarPixel[0] > 200 && fallbackFarPixel[1] > 200 &&
                                       fallbackFarPixel[2] > 200;
            printf("end-to-end resolve (no client size): far-corner pixel rgb = %d,%d,%d "
                   "(expected white)\n", fallbackFarPixel[0], fallbackFarPixel[1],
                   fallbackFarPixel[2]);
            ok = Check(fallbackFarIsWhite,
                       "end-to-end resolve (no client size): an unavailable window size falls "
                       "back to the game's own viewport, not to the supersampled size") && ok;
        }

        // Restore what this block changed so every scenario below runs exactly as it did
        // before this case existed.
        mutableConfig.stageCount = savedStageCount;
        mutableConfig.renderWidth = savedRenderWidth;
        mutableConfig.renderHeight = savedRenderHeight;
        mutableConfig.frameDumpKey = savedFrameDumpKey;
        ResetRenderTargetState();
        pGlViewport(0, 0, width, height);
    }

    // --- The resolve at a ratio ABOVE 2x, where a single bilinear tap is not enough. ---
    //
    // Every other resolve case in this suite - here and in render_target_gpu_test.cpp - uses a
    // 2x ratio, and 2x is the one ratio at which the old single-blit resolve was correct. That
    // is why none of them could see the bug: a shrinking GL_LINEAR blit is ONE bilinear tap, so
    // at 2x it weighs all four source texels of each destination pixel and at 4x it weighs four
    // of sixteen. renderWidth/renderHeight are free-form numbers, so ratios above 2x are just
    // what anyone typing a big value gets.
    //
    // The pattern is chosen so the two behaviours cannot agree. At 4x, destination pixel i
    // covers source columns [4i, 4i+4), and the bilinear tap for it sits at source coordinate
    // 4i+2 - exactly on the boundary between texels 4i+1 and 4i+2, so it returns the average of
    // those TWO and ignores the other fourteen texels in the box. Painting every fourth column
    // white (x % 4 == 3) therefore splits the answers cleanly:
    //
    //   a true 4x4 box average -> 1 white column in 4 -> ~64
    //   a single bilinear tap   -> texels 4i+1 and 4i+2, both black -> ~0
    //
    // A seam-straddling pattern like the 2x case above cannot do this job: the seam is where a
    // tap and a box happen to agree, which is exactly why that case passes either way.
    {
        AnaxConfig& mutableConfig = GetMutableAnaxConfig();
        int savedStageCount = mutableConfig.stageCount;
        int savedRenderWidth = mutableConfig.renderWidth;
        int savedRenderHeight = mutableConfig.renderHeight;
        int savedFrameDumpKey = mutableConfig.frameDumpKey;

        mutableConfig.stageCount = 0;
        mutableConfig.renderWidth = width * 4;
        mutableConfig.renderHeight = height * 4;
        mutableConfig.frameDumpKey = 0;

        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, width, height);
        ArmSupersampleForFrame(EnsureRenderTarget());
        bool armed4x = Check(IsSupersampleActive(),
                             "4x resolve: supersampling armed at a 4x ratio "
                             "(if this fails, nothing below proves anything)");
        ok = armed4x && ok;

        if (armed4x) {
            BindRenderTarget();
            pGlViewport(0, 0, mutableConfig.renderWidth, mutableConfig.renderHeight);

            pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);
            pGlEnable(GL_SCISSOR_TEST);
            pGlClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            for (int x = 3; x < mutableConfig.renderWidth; x += 4) {
                pGlScissor(x, 0, 1, mutableConfig.renderHeight);
                pGlClear(GL_COLOR_BUFFER_BIT);
            }
            pGlDisable(GL_SCISSOR_TEST);

            gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            pGlClearColor(40.0f / 255.0f, 40.0f / 255.0f, 180.0f / 255.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);

            ApplySelectedEffect(hdc);
            ok = Check(gl.glGetError() == 0,
                       "4x resolve: ApplySelectedEffect() left no GL error") && ok;

            std::vector<unsigned char> pixels4x((size_t)width * height * 4);
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            gl.glReadBuffer(GL_BACK);
            gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels4x.data());

            // Sampled across the middle row rather than at one pixel: a box average of this
            // pattern is the same everywhere, so the interesting failure is not one wrong pixel
            // but a whole row that came back at the wrong level. The edge columns are skipped
            // because a blit's filtering at the very edge of the source has nothing outside to
            // weigh and drivers clamp differently there.
            int lowest = 255, highest = 0;
            long long total = 0;
            int counted = 0;
            for (int x = 4; x < width - 4; ++x) {
                int value = pixels4x[((size_t)(height / 2) * width + x) * 4];
                lowest = value < lowest ? value : lowest;
                highest = value > highest ? value : highest;
                total += value;
                ++counted;
            }
            int mean = (int)(total / counted);
            printf("4x resolve: middle row red channel min=%d mean=%d max=%d "
                   "(a 4x4 box average of this pattern is 64; a single bilinear tap is 0)\n",
                   lowest, mean, highest);

            // The band is wide on purpose. 64 is what an exact box gives, but the last step of
            // the resolve is still a real GL blit and drivers round differently; what no
            // single-tap implementation can produce is anything near a quarter-white average.
            ok = Check(mean >= 40 && mean <= 90,
                       "4x resolve: the resolved image carries the quarter-white average of "
                       "the whole 4x4 box, not the two texels a single bilinear tap reads") && ok;

            // Uniformity is the other half of the claim, and it is what separates "averaged
            // something" from "averaged the right box": this pattern has the same content under
            // every destination pixel, so a correct resolve is flat. A partial box - or a tap
            // that drifts across the pattern's phase - comes back striped.
            ok = Check(highest - lowest <= 24,
                       "4x resolve: and it is flat across the row, as a correct box average of "
                       "a uniformly periodic pattern must be") && ok;
        }

        mutableConfig.stageCount = savedStageCount;
        mutableConfig.renderWidth = savedRenderWidth;
        mutableConfig.renderHeight = savedRenderHeight;
        mutableConfig.frameDumpKey = savedFrameDumpKey;
        ResetRenderTargetState();
        pGlViewport(0, 0, width, height);
    }

    // --- The same end-to-end resolve, beside the empty-chain case above, but with a NON-EMPTY
    // effect chain - the configuration a real user actually runs (a full effect= list AND a
    // render size). ---
    //
    // The empty-chain case above found a real bug in the capture/pair-selection code
    // (g_pipeline.hasNativePair vs hasRealUpscale - see post_effects.cpp) that both the
    // stageCount==0 and stageCount>0 paths share. Proving the fix only against stageCount==0
    // would leave the actually-shipped configuration - a real chain running while supersampling
    // is armed - unverified, so this case exercises the same present-resolve path with one
    // stage actually listed.
    //
    // Stage choice: `gamma`, with gamma=1.0 and brightness=0.5. gamma.h documents that gamma=1
    // skips pow() outright ("gamma=1 with brightness=1 reproduces the input EXACTLY - the pow()
    // is skipped outright"), so at gamma=1 the whole per-pixel transform is a single multiply,
    // c = max(color*brightness, 0) - no transcendental function, nothing to reason about beyond
    // arithmetic. That makes it the cheapest stage in the pipeline to predict exactly, which
    // matters here: this case DERIVES its expected pixel values from that formula rather than
    // hardcoding whatever a prior run happened to print - a fixed expected value copied from
    // observed output would pin this driver's behaviour instead of the production code's.
    //   - Black is an exact fixed point of the formula for any brightness >= 0 (0 * x is 0), so
    //     it stays exactly 0 regardless of the stage - the presence check below is unaffected.
    //   - White (1.0) becomes exactly `brightness` = 0.5, i.e. 127.5/255 - NOT 255 any more.
    //     brightness is deliberately NOT left at 1.0 for this reason: if the wrong texture were
    //     presented (the one BEFORE the chain ran, rather than the one it actually last wrote -
    //     see post_effects.cpp's `cur` ping-pong index), the far corner would still read close
    //     to 255, not the derived ~127.5, and the destination-rectangle assertion below would
    //     catch that mismatch. A brightness of 1.0 could not have told the two cases apart.
    {
        AnaxConfig& mutableConfig = GetMutableAnaxConfig();
        int savedStageCount2 = mutableConfig.stageCount;
        int savedRenderWidth2 = mutableConfig.renderWidth;
        int savedRenderHeight2 = mutableConfig.renderHeight;
        EffectKind savedStage0 = mutableConfig.stages[0];
        float savedGamma = mutableConfig.gamma;
        float savedBrightness = mutableConfig.brightness;
        bool savedFxIndicator = mutableConfig.fxIndicator;

        const float kBrightness = 0.5f;
        const float kGamma = 1.0f;  // skips pow() per gamma.h - a pure linear scale.
        mutableConfig.stageCount = 1;
        mutableConfig.stages[0] = EffectKind::Gamma;
        mutableConfig.gamma = kGamma;
        mutableConfig.brightness = kBrightness;
        // The FX badge draws, unconditionally, on the final texture's own corner whenever
        // stageCount > 0 - the same corner this case samples for its destination-rectangle
        // check (see the gamma-wiring scenario below for the same reasoning). Off for the same
        // reason: leaving it on would supply a pixel difference of its own and confound the
        // assertion this case is actually trying to make.
        mutableConfig.fxIndicator = false;
        mutableConfig.renderWidth = width * 2;
        mutableConfig.renderHeight = height * 2;

        ResetRenderTargetState();
        NotifyFrameBoundary();
        NotifyGameViewport(0, 0, width, height);
        ArmSupersampleForFrame(EnsureRenderTarget());
        bool armed2 = Check(IsSupersampleActive(),
                             "end-to-end resolve (gamma chain): supersampling armed for this "
                             "frame (if this fails, nothing below proves anything)");
        ok = armed2 && ok;

        if (armed2) {
            BindRenderTarget();
            pGlViewport(0, 0, mutableConfig.renderWidth, mutableConfig.renderHeight);

            // Same one-texel-off-multiple split as the empty-chain case above, and for the same
            // reason: a split exactly on the 2:1 boundary lands on the edge between two
            // destination pixels' non-overlapping source windows, and neither one would ever
            // see both tones.
            int splitX2 = mutableConfig.renderWidth / 2 - 1;
            pGlEnable(GL_SCISSOR_TEST);
            pGlScissor(0, 0, splitX2, mutableConfig.renderHeight);
            pGlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);
            pGlScissor(splitX2, 0, mutableConfig.renderWidth - splitX2,
                       mutableConfig.renderHeight);
            pGlClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);
            pGlDisable(GL_SCISSOR_TEST);

            gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            pGlClearColor(40.0f / 255.0f, 40.0f / 255.0f, 180.0f / 255.0f, 1.0f);
            pGlClear(GL_COLOR_BUFFER_BIT);

            ApplySelectedEffect(hdc);
            ok = Check(gl.glGetError() == 0,
                       "end-to-end resolve (gamma chain): ApplySelectedEffect() left no GL "
                       "error") && ok;

            std::vector<unsigned char> resolvedPixels2((size_t)width * height * 4);
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            gl.glReadBuffer(GL_BACK);
            gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                             resolvedPixels2.data());
            ok = Check(gl.glGetError() == 0,
                       "end-to-end resolve (gamma chain): readback of the real back buffer "
                       "left no GL error") && ok;

            auto sampleAt2 = [&](int x, int y, unsigned char out[4]) {
                size_t i = ((size_t)y * width + x) * 4;
                out[0] = resolvedPixels2[i + 0];
                out[1] = resolvedPixels2[i + 1];
                out[2] = resolvedPixels2[i + 2];
                out[3] = resolvedPixels2[i + 3];
            };

            // Derived expected channel values - see the block comment above for the formula
            // each one comes from.
            const float expectedBlackChannel = 0.0f;
            const float expectedWhiteChannel = kBrightness * 255.0f;  // 127.5

            // PRESENCE: well inside the left (black) half. Black is a fixed point of the gamma
            // stage's formula, so the same tight bound as the empty-chain case still applies
            // unchanged.
            unsigned char leftPixel[4];
            sampleAt2(width / 4, height / 2, leftPixel);
            bool presented2 = leftPixel[0] < 50 && leftPixel[1] < 50 && leftPixel[2] < 50;
            printf("end-to-end resolve (gamma chain): presence pixel rgb = %d,%d,%d (expected "
                   "black-ish)\n", leftPixel[0], leftPixel[1], leftPixel[2]);
            ok = Check(presented2,
                       "end-to-end resolve (gamma chain): presence - something was presented, "
                       "not the black-screen regression") && ok;

            // DESTINATION RECTANGLE: a few pixels in from the far (high x, high y) corner, deep
            // in the (gamma-transformed) white half. Banded around the DERIVED
            // expectedWhiteChannel (~127.5) rather than a hardcoded 255, with the same +/-45
            // margin the empty-chain case effectively used around ITS derived value (255) -
            // wide enough for blit/8-bit-rounding slop, narrow enough to fail both if the
            // destination rectangle is wrong (which crops to an almost-all-black region, per
            // the empty-chain case's investigation - comfortably below this band) and if the
            // WRONG, still-255 pre-gamma texture were presented instead of the chain's actual
            // last-written one (comfortably above this band).
            unsigned char farPixel[4];
            sampleAt2(width - 5, height - 5, farPixel);
            bool farMatchesTransformedWhite =
                farPixel[0] > expectedWhiteChannel - 45.0f && farPixel[0] < expectedWhiteChannel + 45.0f &&
                farPixel[1] > expectedWhiteChannel - 45.0f && farPixel[1] < expectedWhiteChannel + 45.0f &&
                farPixel[2] > expectedWhiteChannel - 45.0f && farPixel[2] < expectedWhiteChannel + 45.0f;
            printf("end-to-end resolve (gamma chain): far-corner pixel rgb = %d,%d,%d (expected "
                   "~%.1f - the gamma-transformed white, not 255 or the clear colour)\n",
                   farPixel[0], farPixel[1], farPixel[2], expectedWhiteChannel);
            ok = Check(farMatchesTransformedWhite,
                       "end-to-end resolve (gamma chain): destination rectangle - the chain's "
                       "actual output fills the whole window, and it is the chain's last-"
                       "written texture that was presented") && ok;

            // FILTER: straddling the seam. The present blit mixes the ALREADY gamma-transformed
            // pixel values (gamma runs before the present blit, at native/offscreen
            // resolution), so the expected band is the SAME 60/255..195/255 fraction of the way
            // between the two tones the empty-chain case already justified, rescaled to this
            // stage's own output range [expectedBlackChannel, expectedWhiteChannel] instead of
            // [0, 255] - not a fresh tolerance, the same one, applied to the transformed range.
            //
            // Checked on ALL THREE channels, not just channel 0: this range is narrower than
            // the empty-chain case's (0..127.5 rather than 0..255), and the clear colour's own
            // R and G channels (40) land inside it by coincidence - found empirically by running
            // the ShouldSkipEffectChain mutation below against a single-channel version of this
            // check, which passed for the wrong reason (the pixel was still the untouched clear
            // colour, R=G=40, B=180 - not a real seam blend). The clear colour is not achromatic
            // like every legitimate value in this test (black, transformed white, and their
            // blends all have R==G==B), so requiring all three channels in-band is what actually
            // rules it out.
            int seamX2 = splitX2 / 2;
            unsigned char seamPixel[4];
            sampleAt2(seamX2, height / 2, seamPixel);
            float seamRange = expectedWhiteChannel - expectedBlackChannel;
            float seamBandLo = expectedBlackChannel + seamRange * (60.0f / 255.0f);
            float seamBandHi = expectedBlackChannel + seamRange * (195.0f / 255.0f);
            bool seamIsIntermediate2 = seamPixel[0] > seamBandLo && seamPixel[0] < seamBandHi &&
                                       seamPixel[1] > seamBandLo && seamPixel[1] < seamBandHi &&
                                       seamPixel[2] > seamBandLo && seamPixel[2] < seamBandHi;
            printf("end-to-end resolve (gamma chain): seam pixel rgb = %d,%d,%d (expected "
                   "between %.1f and %.1f)\n", seamPixel[0], seamPixel[1], seamPixel[2],
                   seamBandLo, seamBandHi);
            ok = Check(seamIsIntermediate2,
                       "end-to-end resolve (gamma chain): filter - the shrink averaged the "
                       "seam rather than point-sampling one side of it") && ok;
        }

        // Restore what this block changed so every scenario below runs exactly as it did
        // before this case existed.
        mutableConfig.stageCount = savedStageCount2;
        mutableConfig.renderWidth = savedRenderWidth2;
        mutableConfig.renderHeight = savedRenderHeight2;
        mutableConfig.stages[0] = savedStage0;
        mutableConfig.gamma = savedGamma;
        mutableConfig.brightness = savedBrightness;
        mutableConfig.fxIndicator = savedFxIndicator;
        ResetRenderTargetState();
        pGlViewport(0, 0, width, height);
    }

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

        ApplySelectedEffect(hdc);

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

    // --- A third scenario: a REAL upscale - the game renders at `width`x`height` but the real
    // window is bigger. ---
    //
    // Grows the test window's client area without touching glViewport (still `width`x`height`) -
    // exactly what windowWidth/windowHeight (see window_override.h) produces in practice: the
    // game keeps rendering at its own resolution, the window around it is bigger. This is what
    // should turn `bilinear` into an actual reconstruction from a genuinely smaller source, not
    // the same-size "preview a lower-res look" round-trip `scale` alone does on a full-size
    // capture (see fsr.h's header comment and ApplySelectedEffect()'s in post_effects.cpp).
    {
        const int dstSize = width * 2;
        RECT bigWindowRect = {0, 0, dstSize, dstSize};
        AdjustWindowRect(&bigWindowRect, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(hwnd, nullptr, 0, 0, bigWindowRect.right - bigWindowRect.left,
                     bigWindowRect.bottom - bigWindowRect.top, SWP_NOMOVE | SWP_NOZORDER);

        RECT grownClientRect = {};
        GetClientRect(hwnd, &grownClientRect);
        bool grown = Check(grownClientRect.right - grownClientRect.left == dstSize &&
                            grownClientRect.bottom - grownClientRect.top == dstSize,
                            "real upscale: test window grew to a real 2x client area");
        // Folded into `ok` as well as gating the block below: if the resize doesn't take, every
        // assertion that actually exercises the upscale path is skipped, and this test must fail
        // rather than report a pass for checks it never ran.
        ok = grown && ok;

        if (grown) {
            AnaxConfig& mutableConfig = GetMutableAnaxConfig();
            mutableConfig.fxIndicator = false;
            mutableConfig.stageCount = 1;
            mutableConfig.stages[0] = EffectKind::Bilinear;
            // Deliberately NOT 0.5 (the "correct" ratio, if this were read at all): proves the
            // real native/dst texture sizes drive the reconstruction, not this config value.
            mutableConfig.scale = 1.0f;

            // Re-paint the test pattern into the (still `width`x`height`) region glViewport
            // covers - exactly where the game "rendered" this frame.
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, patternFbo);
            gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

            ApplySelectedEffect(hdc);
            ok = Check(gl.glGetError() == 0, "real upscale: ApplySelectedEffect() left no GL error") && ok;

            std::vector<unsigned char> upscaledPixels((size_t)dstSize * dstSize * 4);
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            gl.glReadBuffer(GL_BACK);
            gl.glReadPixels(0, 0, dstSize, dstSize, GL_RGBA, GL_UNSIGNED_BYTE, upscaledPixels.data());
            ok = Check(gl.glGetError() == 0, "real upscale: readback at the real (bigger) window size left no GL error") && ok;

            // The bright highlight patch sits at native x<width/8,y<height/8 (GL bottom-left
            // origin - see BuildTestPattern/WritePpm's header comments), so after a clean 2x
            // reconstruction it must show up near the SAME corner of the bigger window.
            size_t cornerI = ((size_t)10 * dstSize + 10) * 4;
            bool cornerBright = upscaledPixels[cornerI] > 200 && upscaledPixels[cornerI + 1] > 200 &&
                                 upscaledPixels[cornerI + 2] > 200;
            ok = Check(cornerBright,
                       "real upscale: the source's bright corner reconstructed near the same corner of the bigger window") && ok;

            // The real test: did the WHOLE window get filled with reconstructed content, or only
            // a width x height corner of it (the bug this feature exists to fix)? A clean
            // upscale roughly preserves average brightness; a pipeline that only wrote the small
            // native image into one corner and left the rest of a much bigger canvas at
            // whatever it was before (typically much darker) would pull the whole-image mean
            // well below the original pattern's.
            double inputMeanAll = 0.0, upscaledMeanAll = 0.0;
            for (size_t i = 0; i < inputPixels.size(); i += 4) {
                inputMeanAll += inputPixels[i] + inputPixels[i + 1] + inputPixels[i + 2];
            }
            inputMeanAll /= (double)(inputPixels.size() / 4) * 3.0;
            for (size_t i = 0; i < upscaledPixels.size(); i += 4) {
                upscaledMeanAll += upscaledPixels[i] + upscaledPixels[i + 1] + upscaledPixels[i + 2];
            }
            upscaledMeanAll /= (double)(upscaledPixels.size() / 4) * 3.0;
            printf("real upscale: whole-image mean channel %.2f (source) vs %.2f (2x upscaled)\n",
                   inputMeanAll, upscaledMeanAll);
            ok = Check(fabs(upscaledMeanAll - inputMeanAll) < 20.0,
                       "real upscale: the WHOLE bigger window's mean brightness matches the source, "
                       "not just a corner of it") && ok;
        }
    }

    // --- A fourth scenario: an OFFSET viewport must NOT be treated as a small render. ---
    //
    // A game whose final pass targets a region of a full-size back buffer (pillarboxed 4:3
    // content in a wider window, a letterboxed cutscene) is not rendering small-and-then-scaled.
    // The window is still 2x here, so the only thing separating this from the genuine upscale
    // above is the viewport's non-zero origin - if that alone isn't enough to reject it, the
    // rendered region gets stretched over the whole window and the image visibly jumps.
    {
        const int dstSize = width * 2;
        const int offset = 64;

        AnaxConfig& mutableConfig = GetMutableAnaxConfig();
        mutableConfig.fxIndicator = false;
        mutableConfig.stageCount = 1;
        mutableConfig.stages[0] = EffectKind::Bilinear;

        // A distinctive full-window background, so anything the pipeline writes outside the
        // rendered region is unmistakable. Cleared through a full-window viewport, since
        // glClear is itself viewport-independent but glScissor/viewport state must not be
        // left over from the previous scenario.
        pGlViewport(0, 0, dstSize, dstSize);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        pGlClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        pGlClear(GL_COLOR_BUFFER_BIT);

        // The game's content, drawn into the bottom-left corner of that bigger buffer...
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, patternFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // ...and an OFFSET sub-viewport left current, the way a pillarboxing game would.
        pGlViewport(offset, offset, width, height);

        ApplySelectedEffect(hdc);
        ok = Check(gl.glGetError() == 0, "offset viewport: ApplySelectedEffect() left no GL error") && ok;

        std::vector<unsigned char> offsetPixels((size_t)dstSize * dstSize * 4);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.glReadBuffer(GL_BACK);
        gl.glReadPixels(0, 0, dstSize, dstSize, GL_RGBA, GL_UNSIGNED_BYTE, offsetPixels.data());
        ok = Check(gl.glGetError() == 0, "offset viewport: readback left no GL error") && ok;

        // Far outside the rendered region: must still be the background. If an offset viewport
        // were mistaken for a small render, reconstructed game content would cover this.
        size_t farI = ((size_t)(dstSize - 40) * dstSize + (dstSize - 40)) * 4;
        bool stillBackground = offsetPixels[farI] < 60 && offsetPixels[farI + 1] > 195 &&
                                offsetPixels[farI + 2] < 60;
        printf("offset viewport: far pixel rgb = %d,%d,%d (expected the green background)\n",
               offsetPixels[farI], offsetPixels[farI + 1], offsetPixels[farI + 2]);
        ok = Check(stillBackground,
                   "offset viewport: content was NOT stretched over the whole window") && ok;

        pGlViewport(0, 0, width, height);
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
