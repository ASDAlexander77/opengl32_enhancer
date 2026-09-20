// See render_target.h.
#include "render_target.h"

#include <cmath>
#include <cstdio>

#include "config.h"
#include "gl_loader.h"

namespace {

// The game's own full-frame viewport - the denominator of the scale factor, and the size the
// finished frame is downsampled back to on present.
int g_refWidth = 0;
int g_refHeight = 0;
bool g_haveRef = false;

// Whether the next NotifyGameViewport is the frame's first, and therefore the reference.
bool g_expectFirstViewport = false;

// The per-frame latch. Nothing reads config to decide whether to scale - everything reads this,
// so the viewport hook and the capture path cannot disagree within a frame.
bool g_armed = false;

// One edge, scaled. Both edges of a rectangle go through this, and the size is their difference,
// so adjacent rectangles abut exactly however fractional the factor is.
int ScaleEdge(int edge, int numerator, int denominator) {
    // Unreachable today: ScaleGameRect only calls this while armed, and arming implies a
    // positive reference. Kept anyway, unlike the equally unreachable guard removed from
    // ArmSupersampleForFrame, because that one decided a boolean and this one stands in front
    // of a division - the cost of being wrong here is a NaN propagating through lround into a
    // viewport rectangle, which is not a failure mode worth trading for one fewer branch.
    if (denominator <= 0) {
        return edge;
    }
    double scaled = ((double)edge * (double)numerator) / (double)denominator;
    return (int)lround(scaled);
}

}  // namespace

void NotifyFrameBoundary() {
    g_expectFirstViewport = true;
}

void NotifyGameViewport(int x, int y, int width, int height) {
    (void)x;
    (void)y;
    if (!g_expectFirstViewport) {
        return;
    }
    g_expectFirstViewport = false;
    if (width <= 0 || height <= 0) {
        return;
    }
    g_refWidth = width;
    g_refHeight = height;
    g_haveRef = true;
}

void ArmSupersampleForFrame(bool targetReady) {
    const AnaxConfig& config = GetAnaxConfig();
    // No `renderWidth > 0` check: g_haveRef implies the reference is positive, so the >=
    // comparisons below already imply it. A mutation test proved the explicit check
    // unreachable, which is the definition of code that is not doing anything.
    g_armed = targetReady &&
              g_haveRef &&
              config.renderWidth >= g_refWidth && config.renderHeight >= g_refHeight;
}

bool IsSupersampleActive() {
    return g_armed;
}

void ScaleGameRect(int& x, int& y, int& width, int& height) {
    if (!g_armed) {
        return;
    }
    const AnaxConfig& config = GetAnaxConfig();
    int left = ScaleEdge(x, config.renderWidth, g_refWidth);
    int right = ScaleEdge(x + width, config.renderWidth, g_refWidth);
    int bottom = ScaleEdge(y, config.renderHeight, g_refHeight);
    int top = ScaleEdge(y + height, config.renderHeight, g_refHeight);
    x = left;
    width = right - left;
    y = bottom;
    height = top - bottom;
}

void ResetRenderTargetState() {
    g_refWidth = 0;
    g_refHeight = 0;
    g_haveRef = false;
    g_expectFirstViewport = false;
    g_armed = false;
}

namespace {

const unsigned int GL_TEXTURE_2D            = 0x0DE1;
const unsigned int GL_FRAMEBUFFER           = 0x8D40;
const unsigned int GL_COLOR_ATTACHMENT0     = 0x8CE0;
const unsigned int GL_DEPTH_ATTACHMENT      = 0x8D00;
const unsigned int GL_FRAMEBUFFER_COMPLETE  = 0x8CD5;
const unsigned int GL_RGBA8                 = 0x8058;
const unsigned int GL_RGBA16F               = 0x881A;
const unsigned int GL_DEPTH_COMPONENT24     = 0x81A6;
const unsigned int GL_BACK                  = 0x0405;
const unsigned int GL_MAX_TEXTURE_SIZE      = 0x0D33;
const unsigned int GL_TEXTURE_MIN_FILTER    = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER    = 0x2800;
const unsigned int GL_LINEAR                = 0x2601;

unsigned int g_fbo = 0;
unsigned int g_colorTex = 0;
unsigned int g_depthTex = 0;
int g_targetWidth = 0;
int g_targetHeight = 0;
bool g_targetFloat = false;
unsigned int g_generation = 0;

void DestroyRenderTarget(const GlComputeApi& gl) {
    if (g_fbo != 0) {
        gl.glDeleteFramebuffers(1, &g_fbo);
    }
    if (g_colorTex != 0) {
        gl.glDeleteTextures(1, &g_colorTex);
    }
    if (g_depthTex != 0) {
        gl.glDeleteTextures(1, &g_depthTex);
    }
    g_fbo = g_colorTex = g_depthTex = 0;
    g_targetWidth = g_targetHeight = 0;
}

}  // namespace

bool EnsureRenderTarget() {
    const AnaxConfig& config = GetAnaxConfig();
    const GlComputeApi& gl = GetGlComputeApi();
    if (config.renderWidth <= 0 || config.renderHeight <= 0) {
        return false;
    }
    if (!gl.loaded || gl.glCheckFramebufferStatus == nullptr) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] render_target: GL 4.3 unavailable, supersampling off\n");
            warned = true;
        }
        return false;
    }

    // A dead context's texture names mean nothing - same reasoning as every other cached
    // GL-derived value in this DLL, see gl_loader.h's GetGlContextGeneration().
    if (g_generation != GetGlContextGeneration()) {
        g_generation = GetGlContextGeneration();
        g_fbo = g_colorTex = g_depthTex = 0;
        g_targetWidth = g_targetHeight = 0;
    }

    if (g_fbo != 0 && g_targetWidth == config.renderWidth &&
        g_targetHeight == config.renderHeight && g_targetFloat == config.renderFloatBuffer) {
        return true;
    }

    // Two guards, deliberately, for two different failures. This one refuses a size the driver
    // will not allocate at all - checking first means we never ATTEMPT a doomed allocation, and
    // the log names the real reason. The glCheckFramebufferStatus check below is the correctness
    // backstop for anything that allocates but does not assemble into a complete framebuffer.
    // A mutation test kills neither in isolation, because every size that allocates also
    // completes; they are lethal together, which is the honest description.
    int maxTextureSize = 0;
    gl.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize > 0 &&
        (config.renderWidth > maxTextureSize || config.renderHeight > maxTextureSize)) {
        static bool warnedSize = false;
        if (!warnedSize) {
            printf("[opengl32_enh_cpp] render_target: %dx%d exceeds this driver's maximum "
                   "texture size of %d, supersampling off\n",
                   config.renderWidth, config.renderHeight, maxTextureSize);
            warnedSize = true;
        }
        return false;
    }

    DestroyRenderTarget(gl);

    gl.glGenTextures(1, &g_colorTex);
    gl.glBindTexture(GL_TEXTURE_2D, g_colorTex);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, config.renderFloatBuffer ? GL_RGBA16F : GL_RGBA8,
                      config.renderWidth, config.renderHeight);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);

    gl.glGenTextures(1, &g_depthTex);
    gl.glBindTexture(GL_TEXTURE_2D, g_depthTex);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24,
                      config.renderWidth, config.renderHeight);

    gl.glGenFramebuffers(1, &g_fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_colorTex, 0);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_depthTex, 0);

    unsigned int status = gl.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("[opengl32_enh_cpp] render_target: framebuffer incomplete (0x%04X) at %dx%d, "
               "supersampling off\n", status, config.renderWidth, config.renderHeight);
        DestroyRenderTarget(gl);
        return false;
    }

    g_targetWidth = config.renderWidth;
    g_targetHeight = config.renderHeight;
    g_targetFloat = config.renderFloatBuffer;

    // Runs, but says so: a render target whose aspect ratio differs from the game's own means
    // the present blit stretches the image on one axis. Warned rather than refused, and once
    // rather than per frame - the same treatment post_effects.cpp gives the upscale case.
    int refWidth = 0, refHeight = 0;
    GetReferenceViewport(refWidth, refHeight);
    if (refWidth > 0 && refHeight > 0) {
        double targetAspect = (double)g_targetWidth / (double)g_targetHeight;
        double gameAspect = (double)refWidth / (double)refHeight;
        static bool warnedAspect = false;
        if (!warnedAspect && (targetAspect - gameAspect > 0.01 || gameAspect - targetAspect > 0.01)) {
            printf("[opengl32_enh_cpp] render_target: %dx%d and the game's own %dx%d don't share "
                   "an aspect ratio - the presented image will be stretched\n",
                   g_targetWidth, g_targetHeight, refWidth, refHeight);
            warnedAspect = true;
        }
    }
    printf("[opengl32_enh_cpp] render_target: supersampling into %dx%d (%s)\n",
           g_targetWidth, g_targetHeight, g_targetFloat ? "RGBA16F" : "RGBA8");
    return true;
}

void BindRenderTarget() {
    if (!IsSupersampleActive() || g_fbo == 0) {
        return;
    }
    GetGlComputeApi().glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
}

unsigned int GetGameFramebuffer() {
    return IsSupersampleActive() ? g_fbo : 0;
}

unsigned int GetGameReadBuffer() {
    return IsSupersampleActive() ? GL_COLOR_ATTACHMENT0 : GL_BACK;
}

void GetRenderTargetSize(int& width, int& height) {
    if (g_targetWidth <= 0 || g_targetHeight <= 0) {
        return;
    }
    width = g_targetWidth;
    height = g_targetHeight;
}

void GetReferenceViewport(int& width, int& height) {
    if (!g_haveRef) {
        return;
    }
    width = g_refWidth;
    height = g_refHeight;
}
