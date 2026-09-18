// See texture_effect.h. Reuses the existing NVSharpen/CAS/Invert compute-shader effects (see
// nis_effect.h, cas.h, pixel_invert.h) at texture-upload time: upload the game's own pixels into a
// scratch RGBA8 texture, run the selected effect into an RGBA16F texture of the SAME
// dimensions, read the result back to RGBA8, and hand THAT to the real glTexImage2D instead of
// what the game gave us - width/height never change, unlike the abandoned upscale spike.
#include <cstdio>

#include "texture_effect.h"
#include "config.h"
#include "gl_loader.h"
#include "nis_effect.h"
#include "pixel_invert.h"
#include "cas.h"

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
const unsigned int GL_RGB                       = 0x1907;
const unsigned int GL_RGBA                      = 0x1908;
const unsigned int GL_RGB8                      = 0x8051;
const unsigned int GL_UNSIGNED_BYTE             = 0x1401;
const unsigned int GL_FRAMEBUFFER               = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER          = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER          = 0x8CA9;
const unsigned int GL_READ_FRAMEBUFFER_BINDING  = 0x8CAA;
const unsigned int GL_DRAW_FRAMEBUFFER_BINDING  = 0x8CA6;
const unsigned int GL_COLOR_ATTACHMENT0         = 0x8CE0;
const unsigned int GL_NO_ERROR                  = 0;
const unsigned int GL_PIXEL_PACK_BUFFER         = 0x88EB;
const unsigned int GL_PIXEL_PACK_BUFFER_BINDING = 0x88ED;
const unsigned int GL_PIXEL_UNPACK_BUFFER_BINDING = 0x88EF;

// A simple heuristic, not a real texture classifier - see texture_effect.h.
const int kMaxDim = 1024;

// GL_PACK_*/GL_UNPACK_* pixel-store parameters, in a fixed shared order so saving, forcing and
// restoring can all run the same loop. Both the readback below and the substitute upload move
// a TIGHTLY-PACKED buffer, but these are global context state the app may have left on
// non-default values (a non-zero GL_UNPACK_ROW_LENGTH is common when a game uploads a
// sub-rectangle of a larger image) - applying those to our own buffer reads or writes at the
// wrong stride, which corrupts the texture and can run past the end of the allocation.
const unsigned int kPackPnames[6] = {
    0x0D00,  // GL_PACK_SWAP_BYTES
    0x0D01,  // GL_PACK_LSB_FIRST
    0x0D02,  // GL_PACK_ROW_LENGTH
    0x0D03,  // GL_PACK_SKIP_ROWS
    0x0D04,  // GL_PACK_SKIP_PIXELS
    0x0D05,  // GL_PACK_ALIGNMENT
};
const unsigned int kUnpackPnames[6] = {
    0x0CF0,  // GL_UNPACK_SWAP_BYTES
    0x0CF1,  // GL_UNPACK_LSB_FIRST
    0x0CF2,  // GL_UNPACK_ROW_LENGTH
    0x0CF3,  // GL_UNPACK_SKIP_ROWS
    0x0CF4,  // GL_UNPACK_SKIP_PIXELS
    0x0CF5,  // GL_UNPACK_ALIGNMENT
};
// Same order as the pname tables: no swapping, no skipping, rows exactly `width` wide, and
// alignment 1 so an odd width never gets row padding.
const int kTightPixelStore[6] = {0, 0, 0, 0, 0, 1};

void SavePixelStore(const GlComputeApi& gl, const unsigned int (&pnames)[6], int (&out)[6]) {
    for (int i = 0; i < 6; ++i) {
        gl.glGetIntegerv(pnames[i], &out[i]);
    }
}

void SetPixelStore(const GlComputeApi& gl, const unsigned int (&pnames)[6], const int (&values)[6]) {
    for (int i = 0; i < 6; ++i) {
        gl.glPixelStorei(pnames[i], values[i]);
    }
}

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

// `internalformat` decides how the driver STORES what we hand back, so it has to be one where
// an RGBA8 round-trip preserves the app's intent. GL 1.x apps still pass the legacy
// component-count spellings (Anachronox uploads with internalformat 1), and a single- or
// two-channel format would keep only part of a sharpened RGB result. Anything sized, compressed
// or depth/stencil is likewise not ours to reinterpret, so allow-list rather than deny-list.
bool IsSupportedInternalFormat(int internalformat) {
    switch (internalformat) {
        case 3:                  // legacy "3 components", i.e. RGB
        case 4:                  // legacy "4 components", i.e. RGBA
        case (int)GL_RGB:
        case (int)GL_RGBA:
        case (int)GL_RGB8:
        case (int)GL_RGBA8:
            return true;
        default:
            return false;
    }
}

bool IsEligible(unsigned int target, int internalformat, unsigned int format, unsigned int type,
                 int width, int height, const void* pixels) {
    return target == GL_TEXTURE_2D
        && IsSupportedInternalFormat(internalformat)
        && pixels != nullptr
        && format == GL_RGBA
        && type == GL_UNSIGNED_BYTE
        && width > 0 && height > 0
        && width <= kMaxDim && height <= kMaxDim;
}

}  // namespace

void ApplyTextureEffectUpload(RealTexImage2DFn realFn, unsigned int target, int level, int internalformat,
                               int width, int height, int border, unsigned int format, unsigned int type,
                               void* pixels) {
    const AnaxConfig& config = GetAnaxConfig();

    if (config.textureEffect == EffectKind::None) {
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
        return;
    }

    if (!IsEligible(target, internalformat, format, type, width, height, pixels)) {
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
        return;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
        return;
    }

    // With a buffer bound to GL_PIXEL_UNPACK_BUFFER, `pixels` is an offset into THAT buffer,
    // not a client pointer: reading the app's texels from it would dereference an offset as an
    // address, and substituting our own heap pointer would have the driver read it as an
    // offset. Neither is recoverable here, so hand this upload straight through.
    int unpackBufferBinding = 0;
    gl.glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBufferBinding);
    if (unpackBufferBinding != 0) {
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

    // Drain anything the app left pending before we touch GL. glGetError() reports the first
    // error since it was last called, with no notion of who caused it, so without this an error
    // the game generated in its own rendering gets attributed to whichever stage of ours checks
    // next - which is exactly how a "cas: glGetError() = 0x0502" came to be reported for a
    // dispatch that may not have been at fault.
    unsigned int pendingErr = gl.glGetError();
    if (pendingErr != GL_NO_ERROR) {
        static bool warnedPending = false;
        if (!warnedPending) {
            printf("[opengl32_enh_cpp] texture_effect: the app had glGetError() = 0x%04X pending "
                   "before this upload; draining it so it is not blamed on a later stage\n",
                   pendingErr);
            warnedPending = true;
        }
    }

    unsigned int srcTex = MakeTexture(gl, GL_RGBA8, width, height);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);

    unsigned int uploadErr = gl.glGetError();
    if (uploadErr != GL_NO_ERROR) {
        static bool warnedUpload = false;
        if (!warnedUpload) {
            printf("[opengl32_enh_cpp] texture_effect: glGetError() = 0x%04X staging the app's "
                   "%dx%d texture (internalformat=%d, format=0x%04X, type=0x%04X)\n",
                   uploadErr, width, height, internalformat, format, type);
            warnedUpload = true;
        }
    }

    unsigned int dstTex = MakeTexture(gl, GL_RGBA16F, width, height);
    // The None check above already means only Sharpen/Invert reach here, but switch on it
    // explicitly rather than defaulting the else branch to sharpen, so a future third
    // textureEffect value fails loudly instead of quietly running the wrong pass.
    bool applied = false;
    switch (config.textureEffect) {
        case EffectKind::Invert:
            applied = ApplyInvert(srcTex, dstTex, width, height);
            break;
        case EffectKind::Sharpen:
            applied = ApplyNVSharpen(srcTex, dstTex, width, height, config.sharpness);
            break;
        case EffectKind::Cas:
            applied = ApplyCas(srcTex, dstTex, width, height, config.sharpness);
            break;
        default:
            break;
    }

    unsigned char* buffer = nullptr;
    bool readOk = false;
    if (applied) {
        unsigned int fbo = 0;
        gl.glGenFramebuffers(1, &fbo);
        gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
        gl.glReadBuffer(GL_COLOR_ATTACHMENT0);

        buffer = new unsigned char[(size_t)width * (size_t)height * 4];
        int savedPack[6];
        SavePixelStore(gl, kPackPnames, savedPack);
        SetPixelStore(gl, kPackPnames, kTightPixelStore);
        // A bound GL_PIXEL_PACK_BUFFER would send the readback into that buffer and treat our
        // heap pointer as an offset into it, so unbind it for the duration.
        int savedPackBuffer = 0;
        gl.glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &savedPackBuffer);
        gl.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        gl.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        readOk = gl.glGetError() == GL_NO_ERROR;
        gl.glBindBuffer(GL_PIXEL_PACK_BUFFER, (unsigned int)savedPackBuffer);
        SetPixelStore(gl, kPackPnames, savedPack);
        if (!readOk) {
            printf("[opengl32_enh_cpp] texture_effect: glGetError() after readback, "
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
        // Our readback buffer is tightly packed, so this upload needs tight GL_UNPACK_* state
        // rather than whatever the app set for ITS buffer - then put the app's values straight
        // back, since the next glTexImage2D we don't intercept must still see them.
        int savedUnpack[6];
        SavePixelStore(gl, kUnpackPnames, savedUnpack);
        SetPixelStore(gl, kUnpackPnames, kTightPixelStore);
        realFn(target, level, internalformat, width, height, border, format, type, buffer);
        SetPixelStore(gl, kUnpackPnames, savedUnpack);
    } else {
        // Falling back to the app's own pointer, which its own GL_UNPACK_* state describes -
        // leave that state alone.
        realFn(target, level, internalformat, width, height, border, format, type, pixels);
    }
    delete[] buffer;
}
