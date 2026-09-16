// See texture_sharpen.h. Reuses the existing NVSharpen compute-shader effect (see
// nis_effect.h) at texture-upload time: upload the game's own pixels into a scratch RGBA8
// texture, run ApplyNVSharpen into an RGBA16F texture of the SAME dimensions, read the result
// back to RGBA8, and hand THAT to the real glTexImage2D instead of what the game gave us -
// width/height never change, unlike the abandoned upscale spike.
#include <cstdio>

#include "texture_sharpen.h"
#include "config.h"
#include "gl_loader.h"
#include "nis_effect.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_ACTIVE_TEXTURE            = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D        = 0x8069;
const unsigned int GL_TEXTURE_MIN_FILTER        = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER        = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S            = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T            = 0x2803;
const unsigned int GL_LINEAR                    = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE             = 0x812F;
const unsigned int GL_RGBA8                     = 0x8058;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_RGBA                      = 0x1908;
const unsigned int GL_UNSIGNED_BYTE             = 0x1401;
const unsigned int GL_FRAMEBUFFER               = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER          = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER          = 0x8CA9;
const unsigned int GL_READ_FRAMEBUFFER_BINDING  = 0x8CAA;
const unsigned int GL_DRAW_FRAMEBUFFER_BINDING  = 0x8CA6;
const unsigned int GL_COLOR_ATTACHMENT0         = 0x8CE0;
const unsigned int GL_NO_ERROR                  = 0;

// Spike-grade heuristic, not a real classifier - see texture_sharpen.h.
const int kMaxDim = 1024;

unsigned int MakeTexture(const GlComputeApi& gl, unsigned int internalFormat, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
    return texture;
}

bool IsEligible(const AnaxConfig& config, unsigned int target, unsigned int format,
                 unsigned int type, int width, int height, const void* pixels) {
    return config.textureSharpen
        && target == GL_TEXTURE_2D
        && pixels != nullptr
        && format == GL_RGBA
        && type == GL_UNSIGNED_BYTE
        && width > 0 && height > 0
        && width <= kMaxDim && height <= kMaxDim;
}

}  // namespace

void SharpenTextureUpload(RealTexImage2DFn realFn, unsigned int target, int level, int internalformat,
                           int width, int height, int border, unsigned int format, unsigned int type,
                           void* pixels) {
    const AnaxConfig& config = GetAnaxConfig();

    if (!IsEligible(config, target, format, type, width, height, pixels)) {
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
        return;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
        return;
    }

    // Query the binding BEFORE switching the active unit: glTexImage2D always targets
    // whichever unit was active when the game called it, which isn't necessarily unit 0.
    int savedActiveTexture = 0;
    gl.glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    int savedTextureBinding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    int savedDrawFbo = 0;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFbo);

    unsigned int srcTex = MakeTexture(gl, GL_RGBA8, width, height);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);

    unsigned int dstTex = MakeTexture(gl, GL_RGBA16F, width, height);
    bool sharpened = ApplyNVSharpen(srcTex, dstTex, width, height, config.sharpness);

    unsigned char* buffer = nullptr;
    bool readOk = false;
    if (sharpened) {
        unsigned int fbo = 0;
        gl.glGenFramebuffers(1, &fbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        gl.glReadBuffer(GL_COLOR_ATTACHMENT0);

        buffer = new unsigned char[(size_t)width * (size_t)height * 4];
        gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        readOk = gl.glGetError() == GL_NO_ERROR;
        if (!readOk) {
            printf("[opengl32_enh_cpp] texture_sharpen: glGetError() after readback, "
                   "falling back to the original %dx%d upload\n", width, height);
        }

        gl.glDeleteFramebuffers(1, &fbo);
    }

    gl.glDeleteTextures(1, &srcTex);
    gl.glDeleteTextures(1, &dstTex);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
    // Active unit must be restored before the texture binding: glBindTexture always targets
    // whichever unit is currently active, and realFn's upload below must land on the same
    // unit+binding the game had before this call, not unit 0.
    gl.glActiveTexture((unsigned int)savedActiveTexture);
    gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding);

    if (readOk) {
        realFn(target, level, internalformat, width, height, border, format, type, buffer);
    } else {
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
    }
    delete[] buffer;
}
