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

// The reference ArmSupersampleForFrame validated this frame's latch against, snapshotted at the
// moment of that decision. ScaleGameRect divides by this and never by g_refWidth/g_refHeight:
// the live reference is rewritten by the frame's first viewport, which happens AFTER the latch
// is taken, so on a mode-change frame the two disagree. See render_target.h's amendment.
int g_armedRefWidth = 0;
int g_armedRefHeight = 0;

// Defined with the rest of the GL-touching state further down; declared here because
// NotifyGameViewport revokes the latch and the binding has to go with it, and because
// ResetRenderTargetState has to drop the record of it.
void ReleaseRenderTargetBinding();
void ForgetRenderTargetBinding();

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

    // Revocation - see render_target.h's amendment to the invariant. This frame's latch was
    // decided before this viewport existed, against the PREVIOUS frame's reference. If the game
    // has changed mode since, that reference no longer describes what it is drawing, and the
    // choice is between scaling by a reference we never validated (which for a mode change to a
    // size above the configured target renders BELOW native - the one thing this feature
    // refuses) and not scaling at all. We do not scale at all: disarm, and put the game back on
    // framebuffer 0 for the rest of this frame, because the viewport and the binding have to
    // move together or the player sees a corner of the frame blown up.
    if (g_armed && (width != g_armedRefWidth || height != g_armedRefHeight)) {
        g_armed = false;
        g_armedRefWidth = 0;
        g_armedRefHeight = 0;
        ReleaseRenderTargetBinding();
    }

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
    // The reference that decision was made from, frozen for the rest of the frame. Zeroed when
    // not armed so a stale snapshot can never be divided by.
    g_armedRefWidth = g_armed ? g_refWidth : 0;
    g_armedRefHeight = g_armed ? g_refHeight : 0;
}

bool IsSupersampleActive() {
    return g_armed;
}

void ScaleGameRect(int& x, int& y, int& width, int& height) {
    if (!g_armed) {
        return;
    }
    const AnaxConfig& config = GetAnaxConfig();
    // g_armedRef*, not g_ref*: the denominator has to be the reference this frame's arming
    // decision was validated against, not whatever the live reference has since become.
    int left = ScaleEdge(x, config.renderWidth, g_armedRefWidth);
    int right = ScaleEdge(x + width, config.renderWidth, g_armedRefWidth);
    int bottom = ScaleEdge(y, config.renderHeight, g_armedRefHeight);
    int top = ScaleEdge(y + height, config.renderHeight, g_armedRefHeight);
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
    g_armedRefWidth = 0;
    g_armedRefHeight = 0;
    // Forgotten, not acted on - see render_target.h. This is a test hook and must not issue GL
    // calls; a test that cares about the binding sets it explicitly after calling this.
    ForgetRenderTargetBinding();
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
const unsigned int GL_TEXTURE_BINDING_2D    = 0x8069;

unsigned int g_fbo = 0;
unsigned int g_colorTex = 0;
unsigned int g_depthTex = 0;
int g_targetWidth = 0;
int g_targetHeight = 0;
bool g_targetFloat = false;
unsigned int g_generation = 0;

// Remembers the configuration that failed, so a persistent failure costs one attempt rather
// than one per frame. Cleared by a context change (the generation reset above) or by the
// config naming a different size - either of which could legitimately succeed where this
// one did not.
int g_failedWidth = 0;
int g_failedHeight = 0;
bool g_failedFloat = false;
bool g_haveFailed = false;

// Whether THIS module currently has the offscreen framebuffer bound. The whole point is that
// the off path stays a true no-op: a proxy that has never armed must not issue a
// glBindFramebuffer at all, because the binding it would overwrite is the game's own.
bool g_targetBound = false;

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

// The armed -> unarmed transition, in one place, because two callers need it: BindRenderTarget
// on a frame that is no longer armed, and NotifyGameViewport when it revokes the latch
// mid-frame. Exactly one glBindFramebuffer(0) per transition, and none at all if we never bound
// anything.
void ReleaseRenderTargetBinding() {
    if (!g_targetBound) {
        return;
    }
    // The flag is cleared BEFORE the call can be skipped, not after it succeeds: if we cannot
    // issue the call it is because the context that owned this binding is gone, and a binding in
    // a dead context is not a thing there is anything left to release.
    g_targetBound = false;
    // Every other step of the swap sequence sits behind a gl.loaded check somewhere upstream;
    // this one does not, because it runs on the path where the feature is switched OFF.
    // GetGlComputeApi() zeroes itself on a context change, and a context whose entry points
    // never resolved at all is routine - no context current, or the throwaway software-GL
    // context games create to query the WGL extension string. Calling through a null pointer
    // there would turn a handled degradation into an access violation inside the game's swap,
    // which is the worst direction a proxy DLL can fail in.
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded || gl.glBindFramebuffer == nullptr) {
        return;
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Drops the record without touching GL - only ResetRenderTargetState, the test hook, wants this.
void ForgetRenderTargetBinding() {
    g_targetBound = false;
}

}  // namespace

bool EnsureRenderTarget() {
    const AnaxConfig& config = GetAnaxConfig();
    if (config.renderWidth <= 0 || config.renderHeight <= 0) {
        return false;
    }
    const GlComputeApi& gl = GetGlComputeApi();
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
        g_haveFailed = false;
    }

    if (g_fbo != 0 && g_targetWidth == config.renderWidth &&
        g_targetHeight == config.renderHeight && g_targetFloat == config.renderFloatBuffer) {
        return true;
    }

    if (g_haveFailed && g_failedWidth == config.renderWidth &&
        g_failedHeight == config.renderHeight && g_failedFloat == config.renderFloatBuffer) {
        return false;
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

    // Saved and restored because this runs inside the game's frame, from the swap hook, after
    // post_effects.cpp has already restored its own state - so anything left bound here is what
    // the game's next frame starts with. Only the 2D binding: this function never calls
    // glActiveTexture, so the active unit is not ours to disturb and saving it would be copying
    // post_effects.cpp's list rather than reasoning about what this code actually touches.
    int savedTextureBinding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding);

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
        g_failedWidth = config.renderWidth;
        g_failedHeight = config.renderHeight;
        g_failedFloat = config.renderFloatBuffer;
        g_haveFailed = true;
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
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
    gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
    return true;
}

void BindRenderTarget() {
    if (!IsSupersampleActive() || g_fbo == 0) {
        // Not armed. If we bound the target on an earlier frame it is STILL bound - nothing
        // else takes it down, and post_effects.cpp's state restore puts back whatever it found
        // at entry, which is our framebuffer - so the game would keep rendering into a buffer
        // nothing presents while every capture site read framebuffer 0.
        ReleaseRenderTargetBinding();
        return;
    }
    g_targetBound = true;
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
