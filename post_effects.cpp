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
#include <cstdio>

#include "post_effects.h"
#include "config.h"
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
#include "smaa.h"
#include "fsr.h"
#include "cas.h"
#include "nr.h"
#include "local_contrast.h"
#include "fx_indicator.h"
#include "depth_vignette.h"
#include "ssao.h"
#include "projection_capture.h"
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
const unsigned int GL_NO_ERROR                 = 0;

struct PipelineTextures {
    bool valid = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    int width = 0;
    int height = 0;
    unsigned int tex[2] = {0, 0};
    unsigned int presentFbo = 0;   // reused, re-attached to whichever tex[] is final each frame
    // EXPERIMENTAL (see depth_vignette.h): the default framebuffer's depth attachment, blitted
    // here once per frame, but only when depthvignette is actually listed - see
    // ApplySelectedEffect(). depthFbo exists purely as a blit target for depthTex; nothing ever
    // reads from it as a framebuffer otherwise.
    unsigned int depthTex = 0;
    unsigned int depthFbo = 0;
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

void EnsurePipelineTextures(const GlComputeApi& gl, int width, int height, bool needDepth) {
    // A context change invalidates the cached textures and FBO. Drop the handles rather than
    // deleting them (the owning context freed them already, and glDelete* now would hit
    // unrelated objects in the current context); everything below then recreates them.
    if (g_pipeline.generation != GetGlContextGeneration()) {
        g_pipeline = PipelineTextures{};
        g_pipeline.generation = GetGlContextGeneration();
    }

    if (g_pipeline.valid && g_pipeline.width == width && g_pipeline.height == height) {
        if (needDepth && g_pipeline.depthTex == 0) {
            g_pipeline.depthTex = CreateDepthTexture(gl, width, height);
            gl.glGenFramebuffers(1, &g_pipeline.depthFbo);
            gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.depthFbo);
            gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_pipeline.depthTex, 0);
        }
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

    // A resize invalidates the old depth texture/FBO too (they were sized to the old
    // width/height) - drop the stale handles so the needDepth branch just below rebuilds them
    // against the new size.
    g_pipeline.depthTex = 0;
    g_pipeline.depthFbo = 0;
    if (needDepth) {
        g_pipeline.depthTex = CreateDepthTexture(gl, width, height);
        gl.glGenFramebuffers(1, &g_pipeline.depthFbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, g_pipeline.depthFbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_pipeline.depthTex, 0);
    }
}

}  // namespace

void ApplySelectedEffect() {
    const AnaxConfig& config = GetAnaxConfig();

    if (config.stageCount == 0) {
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

    bool needDepth = HasEffectStage(config, EffectKind::DepthVignette) ||
                      HasEffectStage(config, EffectKind::Ssao);
    EnsurePipelineTextures(gl, width, height, needDepth);

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

    // Blit the default framebuffer's depth attachment into g_pipeline.depthTex, same
    // read-framebuffer-0 reasoning as the color capture above. Only attempted when a stage that
    // consumes depth is actually listed - everyone else pays nothing for this. A blit error
    // (e.g. this GL context's pixel format has no depth buffer at all) is logged once and leaves
    // depthCaptured false, which makes the depth-consuming stages below no-op for this frame
    // exactly like any other stage that can't run.
    bool depthCaptured = false;
    if (needDepth) {
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_pipeline.depthFbo);
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
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
    // `cur` indexes whichever ping-pong texture currently holds the live image; a stage reads
    // it, writes the other one, and only when the stage reports it actually wrote does `cur`
    // flip to follow the image (see e.g. lut_grading.h for why a stage can legitimately
    // no-op). A stage that no-ops therefore drops out of the chain cleanly instead of
    // handing the next stage an untouched buffer.
    int cur = 0;
    for (int i = 0; i < config.stageCount; ++i) {
        unsigned int src = g_pipeline.tex[cur];
        unsigned int dst = g_pipeline.tex[1 - cur];
        bool wrote = false;
        switch (config.stages[i]) {
            case EffectKind::None:
                break;
            case EffectKind::Invert:
                wrote = ApplyInvert(src, dst, width, height);
                break;
            case EffectKind::Bilinear:
                wrote = ApplyBilinearUpscale(src, dst, width, height, config.scale);
                break;
            case EffectKind::Cas:
                wrote = ApplyCas(src, dst, width, height, config.sharpness);
                break;
            case EffectKind::Fsr:
                wrote = ApplyFsr(src, dst, width, height, config.scale, config.sharpness,
                                 config.fsrDenoise, config.fsrFilmGrain);
                break;
            case EffectKind::NVScaler:
                wrote = ApplyNVScaler(src, dst, width, height, config.scale, config.sharpness);
                break;
            case EffectKind::AcesToneMap:
                wrote = ApplyHdrLook(src, dst, width, height, config.acesStrength);
                break;
            case EffectKind::Bloom:
                wrote = ApplyBloom(src, dst, width, height, config.bloomThreshold, config.bloomIntensity);
                break;
            case EffectKind::Sharpen:
                wrote = ApplyNVSharpen(src, dst, width, height, config.sharpness);
                break;
            case EffectKind::LutGrading:
                wrote = ApplyLutGrading(src, dst, width, height, config.lutPath, config.lutStrength);
                break;
            case EffectKind::Vignette:
                wrote = ApplyVignette(src, dst, width, height, config.vignetteIntensity, config.vignetteRadius);
                break;
            case EffectKind::ChromaticAberration:
                wrote = ApplyChromaticAberration(src, dst, width, height, config.chromaticAberrationStrength);
                break;
            case EffectKind::Taa:
                wrote = ApplyTaa(src, dst, width, height, config.taaBlend, config.shimmerSuppression);
                break;
            case EffectKind::Dither:
                wrote = ApplyDither(src, dst, width, height, config.ditherStrength);
                break;
            case EffectKind::Smaa:
                wrote = ApplySmaa(src, dst, width, height);
                break;
            case EffectKind::Nr:
                wrote = ApplyNr(src, dst, width, height, config.nrIntensity, config.nrPasses,
                                 config.nrColorStrength, config.nrTonePreservation,
                                 config.nrGrainPreservation);
                break;
            case EffectKind::LocalContrast:
                wrote = ApplyLocalContrast(src, dst, width, height, config.localStructureStrength,
                                            config.localToneStrength);
                break;
            case EffectKind::DepthVignette:
                wrote = depthCaptured && ApplyDepthVignette(src, dst, g_pipeline.depthTex, width, height,
                                                              config.depthVignetteIntensity,
                                                              config.depthVignetteThreshold);
                break;
            case EffectKind::Ssao: {
                // Unlike every other stage, this needs the game's projection to unproject depth
                // - see ssao.h. GetCapturedProjection() is false until the game first sets up a
                // 3D view (menu-only frames), which no-ops the stage rather than guessing.
                ProjectionParams projection;
                wrote = depthCaptured && GetCapturedProjection(projection) &&
                         ApplySsao(src, dst, g_pipeline.depthTex, width, height, projection,
                                   config.ssaoRadius, config.ssaoIntensity, config.ssaoBias);
                break;
            }
        }
        if (wrote) {
            cur = 1 - cur;
        }
    }

    // Draws directly on top of the final texture, unconditionally - not a stage, doesn't
    // participate in the src/dst chain above. Reaching this point already means
    // config.stageCount > 0 and the frame was captured/processed, which is exactly what the
    // badge is meant to confirm happened.
    if (config.fxIndicator) {
        DrawFxIndicator(g_pipeline.tex[cur], width, height);
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
