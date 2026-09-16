// Owns the shared post-effect pipeline: two RGBA16F ping-pong textures. Captures the real
// back buffer into them exactly once per frame, chains every enabled stage by alternating
// which texture is "source" and which is "destination" (each stage's ApplyX reads one and
// writes the other, returning whether it actually wrote - see e.g. lut_grading.h's header
// comment for why a stage can legitimately no-op), and blits the final result back to the
// real back buffer exactly once. This replaces the old design where every effect captured
// and blitted independently (round-tripping through its own 8-bit RGBA8 texture even though
// five effects chain together in one frame) - working in float and capturing/blitting once
// meaningfully reduces the banding/precision loss that repeated 8-bit round-trips caused.
//
// Also owns the single GL state save/restore around the whole chain: since nothing outside
// this pipeline runs between the capture and the final blit, one outer save/restore here
// covers every stage - individual ApplyX functions don't need (and don't do) their own.
#include <cstdio>

#include "post_effects.h"
#include "config.h"
#include "pixel_invert.h"
#include "bilinear_upscale.h"
#include "nis_effect.h"
#include "hdr_look.h"
#include "bloom.h"
#include "lut_grading.h"
#include "taa.h"
#include "dither.h"
#include "gl_loader.h"

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
const unsigned int GL_NO_ERROR                 = 0;

struct PipelineTextures {
    bool valid = false;
    int width = 0;
    int height = 0;
    unsigned int tex[2] = {0, 0};
    unsigned int presentFbo = 0;   // reused, re-attached to whichever tex[] is final each frame
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

void EnsurePipelineTextures(const GlComputeApi& gl, int width, int height) {
    if (g_pipeline.valid && g_pipeline.width == width && g_pipeline.height == height) {
        return;
    }

    if (g_pipeline.valid) {
        gl.glDeleteTextures(2, g_pipeline.tex);
        g_pipeline.valid = false;
    }
    if (g_pipeline.presentFbo == 0) {
        gl.glGenFramebuffers(1, &g_pipeline.presentFbo);
    }

    g_pipeline.tex[0] = CreatePipelineTexture(gl, width, height);
    g_pipeline.tex[1] = CreatePipelineTexture(gl, width, height);
    g_pipeline.width = width;
    g_pipeline.height = height;
    g_pipeline.valid = true;
}

}  // namespace

void ApplySelectedEffect() {
    const AnaxConfig& config = GetAnaxConfig();

    bool anyStageEnabled = config.effect != EffectKind::None || config.enableAcesToneMap ||
        config.enableBloom || config.enableSharpen || config.enableLutGrading ||
        config.enableTaa || config.enableDither;
    if (!anyStageEnabled) {
        return;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        // Every stage below independently no-ops and logs once when GL 4.3 compute support
        // is unavailable - skip the capture/blit machinery here too rather than round-trip
        // the back buffer through a pipeline nothing can actually run.
        return;
    }

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    if (width <= 0 || height <= 0) {
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

    EnsurePipelineTextures(gl, width, height);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, (unsigned int)savedUniformBuffer);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, (unsigned int)savedUniformBuffer);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
    };

    // Force the read framebuffer to the default (live back buffer) before capture, same
    // reasoning as every individual effect used to: whatever the host app had bound as its
    // read framebuffer at swap time would otherwise still be bound here.
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, g_pipeline.tex[0]);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    unsigned int captureErr = gl.glGetError();
    if (captureErr != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] post_effects: glGetError() = 0x%04X after capture, "
               "skipping this frame\n", captureErr);
        restoreState();
        return;
    }

    int cur = 0;

    // Primary effect: the base upscale/misc transform, runs first against the raw capture.
    switch (config.effect) {
        case EffectKind::None:
            break;
        case EffectKind::Invert:
            if (ApplyInvert(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height)) cur = 1 - cur;
            break;
        case EffectKind::Bilinear:
            if (ApplyBilinearUpscale(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height)) cur = 1 - cur;
            break;
        case EffectKind::NVScaler:
            if (ApplyNVScaler(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.sharpness)) cur = 1 - cur;
            break;
    }

    // Everything below is an optional addon pass, each independent of the others, always
    // applied in this fixed order: ACES tone map -> Bloom -> Sharpen -> LUT grading -> TAA ->
    // Dither. Dither runs last (right before the final blit) so it dithers whatever the rest
    // of the pipeline produced.
    if (config.enableAcesToneMap) {
        if (ApplyHdrLook(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.acesStrength)) cur = 1 - cur;
    }
    if (config.enableBloom) {
        if (ApplyBloom(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.bloomThreshold, config.bloomIntensity)) cur = 1 - cur;
    }
    if (config.enableSharpen) {
        if (ApplyNVSharpen(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.sharpness)) cur = 1 - cur;
    }
    if (config.enableLutGrading) {
        if (ApplyLutGrading(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.lutPath, config.lutStrength)) cur = 1 - cur;
    }
    if (config.enableTaa) {
        if (ApplyTaa(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.taaBlend)) cur = 1 - cur;
    }
    if (config.enableDither) {
        if (ApplyDither(g_pipeline.tex[cur], g_pipeline.tex[1 - cur], width, height, config.ditherStrength)) cur = 1 - cur;
    }

    // Present: blit whichever texture ended up final back onto the real back buffer.
    gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.presentFbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_pipeline.tex[cur], 0);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] post_effects: glGetError() = 0x%04X after present\n", err);
    }

    restoreState();
}
