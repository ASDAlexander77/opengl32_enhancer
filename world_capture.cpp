// See world_capture.h.
#include <cstdio>

#include "world_capture.h"
#include "config.h"
#include "gl_loader.h"
#include "render_target.h"

namespace {

const unsigned int GL_TEXTURE_2D              = 0x0DE1;
const unsigned int GL_TEXTURE0                = 0x84C0;
const unsigned int GL_ACTIVE_TEXTURE          = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D      = 0x8069;
const unsigned int GL_TEXTURE_MIN_FILTER      = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER      = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S          = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T          = 0x2803;
const unsigned int GL_LINEAR                  = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE           = 0x812F;
const unsigned int GL_RGBA16F                 = 0x881A;
const unsigned int GL_BACK                    = 0x0405;
const unsigned int GL_READ_BUFFER             = 0x0C02;
const unsigned int GL_READ_FRAMEBUFFER        = 0x8CA8;
const unsigned int GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const unsigned int GL_VIEWPORT                = 0x0BA2;
const unsigned int GL_NO_ERROR                = 0;

bool g_armed = false;
bool g_latched = false;
unsigned int g_texture = 0;
int g_width = 0;
int g_height = 0;
unsigned int g_generation = 0;
unsigned int g_frameCounter = 0;

// RGBA16F on purpose, matching the pipeline's own textures: the HUD mask compares this against
// post_effects.cpp's pristine capture, and if the two were stored at different precisions an
// identical pixel could differ by a quantisation step and read as HUD.
void EnsureTexture(const GlComputeApi& gl, int width, int height) {
    if (g_texture != 0 && g_width == width && g_height == height) {
        return;
    }
    if (g_texture != 0) {
        gl.glDeleteTextures(1, &g_texture);
        g_texture = 0;
    }
    gl.glGenTextures(1, &g_texture);
    gl.glBindTexture(GL_TEXTURE_2D, g_texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    g_width = width;
    g_height = height;
}

void LatchWorldFrame() {
    if (!g_armed || g_latched) {
        return;
    }

    // Everything below this point costs a handful of glGetIntegerv calls, a full-resolution
    // glCopyTexSubImage2D of the back buffer with 8-bit->16F conversion, a glGetError() (a
    // driver sync point) and a permanently-allocated full-res RGBA16F texture - all of it wasted
    // if no stage that needs it (see StageNeedsWorldCapture in config.h) is even listed.
    // GetAnaxConfig() parses the ini once and caches the result for the process lifetime (see
    // config.h), so this gate is a cached struct read plus a short loop over at most a couple of
    // dozen stages - not a per-frame re-parse, and not something that tracks an ini edit made
    // while the DLL is already loaded.
    if (!AnyStageNeedsWorldCapture(GetAnaxConfig())) {
        return;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        // Unlike the camera capture, this exists solely to feed a compute stage. If compute is
        // unavailable the consumer cannot run either, so there is nothing to capture for.
        return;
    }

    if (g_generation != GetGlContextGeneration()) {
        // The old texture belonged to a context that is gone; drop the handle rather than
        // deleting it, which would now hit an unrelated object.
        g_texture = 0;
        g_width = 0;
        g_height = 0;
        g_generation = GetGlContextGeneration();
    }

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) {
        return;
    }

    // Running inside the game's own GL state, so everything touched is put back.
    int savedActiveTexture = 0;
    gl.glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedBinding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedBinding);
    int savedReadBuffer = 0;
    gl.glGetIntegerv(GL_READ_BUFFER, &savedReadBuffer);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);

    // Drain anything the app left pending before ANY of this module's own GL calls, including
    // EnsureTexture()'s glTexStorage2D on a resize - not just the copy below. glGetError()
    // reports the first error since it was last called, with no notion of who caused it: placed
    // after EnsureTexture(), a storage allocation failure of OUR OWN would itself be drained and
    // logged as "the app had glGetError() pending", blaming the game for our own fault. Draining
    // here, before either call, keeps that from happening while still fixing the original
    // problem: an error the GAME left pending is misread as this module's own failure, and worse,
    // reading it CLEARS the flag - so a Quake II-family engine's own GL_CheckErrors() call after
    // R_SetGL2D would silently lose an error it would otherwise have reported. Logging once (same
    // pattern as post_effects.cpp's pending-error drain around its own capture) fixes both: the
    // real result of each of this module's OWN calls is read afterward, and the game keeps seeing
    // its own errors. See world_capture.h.
    unsigned int pendingErr = gl.glGetError();
    if (pendingErr != GL_NO_ERROR) {
        static bool warnedPending = false;
        if (!warnedPending) {
            printf("[opengl32_enh_cpp] world_capture: the app had glGetError() = 0x%04X pending "
                   "before this capture; draining it so it is not blamed on us\n",
                   pendingErr);
            warnedPending = true;
        }
    }

    EnsureTexture(gl, viewport[2], viewport[3]);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, GetGameFramebuffer());
    gl.glReadBuffer(GetGameReadBuffer());
    gl.glBindTexture(GL_TEXTURE_2D, g_texture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_width, g_height);

    unsigned int err = gl.glGetError();

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
    gl.glReadBuffer((unsigned int)savedReadBuffer);
    gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedBinding);
    gl.glActiveTexture((unsigned int)savedActiveTexture);

    if (err != GL_NO_ERROR) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] world_capture: glGetError() = 0x%04X after copy, "
                   "world-only frame unavailable\n", err);
            warned = true;
        }
        return;
    }

    g_latched = true;
}

// Reuses cameraLogInterval rather than adding a knob: the thing being validated is the same
// class of claim the camera latch was - a guess about WHEN - so it is validated the same way
// and in the same log.
void LogWorldCaptureIfDue() {
    int interval = GetAnaxConfig().cameraLogInterval;
    if (interval <= 0 || (g_frameCounter % (unsigned int)interval) != 0) {
        return;
    }
    printf("[opengl32_enh_cpp] world_capture: frame %u latched=%s size=%dx%d\n",
           g_frameCounter, g_latched ? "yes" : "no", g_width, g_height);
}

}  // namespace

void NotifyWorldPassBegan() {
    g_armed = true;
}

void NotifyTwoDPassBegan() {
    LatchWorldFrame();
    g_armed = false;
}

void InvalidateWorldFrame() {
    LogWorldCaptureIfDue();
    ++g_frameCounter;
    g_armed = false;
    g_latched = false;
}

bool GetWorldOnlyFrame(unsigned int& texture, int& width, int& height) {
    if (!g_latched || g_texture == 0 || g_generation != GetGlContextGeneration()) {
        return false;
    }
    texture = g_texture;
    width = g_width;
    height = g_height;
    return true;
}
