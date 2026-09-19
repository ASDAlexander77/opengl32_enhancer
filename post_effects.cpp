// Owns the shared post-effect pipeline: two RGBA16F ping-pong textures. Captures the real
// back buffer into them exactly once per frame, chains every stage the config listed - in the
// order it listed them - by alternating which texture is "source" and which is "destination"
// (each stage's ApplyX reads one and writes the other, returning whether it actually wrote -
// see e.g. lut_grading.h's header comment for why a stage can legitimately no-op), and blits
// the final result back to the real back buffer exactly once. This replaces the old design where every effect captured
// and blitted independently (round-tripping through its own 8-bit RGBA8 texture even though
// five effects chain together in one frame) - working in float and capturing/blitting once
// meaningfully reduces the banding/precision loss that repeated 8-bit round-trips caused.
//
// Also owns the single GL state save/restore around the whole chain: since nothing outside
// this pipeline runs between the capture and the final blit, one outer save/restore here
// covers every stage - individual ApplyX functions don't need (and don't do) their own.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "post_effects.h"
#include "config.h"
#include "debug_log.h"
#include "pixel_invert.h"
#include "bilinear_upscale.h"
#include "nis_effect.h"
#include "hdr_look.h"
#include "bloom.h"
#include "lut_grading.h"
#include "vignette.h"
#include "chromatic_aberration.h"
#include "taa.h"
#include "dither.h"
#include "gamma.h"
#include "smaa.h"
#include "fsr.h"
#include "cas.h"
#include "nr.h"
#include "local_contrast.h"
#include "fx_indicator.h"
#include "depth_vignette.h"
#include "dof.h"
#include "fog.h"
#include "light_shafts.h"
#include "ssr.h"
#include "ssao.h"
#include "modelview_capture.h"
#include "projection_capture.h"
#include "frame_dump.h"
#include "gl_loader.h"
#include "window_override.h"

// Same no-<windows.h> discipline as the rest of this DLL (see config.cpp's equivalent block and
// wrapper.cpp's header comment): <windows.h> declares dllimport wgl*/gl* names that collide with
// wrapper.cpp's dllexport definitions of the same names. GetAsyncKeyState reads the keyboard
// without needing a message pump or a window hook, which is exactly why the frame dump is on a
// polled hotkey rather than a WndProc subclass - nothing about this DLL's relationship with the
// host process changes.
extern "C" {
    __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
}

namespace {

const unsigned int GL_VIEWPORT                 = 0x0BA2;
const unsigned int GL_BACK                     = 0x0405;
const unsigned int GL_TEXTURE_2D               = 0x0DE1;
const unsigned int GL_TEXTURE0                 = 0x84C0;
const unsigned int GL_ACTIVE_TEXTURE           = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D       = 0x8069;
const unsigned int GL_TEXTURE_MIN_FILTER       = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER       = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S           = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T           = 0x2803;
const unsigned int GL_LINEAR                   = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE            = 0x812F;
const unsigned int GL_RGBA16F                  = 0x881A;
const unsigned int GL_DEPTH_COMPONENT24        = 0x81A6;
const unsigned int GL_DEPTH_ATTACHMENT         = 0x8D00;
const unsigned int GL_DEPTH_BUFFER_BIT         = 0x00000100;
const unsigned int GL_CURRENT_PROGRAM          = 0x8B8D;
const unsigned int GL_FRAMEBUFFER              = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER         = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER         = 0x8CA9;
const unsigned int GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const unsigned int GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const unsigned int GL_COLOR_ATTACHMENT0        = 0x8CE0;
const unsigned int GL_COLOR_BUFFER_BIT         = 0x00004000;
const unsigned int GL_NEAREST                  = 0x2600;
const unsigned int GL_UNIFORM_BUFFER           = 0x8A11;
const unsigned int GL_UNIFORM_BUFFER_BINDING   = 0x8A28;
const unsigned int GL_RGBA                     = 0x1908;
const unsigned int GL_UNSIGNED_BYTE            = 0x1401;
const unsigned int GL_DEPTH_COMPONENT          = 0x1902;
const unsigned int GL_FLOAT                    = 0x1406;
const unsigned int GL_NO_ERROR                 = 0;

struct PipelineTextures {
    bool valid = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    // The DESTINATION-resolution ping-pong pair - the size most stages run at, and what every
    // stage after a real upscale (see ApplySelectedEffect()) runs at. Equal to the game's own
    // native render resolution whenever the real window isn't bigger than that (the common
    // case), in which case this is the ONLY pair in use, exactly as before this feature existed.
    int width = 0;
    int height = 0;
    unsigned int tex[2] = {0, 0};
    unsigned int presentFbo = 0;   // reused, re-attached to whichever tex[] is final each frame

    // EXPERIMENTAL (see depth_vignette.h). The default framebuffer's depth attachment, blitted
    // here once per frame, but only when a depth-consuming stage is actually listed - see
    // ApplySelectedEffect(). depthFbo exists purely as a blit target for depthTex; nothing ever
    // reads from it as a framebuffer otherwise. ALWAYS sized at the game's own native render
    // resolution (independent of width/height above) - that's the only resolution a depth buffer
    // actually exists at.
    unsigned int depthTex = 0;
    unsigned int depthFbo = 0;
    int depthWidth = 0;
    int depthHeight = 0;

    // A second ping-pong pair, sized at the game's own native render resolution, that only
    // exists while that native resolution is smaller than the real window (see
    // ApplySelectedEffect()): the initial capture and any stage listed before the upscaler run
    // here, until an upscale-capable stage (bilinear/nvscaler/fsr) copies the image over into
    // tex[]/width/height above at the window's real size.
    bool hasNativePair = false;
    int nativeWidth = 0;
    int nativeHeight = 0;
    unsigned int nativeTex[2] = {0, 0};
};

PipelineTextures g_pipeline;

unsigned int CreatePipelineTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return texture;
}

// EXPERIMENTAL (see depth_vignette.h). GL_NEAREST rather than GL_LINEAR: interpolating across a
// depth discontinuity (a near foreground edge against distant background) would blend two
// unrelated depths into a meaningless mid-value.
unsigned int CreateDepthTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, width, height);
    return texture;
}

// `nativeWidth/nativeHeight` is the game's own render resolution (GL_VIEWPORT); `dstWidth/
// dstHeight` is the real window's client size - equal to native in the common case, larger when
// windowWidth/windowHeight (or a manual resize) makes the window bigger than what the game
// itself renders, which is what turns an upscale-capable stage into a REAL upscale rather than
// the same-size preview `scale` alone gives (see ApplySelectedEffect()'s header comment).
void EnsurePipelineTextures(const GlComputeApi& gl, int nativeWidth, int nativeHeight,
                             int dstWidth, int dstHeight, bool needDepth) {
    // A context change invalidates every cached texture/FBO. Drop the handles rather than
    // deleting them (the owning context freed them already, and glDelete* now would hit
    // unrelated objects in the current context); everything below then recreates them.
    if (g_pipeline.generation != GetGlContextGeneration()) {
        g_pipeline = PipelineTextures{};
        g_pipeline.generation = GetGlContextGeneration();
    }

    // The destination-resolution pair - recreated only when that size actually changes.
    if (!g_pipeline.valid || g_pipeline.width != dstWidth || g_pipeline.height != dstHeight) {
        if (g_pipeline.valid) {
            gl.glDeleteTextures(2, g_pipeline.tex);
        }
        if (g_pipeline.presentFbo == 0) {
            gl.glGenFramebuffers(1, &g_pipeline.presentFbo);
        }
        g_pipeline.tex[0] = CreatePipelineTexture(gl, dstWidth, dstHeight);
        g_pipeline.tex[1] = CreatePipelineTexture(gl, dstWidth, dstHeight);
        g_pipeline.width = dstWidth;
        g_pipeline.height = dstHeight;
        g_pipeline.valid = true;
    }

    // The native-resolution pair only needs to exist while native and destination actually
    // differ - freed again if a later frame's native/window sizes end up matching (e.g. the game
    // changed its own video mode to match the window), so this feature costs nothing once it
    // isn't in play.
    bool needNativePair = (nativeWidth != dstWidth) || (nativeHeight != dstHeight);
    if (needNativePair) {
        if (!g_pipeline.hasNativePair || g_pipeline.nativeWidth != nativeWidth ||
            g_pipeline.nativeHeight != nativeHeight) {
            if (g_pipeline.hasNativePair) {
                gl.glDeleteTextures(2, g_pipeline.nativeTex);
            }
            g_pipeline.nativeTex[0] = CreatePipelineTexture(gl, nativeWidth, nativeHeight);
            g_pipeline.nativeTex[1] = CreatePipelineTexture(gl, nativeWidth, nativeHeight);
            g_pipeline.nativeWidth = nativeWidth;
            g_pipeline.nativeHeight = nativeHeight;
            g_pipeline.hasNativePair = true;
        }
    } else if (g_pipeline.hasNativePair) {
        gl.glDeleteTextures(2, g_pipeline.nativeTex);
        g_pipeline.nativeTex[0] = g_pipeline.nativeTex[1] = 0;
        g_pipeline.hasNativePair = false;
        g_pipeline.nativeWidth = g_pipeline.nativeHeight = 0;
    }

    // Depth is always native-resolution, and only allocated when a depth-consuming stage is
    // actually listed - everyone else pays nothing for this.
    if (needDepth && (g_pipeline.depthTex == 0 || g_pipeline.depthWidth != nativeWidth ||
                       g_pipeline.depthHeight != nativeHeight)) {
        if (g_pipeline.depthTex != 0) {
            gl.glDeleteTextures(1, &g_pipeline.depthTex);
            gl.glDeleteFramebuffers(1, &g_pipeline.depthFbo);
        }
        g_pipeline.depthTex = CreateDepthTexture(gl, nativeWidth, nativeHeight);
        gl.glGenFramebuffers(1, &g_pipeline.depthFbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.depthFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_pipeline.depthTex, 0);
        g_pipeline.depthWidth = nativeWidth;
        g_pipeline.depthHeight = nativeHeight;
    }
}

// True on the frame the dump hotkey transitions from up to down. Polling edge-triggers here
// rather than firing while held, since a held key would otherwise rewrite the dump every frame
// for as long as it is down.
bool ConsumeFrameDumpRequest(int frameDumpKey) {
    static bool wasDown = false;
    if (frameDumpKey == 0) {
        wasDown = false;
        return false;
    }
    bool isDown = (GetAsyncKeyState(frameDumpKey) & 0x8000) != 0;
    bool pressed = isDown && !wasDown;
    wasDown = isDown;
    return pressed;
}

// Reads the game's finished frame straight off the default framebuffer - color and depth both -
// and writes it out for the config editor. Deliberately reads the DEFAULT framebuffer rather
// than the pipeline's captured textures: this must be the unprocessed frame the game drew, and
// reading depth from framebuffer 0 also means a dump needs no depth texture and therefore works
// regardless of whether any depth-consuming stage is listed.
void DumpFrame(const GlComputeApi& gl, int width, int height, const char* path) {
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);

    size_t texelCount = (size_t)width * (size_t)height;
    unsigned char* color = (unsigned char*)malloc(texelCount * 4);
    float* depth = (float*)malloc(texelCount * sizeof(float));
    if (color == nullptr || depth == nullptr) {
        printf("[opengl32_enh_cpp] frame_dump: out of memory for a %dx%d dump\n", width, height);
        free(color);
        free(depth);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        return;
    }

    gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color);
    gl.glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    unsigned int err = gl.glGetError();

    ProjectionParams projection;
    bool hasProjection = GetCapturedProjection(projection);

    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] frame_dump: glGetError() = 0x%04X reading the frame back, "
               "not writing a dump\n", err);
    } else {
        if (!hasProjection) {
            printf("[opengl32_enh_cpp] frame_dump: no projection captured yet - dumping anyway, "
                   "but ssao cannot be tuned against this frame (see projection_capture.h)\n");
        }
        WriteFrameDump(path, width, height, hasProjection, projection, color, depth);
    }

    free(color);
    free(depth);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
}

}  // namespace

// See post_effects.h. Every stage named here passes g_pipeline.depthTex to its Apply*() in the
// switch below, and every stage that does must be named here.
bool StageNeedsDepth(EffectKind stage) {
    switch (stage) {
        case EffectKind::DepthVignette:
        case EffectKind::Ssao:
        case EffectKind::Dof:
        case EffectKind::Fog:
        case EffectKind::Ssr:
            return true;
        default:
            return false;
    }
}

void ApplySelectedEffect(void* hdc) {
    // Idempotent, and cheap after the first call. Also covers the case where a game somehow
    // reaches a swap without ApplyWindowSizeOverride having run first.
    RedirectStdoutToDebugLog();

    const AnaxConfig& config = GetAnaxConfig();

    // Nothing to do at all - don't even reach GetGlComputeApi(). windowWidth/windowHeight being
    // configured counts as something to do (even with an empty effect= list and no frame dump),
    // since that alone can put the game's native resolution and the real window at odds - see
    // the native/dst split below - and this proxy needs to at least stretch-fill the window for
    // that, without the player having to also list an upscale stage just to avoid a small image
    // in the corner of a bigger window. Reaching the loader on a GPU that can't support the
    // effects is no longer a per-frame log: it resolves at most once per context (see
    // gl_loader.cpp's GetGlComputeApi).
    bool windowSizeOverrideActive = config.windowWidth > 0 && config.windowHeight > 0;
    if (config.stageCount == 0 && config.frameDumpKey == 0 && !windowSizeOverrideActive) {
        return;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        // Every stage below independently no-ops and logs once when GL 4.3 compute support
        // is unavailable - skip the capture/blit machinery here too rather than round-trip
        // the back buffer through a pipeline nothing can actually run.
        return;
    }

    // Drain anything the app left pending before any stage of ours checks glGetError(). The
    // flag is global and carries no notion of who set it, so without this the first stage to
    // look reports the GAME's error as its own - see texture_effect.cpp, where exactly that
    // made a healthy CAS dispatch look like it was failing on every run.
    unsigned int pendingErr = gl.glGetError();
    if (pendingErr != GL_NO_ERROR) {
        static bool warnedPending = false;
        if (!warnedPending) {
            printf("[opengl32_enh_cpp] post_effects: the app had glGetError() = 0x%04X pending "
                   "at swap; draining it so it is not blamed on a stage\n", pendingErr);
            warnedPending = true;
        }
    }

    // The game's own render resolution - whatever it last set via glViewport.
    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    int viewportX = viewport[0];
    int viewportY = viewport[1];
    int nativeWidth = viewport[2];
    int nativeHeight = viewport[3];
    if (nativeWidth <= 0 || nativeHeight <= 0) {
        return;
    }

    // Before the pipeline runs, so the dump is the game's own unprocessed frame - which is what
    // the editor needs in order to apply stages to it itself. Always at the native resolution:
    // a dump is meant to capture exactly what the game drew, independent of any window resize.
    if (ConsumeFrameDumpRequest(config.frameDumpKey)) {
        DumpFrame(gl, nativeWidth, nativeHeight, config.frameDumpPath);
    }

    // The real window's client size. Whenever this is bigger than the game's own native render
    // resolution above, an upscale-capable stage (bilinear/nvscaler/fsr) becomes a REAL upscale -
    // reconstructing a genuinely smaller source up to fill the window - rather than the
    // same-size "preview a lower-res look" trick `scale` alone provides on a capture that was
    // already rendered at full size (see fsr.h's header comment). Falls back to the native size
    // (no mismatch, no behavior change from before this feature existed) if the window's size
    // can't be determined.
    int dstWidth = nativeWidth;
    int dstHeight = nativeHeight;
    GetWindowClientSize(hdc, dstWidth, dstHeight);
    if (dstWidth <= 0 || dstHeight <= 0) {
        dstWidth = nativeWidth;
        dstHeight = nativeHeight;
    }

    // Deliberately strict about what counts as "the game is rendering smaller than its window":
    //
    //   - The viewport must start at the back buffer's origin. A game whose final pass targets a
    //     SUB-viewport (a pillarboxed 4:3 view inside a wider window, a letterboxed cutscene) is
    //     not rendering small-and-then-scaled, it is rendering into part of a full-size buffer -
    //     treating it as an upscale would stretch that region over the whole window. The capture
    //     below reads from (0,0) regardless, so an offset viewport also wouldn't be the region
    //     the upscale then blew up.
    //   - The window must be strictly BIGGER on both axes. A client area smaller than the render
    //     resolution (windowWidth/windowHeight set below the game's own mode, or a frame caught
    //     mid-resize) would otherwise produce a ratio above 1.0, which every upscaler either
    //     clamps away or rejects outright.
    //
    // Anything that fails these falls through to the original same-size path: captured, run
    // through the chain and blitted back exactly where it came from.
    bool hasRealUpscale = viewportX == 0 && viewportY == 0 &&
                           dstWidth > nativeWidth && dstHeight > nativeHeight;

    // The exact ratio that maps the real (physically smaller) native-resolution source across a
    // destination-sized output - see fsr.h/bilinear_upscale.cpp/nis_effect.cpp: their `scale`
    // describes a virtual source grid laid over the full output size, which is precisely what a
    // genuinely smaller physical source texture needs.
    float realUpscaleRatio = hasRealUpscale ? (float)nativeWidth / (float)dstWidth : 1.0f;

    if (hasRealUpscale) {
        // The upscalers apply one scalar ratio to both axes - correct as long as native and
        // window share an aspect ratio, which is what setting windowWidth/windowHeight to a clean
        // multiple of the game's own resolution gives you. A mismatched aspect ratio still runs
        // (using the width ratio), it just comes out stretched vertically - flagged once rather
        // than silently producing a subtly wrong image.
        float heightRatio = (float)nativeHeight / (float)dstHeight;
        static bool warnedAspect = false;
        if (!warnedAspect && fabsf(realUpscaleRatio - heightRatio) > 0.01f) {
            printf("[opengl32_enh_cpp] post_effects: native resolution %dx%d and window %dx%d "
                   "don't share an aspect ratio - an upscale stage will stretch the image\n",
                   nativeWidth, nativeHeight, dstWidth, dstHeight);
            warnedAspect = true;
        }
    }

    if (config.stageCount == 0 && !hasRealUpscale) {
        return;
    }

    int savedActiveTexture = 0;
    gl.glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedTextureBinding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding);
    int savedProgram = 0;
    gl.glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    int savedDrawFbo = 0;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFbo);
    int savedUniformBuffer = 0;
    gl.glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &savedUniformBuffer);

    bool needDepth = false;
    for (int i = 0; i < config.stageCount; ++i) {
        if (StageNeedsDepth(config.stages[i])) {
            needDepth = true;
            break;
        }
    }
    EnsurePipelineTextures(gl, nativeWidth, nativeHeight, dstWidth, dstHeight, needDepth);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, (unsigned int)savedUniformBuffer);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, (unsigned int)savedUniformBuffer);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
    };

    // The pair of ping-pong textures actually in use right now: the native-resolution pair while
    // hasRealUpscale is true (nothing has reconstructed up to the window's size yet), otherwise
    // directly the destination pair - which IS the native-resolution pair, size-for-size, when
    // there's no real upscale in play at all.
    unsigned int* pair = hasRealUpscale ? g_pipeline.nativeTex : g_pipeline.tex;

    // Force the read framebuffer to the default (live back buffer) before capture, same
    // reasoning as every individual effect used to: whatever the host app had bound as its
    // read framebuffer at swap time would otherwise still be bound here. Always captures at the
    // NATIVE resolution - that's the real size of what the game actually drew into the back
    // buffer, regardless of how much bigger the window/back buffer itself is.
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, pair[0]);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, nativeWidth, nativeHeight);

    unsigned int captureErr = gl.glGetError();
    if (captureErr != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] post_effects: glGetError() = 0x%04X after capture, "
               "skipping this frame\n", captureErr);
        restoreState();
        return;
    }

    // Blit the default framebuffer's depth attachment into g_pipeline.depthTex, same
    // read-framebuffer-0 reasoning as the color capture above. Only attempted when a stage that
    // consumes depth is actually listed - everyone else pays nothing for this. A blit error
    // (e.g. this GL context's pixel format has no depth buffer at all) is logged once and leaves
    // depthCaptured false, which makes the depth-consuming stages below no-op for this frame
    // exactly like any other stage that can't run. Always native resolution - see depthTex's
    // comment on PipelineTextures.
    bool depthCaptured = false;
    if (needDepth) {
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_pipeline.depthFbo);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.glBlitFramebuffer(0, 0, nativeWidth, nativeHeight, 0, 0, nativeWidth, nativeHeight,
                              GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        unsigned int depthErr = gl.glGetError();
        depthCaptured = (depthErr == GL_NO_ERROR);
        if (!depthCaptured) {
            static bool warnedNoDepth = false;
            if (!warnedNoDepth) {
                printf("[opengl32_enh_cpp] post_effects: glGetError() = 0x%04X blitting depth, "
                       "depth-based stages will no-op until this changes\n", depthErr);
                warnedNoDepth = true;
            }
        }
    }

    // Run the stages in exactly the order the config listed them - there is no hard-coded
    // pipeline order any more, and no distinction between a "primary" effect and an "addon".
    // `cur` indexes whichever ping-pong texture currently holds the live image (within `pair`);
    // a stage reads it, writes the other one, and only when the stage reports it actually wrote
    // does `cur` flip to follow the image (see e.g. lut_grading.h for why a stage can
    // legitimately no-op). A stage that no-ops therefore drops out of the chain cleanly instead
    // of handing the next stage an untouched buffer.
    //
    // `atNativeRes` tracks which side of a real upscale the pipeline is currently on. It starts
    // true (the just-captured image is native-resolution) and flips, at most once, the moment
    // the FIRST upscale-capable stage (bilinear/nvscaler/fsr) actually runs while hasRealUpscale
    // is set - from then on `pair`/`cur`/curWidth/curHeight all refer to the destination
    // (window-sized) pair instead, exactly like every stage always has when there's no real
    // upscale in play at all.
    int cur = 0;
    int curWidth = nativeWidth;
    int curHeight = nativeHeight;
    bool atNativeRes = true;
    static bool warnedDepthAfterUpscale = false;

    for (int i = 0; i < config.stageCount; ++i) {
        EffectKind stage = config.stages[i];

        // The game's depth buffer only exists at its own native resolution (see depthTex's
        // comment on PipelineTextures) - once the chain has moved on to the larger destination
        // resolution there is no depth data left to sample, so a depth-consuming stage listed
        // after the upscaler can only no-op. Logged once so a misordered effect= list is
        // diagnosable instead of silently doing nothing.
        bool isDepthStage = StageNeedsDepth(stage);
        if (isDepthStage && !atNativeRes) {
            if (!warnedDepthAfterUpscale) {
                printf("[opengl32_enh_cpp] post_effects: '%s' is listed after an upscale stage - "
                       "the game's depth buffer only exists at its native resolution, so this "
                       "stage is being skipped. List ssao/dof/fog/ssr/depthvignette BEFORE "
                       "bilinear/nvscaler/fsr in effect= instead.\n", EffectNameFor(stage));
                warnedDepthAfterUpscale = true;
            }
            continue;
        }

        bool isUpscaleStage = (stage == EffectKind::Bilinear || stage == EffectKind::NVScaler ||
                                stage == EffectKind::Fsr);
        // Only the FIRST upscale-capable stage still at native resolution performs the real
        // resolution change - any later one (e.g. a second upscaler listed by mistake) just runs
        // at destination resolution like any other same-size stage, using config.scale as usual.
        bool doRealUpscale = isUpscaleStage && atNativeRes && hasRealUpscale;

        // NVScaler (NIS) is only defined for a 1x..2x resize: nis_effect.cpp's
        // NVScalerUpdateConfig rejects a ratio outside [0.5, 1] outright, and being a public
        // entry point it has no way to know it is being called once per frame. Skip the stage
        // rather than hand it a ratio it will refuse every frame - the implicit stretch after
        // this loop still fills the window, and fsr/bilinear have no such limit.
        if (doRealUpscale && stage == EffectKind::NVScaler && realUpscaleRatio < 0.5f) {
            static bool warnedNvScalerRange = false;
            if (!warnedNvScalerRange) {
                printf("[opengl32_enh_cpp] post_effects: 'nvscaler' cannot upscale beyond 2x "
                       "(window %dx%d is more than double the game's %dx%d), so it is being "
                       "skipped. Use 'fsr' or 'bilinear' for this ratio.\n",
                       dstWidth, dstHeight, nativeWidth, nativeHeight);
                warnedNvScalerRange = true;
            }
            continue;
        }

        unsigned int src = pair[cur];
        unsigned int dst;
        int dstW, dstH;
        if (doRealUpscale) {
            dst = g_pipeline.tex[0];
            dstW = dstWidth;
            dstH = dstHeight;
        } else {
            dst = pair[1 - cur];
            dstW = curWidth;
            dstH = curHeight;
        }

        // config.scale is deliberately NOT used for a real upscale: it would additionally
        // simulate a lower-res look on top of a resize that is already real.
        float upscaleRatio = doRealUpscale ? realUpscaleRatio : config.scale;

        bool wrote = false;
        switch (stage) {
            case EffectKind::None:
                break;
            case EffectKind::Invert:
                wrote = ApplyInvert(src, dst, dstW, dstH);
                break;
            case EffectKind::Bilinear:
                wrote = ApplyBilinearUpscale(src, dst, dstW, dstH, upscaleRatio);
                break;
            case EffectKind::Cas:
                wrote = ApplyCas(src, dst, dstW, dstH, config.sharpness);
                break;
            case EffectKind::Fsr:
                wrote = ApplyFsr(src, dst, dstW, dstH, upscaleRatio, config.sharpness,
                                 config.fsrDenoise, config.fsrFilmGrain);
                break;
            case EffectKind::NVScaler:
                wrote = ApplyNVScaler(src, dst, dstW, dstH, upscaleRatio, config.sharpness);
                break;
            case EffectKind::AcesToneMap:
                wrote = ApplyHdrLook(src, dst, dstW, dstH, config.acesStrength);
                break;
            case EffectKind::Bloom:
                wrote = ApplyBloom(src, dst, dstW, dstH, config.bloomThreshold, config.bloomIntensity);
                break;
            case EffectKind::Sharpen:
                wrote = ApplyNVSharpen(src, dst, dstW, dstH, config.sharpness);
                break;
            case EffectKind::LutGrading:
                wrote = ApplyLutGrading(src, dst, dstW, dstH, config.lutPath, config.lutStrength);
                break;
            case EffectKind::Vignette:
                wrote = ApplyVignette(src, dst, dstW, dstH, config.vignetteIntensity, config.vignetteRadius);
                break;
            case EffectKind::ChromaticAberration:
                wrote = ApplyChromaticAberration(src, dst, dstW, dstH, config.chromaticAberrationStrength);
                break;
            case EffectKind::Taa:
                wrote = ApplyTaa(src, dst, dstW, dstH, config.taaBlend, config.shimmerSuppression);
                break;
            case EffectKind::Dither:
                wrote = ApplyDither(src, dst, dstW, dstH, config.ditherStrength);
                break;
            case EffectKind::Gamma:
                wrote = ApplyGamma(src, dst, dstW, dstH, config.gamma, config.brightness);
                break;
            case EffectKind::Smaa:
                wrote = ApplySmaa(src, dst, dstW, dstH);
                break;
            case EffectKind::Nr:
                wrote = ApplyNr(src, dst, dstW, dstH, config.nrIntensity, config.nrPasses,
                                 config.nrColorStrength, config.nrTonePreservation,
                                 config.nrGrainPreservation);
                break;
            case EffectKind::LocalContrast:
                wrote = ApplyLocalContrast(src, dst, dstW, dstH, config.localStructureStrength,
                                            config.localToneStrength);
                break;
            case EffectKind::DepthVignette:
                wrote = depthCaptured && ApplyDepthVignette(src, dst, g_pipeline.depthTex, dstW, dstH,
                                                              config.depthVignetteIntensity,
                                                              config.depthVignetteThreshold);
                break;
            case EffectKind::LightShafts:
                // No depth and no projection: the light is found in the colour buffer itself,
                // so this is the one late-era stage with no scene-geometry prerequisites.
                wrote = ApplyLightShafts(src, dst, dstW, dstH, config.shaftsIntensity,
                                          config.shaftsDensity, config.shaftsDecay,
                                          config.shaftsThreshold);
                break;
            case EffectKind::Ssr: {
                // Reconstructs view-space positions and normals from depth, so it needs the
                // projection for the same reason ssao does.
                ProjectionParams ssrProjection;
                // The camera is optional where the projection is not: without a projection raw
                // depth cannot be unprojected at all, but without a camera the gate simply falls
                // back to view-space up - the behaviour this stage shipped with. So this is
                // computed outside the short-circuit and handed over as nullptr when absent.
                CameraMatrix ssrCamera;
                float ssrWorldUp[3] = {0.0f, 0.0f, 0.0f};
                const float* ssrWorldUpPtr = nullptr;
                if (GetCapturedCamera(ssrCamera)) {
                    SsrViewSpaceWorldUp(ssrCamera, (SsrWorldUpAxis)config.ssrWorldUpAxis,
                                        ssrWorldUp);
                    ssrWorldUpPtr = ssrWorldUp;
                }
                wrote = depthCaptured && GetCapturedProjection(ssrProjection) &&
                         ApplySsr(src, dst, g_pipeline.depthTex, dstW, dstH, ssrProjection,
                                  ssrWorldUpPtr,
                                  config.ssrIntensity, config.ssrMaxDistance,
                                  config.ssrThickness, config.ssrUpThreshold);
                break;
            }
            case EffectKind::Fog: {
                // World-unit distances, so it needs the projection for the same reason dof does.
                ProjectionParams fogProjection;
                wrote = depthCaptured && GetCapturedProjection(fogProjection) &&
                         ApplyFog(src, dst, g_pipeline.depthTex, dstW, dstH, fogProjection,
                                  config.fogStart, config.fogEnd, config.fogIntensity,
                                  config.fogColorR, config.fogColorG, config.fogColorB);
                break;
            }
            case EffectKind::Dof: {
                // Needs the projection for the same reason ssao does - dof.h's distances are in
                // world units, and raw depth cannot be turned into those without the frustum.
                ProjectionParams dofProjection;
                wrote = depthCaptured && GetCapturedProjection(dofProjection) &&
                         ApplyDof(src, dst, g_pipeline.depthTex, dstW, dstH, dofProjection,
                                  config.dofFocusDistance, config.dofFocusRange,
                                  config.dofBlurStrength);
                break;
            }
            case EffectKind::Ssao: {
                // Unlike every other stage, this needs the game's projection to unproject depth
                // - see ssao.h. GetCapturedProjection() is false until the game first sets up a
                // 3D view (menu-only frames), which no-ops the stage rather than guessing.
                ProjectionParams projection;
                wrote = depthCaptured && GetCapturedProjection(projection) &&
                         ApplySsao(src, dst, g_pipeline.depthTex, dstW, dstH, projection,
                                   config.ssaoRadius, config.ssaoIntensity, config.ssaoBias);
                break;
            }
        }
        if (wrote) {
            if (doRealUpscale) {
                pair = g_pipeline.tex;
                cur = 0;
                curWidth = dstWidth;
                curHeight = dstHeight;
                atNativeRes = false;
            } else {
                cur = 1 - cur;
            }
        }
    }

    // No listed stage performed the resolution change - effect=none, an all-no-op pipeline, or
    // simply no bilinear/nvscaler/fsr stage in the list. Stretch the native image up to fill the
    // real window anyway: without this, a window forced bigger than the game's own render
    // resolution would show the game's frame in only one corner, the rest left showing whatever
    // was there before.
    if (hasRealUpscale && atNativeRes) {
        float autoRatio = (float)nativeWidth / (float)dstWidth;
        bool wrote = ApplyBilinearUpscale(pair[cur], g_pipeline.tex[0], dstWidth, dstHeight, autoRatio);
        if (wrote) {
            pair = g_pipeline.tex;
            cur = 0;
            curWidth = dstWidth;
            curHeight = dstHeight;
            atNativeRes = false;
        }
    }

    // Draws directly on top of the final texture, unconditionally - not a stage, doesn't
    // participate in the src/dst chain above. Gated on stageCount rather than just reaching this
    // point, since the implicit stretch above can also get here with an empty effect= list (only
    // windowWidth/windowHeight configured) - the badge is specifically about the effect=
    // pipeline having run, not about this proxy having touched the frame at all.
    if (config.fxIndicator && config.stageCount > 0) {
        DrawFxIndicator(pair[cur], curWidth, curHeight);
    }

    // Present: blit whichever texture ended up final back onto the real back buffer, at
    // whichever resolution it ended up at - curWidth/curHeight is dstWidth/dstHeight once a real
    // upscale (explicit or the implicit stretch above) has run, or nativeWidth/nativeHeight
    // (== dstWidth/dstHeight when there's no real upscale in play) if it never did.
    gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.presentFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pair[cur], 0);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, curWidth, curHeight, 0, 0, curWidth, curHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] post_effects: glGetError() = 0x%04X after present\n", err);
    }

    restoreState();
}
