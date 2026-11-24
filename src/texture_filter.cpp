// See texture_filter.h.
#include "texture_filter.h"
#include "config.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                     = 0x0DE1;
const unsigned int GL_TEXTURE_MIN_FILTER             = 0x2801;
const unsigned int GL_NEAREST_MIPMAP_NEAREST         = 0x2700;
const unsigned int GL_LINEAR_MIPMAP_NEAREST          = 0x2701;
const unsigned int GL_NEAREST_MIPMAP_LINEAR          = 0x2702;
const unsigned int GL_LINEAR_MIPMAP_LINEAR           = 0x2703;
const unsigned int GL_TEXTURE_MAX_ANISOTROPY_EXT     = 0x84FE;
const unsigned int GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

bool IsMipmapMinFilter(int param) {
    return param == (int)GL_NEAREST_MIPMAP_NEAREST || param == (int)GL_LINEAR_MIPMAP_NEAREST ||
           param == (int)GL_NEAREST_MIPMAP_LINEAR || param == (int)GL_LINEAR_MIPMAP_LINEAR;
}

// Cached hardware anisotropy cap - refreshed whenever the GL context changes, same reasoning
// as every other cached GL-derived value in this DLL - see gl_loader.h's
// GetGlContextGeneration().
struct AnisoState {
    unsigned int generation = 0;
    bool queried = false;
    float hardwareMax = 1.0f;
};
AnisoState g_aniso;

// The anisotropy level to actually request: the configured value, capped to whatever this
// GPU/driver supports. GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT reads back as 1 when the feature is
// entirely unsupported (vanishingly rare on anything running GL 4.3 compute, but free to
// handle), which naturally degrades this to "trilinear upgrade only, no anisotropic widening"
// rather than a GL error from requesting more than the hardware allows.
float ResolveAnisotropy(const GlComputeApi& gl, float configured) {
    if (g_aniso.generation != GetGlContextGeneration()) {
        g_aniso = AnisoState{};
        g_aniso.generation = GetGlContextGeneration();
    }
    if (!g_aniso.queried) {
        g_aniso.queried = true;
        int maxAnisoInt = 1;
        gl.glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisoInt);
        g_aniso.hardwareMax = (maxAnisoInt >= 1) ? (float)maxAnisoInt : 1.0f;
    }
    float capped = configured;
    if (capped > g_aniso.hardwareMax) { capped = g_aniso.hardwareMax; }
    if (capped < 1.0f) { capped = 1.0f; }
    return capped;
}

// Shared decision, applied identically regardless of which GL entry point (i or f) the game
// used to make this exact call.
bool ShouldUpgrade(unsigned int target, unsigned int pname, int paramAsInt) {
    if (GetAnaxConfig().anisotropy < 1.0f) { return false; }  // feature off - true no-op
    if (target != GL_TEXTURE_2D || pname != GL_TEXTURE_MIN_FILTER) { return false; }
    return IsMipmapMinFilter(paramAsInt);
}

}  // namespace

void ApplyTextureFilterOverride(RealTexParameteriFn realFn, unsigned int target, unsigned int pname, int param) {
    if (!ShouldUpgrade(target, pname, param)) {
        realFn(target, pname, param);
        return;
    }
    // Trilinear only needs this same glTexParameteri entry point (realFn), which is always
    // valid regardless of GL 4.3 compute support - apply it unconditionally.
    realFn(target, pname, (int)GL_LINEAR_MIPMAP_LINEAR);

    // Anisotropy needs glTexParameterf/glGetIntegerv resolved via GlComputeApi, which this DLL
    // only resolves as part of its all-or-nothing GL 4.3 compute bundle (see gl_loader.h) -
    // skip it, not the trilinear upgrade above, when that bundle isn't available.
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        return;
    }
    float aniso = ResolveAnisotropy(gl, GetAnaxConfig().anisotropy);
    gl.glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
}

void ApplyTextureFilterOverrideF(RealTexParameterfFn realFn, unsigned int target, unsigned int pname, float param) {
    if (!ShouldUpgrade(target, pname, (int)param)) {
        realFn(target, pname, param);
        return;
    }
    realFn(target, pname, (float)GL_LINEAR_MIPMAP_LINEAR);

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        return;
    }
    float aniso = ResolveAnisotropy(gl, GetAnaxConfig().anisotropy);
    gl.glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
}
