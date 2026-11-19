// See texture_mipmap.h.
#include "texture_mipmap.h"

#include "config.h"
#include <cstdio>

#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D           = 0x0DE1;
const unsigned int GL_TEXTURE_MIN_FILTER   = 0x2801;
const unsigned int GL_LINEAR_MIPMAP_LINEAR = 0x2703;
const unsigned int GL_LINEAR               = 0x2601;
const unsigned int GL_TEXTURE_BINDING_2D   = 0x8069;

// What this module knows about one texture object.
enum TextureState : unsigned char {
    // Never seen uploaded through glTexImage2D. Says nothing about whether it has a chain, so
    // the only safe answer is to leave it alone: render targets and anything allocated before
    // this hook existed land here.
    kUnknown = 0,
    // Level 0 uploaded and no higher level since - the target population.
    kCandidate = 1,
    // The game supplied a level above 0, so it owns this chain.
    kGameOwnsChain = 2,
    // We generated the chain already. One generation per upload, not one per draw.
    kGenerated = 3,
};

// Indexed directly by texture name. A flat array rather than a map because the lookup sits on
// the draw path - every glBegin the game makes reaches it - so the cost has to be one index
// and a compare, with no hashing and no allocation. id Tech 2-era engines get their names from
// the driver in a low, dense range; anything at or above the cap is simply not tracked, which
// degrades to "leave that texture alone" rather than to anything unsafe.
const unsigned int kMaxTrackedTexture = 16384;
unsigned char g_state[kMaxTrackedTexture];

// The min filter the GAME had on each texture at the moment we replaced it with a mipmapped
// one, so it can be put back. That is not cosmetic: once level 0 is re-uploaded at a different
// size, the chain we generated no longer matches it, and a mipmapped min filter over an
// inconsistent chain makes the texture INCOMPLETE - which fixed-function GL draws as if
// texturing were off, i.e. as a solid quad in the vertex colour. In Anachronox that is the
// white box behind HUD labels: label textures are regenerated in place at a new size, long
// after we mipped them. GL enum values for filters fit in 16 bits.
unsigned short g_savedMinFilter[kMaxTrackedTexture];

// The game's current GL_TEXTURE_2D binding and current projection, as reported by the Notify*
// calls. Both are needed at draw time and neither is queryable cheaply from GL.
unsigned int g_boundTexture = 0;
bool g_worldPass = false;

// Whether the current binding was established since the last projection change. See
// texture_mipmap.h's ClaimMipmapGenerationForBoundTexture: a draw uses whatever is still bound,
// and at the top of a frame's world pass that is the previous frame's HUD texture.
//
// "since the last pass change" rather than "during the world pass" on purpose. The two read the
// same at a glance, but the second spelling - assigning g_worldPass here - is redundant, since
// both pass notifications already clear this flag. A mutation test proved it: changing that
// assignment to an unconditional true altered no observable behaviour, which is the definition
// of code that is not doing anything.
bool g_boundSincePassChange = false;

// The context this state describes. Texture names belong to a context, so everything recorded
// here is meaningless once a different one is current - same reasoning as every other cached
// GL-derived value in this DLL, see gl_loader.h's GetGlContextGeneration().
unsigned int g_generation = 0;

void ClearTextureTable() {
    for (unsigned int i = 0; i < kMaxTrackedTexture; ++i) {
        g_state[i] = kUnknown;
        g_savedMinFilter[i] = 0;
    }
}

// Only the texture table, deliberately - NOT the pass flag or the binding. Those are driven by
// the calls around this one (glFrustum has just run, or the caller is mid-bind), so wiping them
// here destroys state that was correct. Doing exactly that is what made the GPU test's
// world-pass case fail: the first bind after a context appeared reset g_worldPass to false
// immediately after glFrustum had set it.
void DiscardStateIfContextChanged() {
    if (g_generation != GetGlContextGeneration()) {
        g_generation = GetGlContextGeneration();
        ClearTextureTable();
    }
}

}  // namespace

bool NotifyTextureLevelUploaded(unsigned int texture, int level) {
    if (texture >= kMaxTrackedTexture) {
        return false;
    }
    bool invalidatesOurChain = (g_state[texture] == kGenerated);
    // Level 0 re-opens the question unconditionally, including over a texture we already
    // generated for: engines reuse texture names, so new level-0 content in an old name is a
    // different texture as far as this decision goes.
    g_state[texture] = (level == 0) ? kCandidate : kGameOwnsChain;
    return invalidatesOurChain;
}

namespace {

// The texture actually bound to GL_TEXTURE_2D on the ACTIVE unit. g_boundTexture is the last
// glBindTexture the game made on ANY unit - a multitexturing engine binds its lightmap on unit
// 1 and its diffuse on unit 0, and selects units through an extension entry point this DLL
// never sees - so it is not reliable enough to decide which texture a GL call lands on. Only
// queried on paths that are already rare (uploads, a first claim), never per draw.
bool QueryActiveBinding(const GlComputeApi& gl, unsigned int& out) {
    if (!gl.loaded || gl.glGetIntegerv == nullptr) {
        return false;
    }
    int binding = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    out = (unsigned int)binding;
    return true;
}

}  // namespace

void NotifyBoundTextureLevelUploaded(int level) {
    const GlComputeApi& gl = GetGlComputeApi();
    unsigned int texture = g_boundTexture;
    if (GetAnaxConfig().autoMipmap) {
        QueryActiveBinding(gl, texture);
    }
    if (!NotifyTextureLevelUploaded(texture, level)) {
        return;
    }
    // We had replaced this texture's min filter with a mipmapped one; the upload about to happen
    // makes our generated levels stale or (at a new size) inconsistent. Hand the game its own
    // filter back now - it is the bound texture, so no bind is needed. If the texture is drawn
    // in the world pass again it is simply claimed and re-mipped from the new content.
    if (gl.loaded && gl.glTexParameteri != nullptr) {
        unsigned short saved = g_savedMinFilter[texture];
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                           (int)(saved != 0 ? saved : GL_LINEAR));
    }
}

void NotifyBoundTextureSubImageUploaded(int level) {
    if (level != 0 || !GetAnaxConfig().autoMipmap) {
        return;
    }
    const GlComputeApi& gl = GetGlComputeApi();
    unsigned int texture = 0;
    if (!QueryActiveBinding(gl, texture) || texture >= kMaxTrackedTexture ||
        g_state[texture] != kGenerated || gl.glGenerateMipmap == nullptr) {
        return;
    }
    // Same size, so the chain stays complete - but it still shows the OLD content at a distance
    // (a dynamic lightmap that never lights up far away, a label that reads the previous text).
    // Rebuilding is GPU-side only, with no readback, so it does not stall.
    gl.glGenerateMipmap(GL_TEXTURE_2D);
}

void NotifyTexturesDeleted(const unsigned int* textures, int count) {
    if (textures == nullptr) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (textures[i] < kMaxTrackedTexture) {
            g_state[textures[i]] = kUnknown;
        }
    }
}

void NotifyTextureBound(unsigned int texture) {
    // The context check belongs HERE, not at claim time. Every upload and every draw of a
    // texture is preceded by the game binding it, so this runs before anything is recorded -
    // whereas discarding at claim time wiped the very state the upload had just written, and
    // no texture was ever claimed. texture_mipmap_gpu_test.cpp is what caught that.
    // GetGlContextGeneration() is a counter read, so this costs a compare on the bind path.
    DiscardStateIfContextChanged();
    g_boundTexture = texture;
    g_boundSincePassChange = true;
}

void NotifyMipmapWorldPass() {
    g_worldPass = true;
    // A pass change invalidates the binding as evidence, whichever direction it goes in: what
    // was bound before this call was bound for the previous pass's drawing, not this one's.
    // This is the ONLY thing that clears the flag, which is what makes it load-bearing.
    g_boundSincePassChange = false;
}

void NotifyMipmapTwoDPass() {
    g_worldPass = false;
    g_boundSincePassChange = false;
}

bool ClaimMipmapGeneration(unsigned int texture, bool worldPass, bool enabled) {
    if (!enabled || !worldPass) {
        return false;
    }
    if (texture >= kMaxTrackedTexture || g_state[texture] != kCandidate) {
        return false;
    }
    g_state[texture] = kGenerated;
    return true;
}

bool ClaimMipmapGenerationForBoundTexture(bool enabled) {
    if (!g_boundSincePassChange) {
        return false;
    }
    return ClaimMipmapGeneration(g_boundTexture, g_worldPass, enabled);
}

void ApplyAutoMipmapForDraw() {
    if (!GetAnaxConfig().autoMipmap) {     // feature off - a true no-op, no GL query at all
        return;
    }
    if (!g_worldPass) {                    // the safety property, checked before anything else
        return;
    }
    // glGenerateMipmap and glTexParameteri both come from the all-or-nothing GL 4.3 bundle this
    // DLL resolves (see gl_loader.h). Without it there is nothing to generate with, so the
    // claim must not be made either - a granted claim that is not honoured would mark the
    // texture done and leave it permanently unmipped.
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded || gl.glGenerateMipmap == nullptr) {
        return;
    }
    // Cheap pre-check before the one GL query below, so the query is only paid for a texture
    // that would actually be claimed - not on every draw of an already-handled texture.
    if (!g_boundSincePassChange || g_boundTexture >= kMaxTrackedTexture ||
        g_state[g_boundTexture] != kCandidate) {
        return;
    }
    // glGenerateMipmap acts on the ACTIVE unit's binding; if that is not the texture we are
    // about to mark done, we would mip one texture and record another. Leave both alone.
    unsigned int active = 0;
    if (!QueryActiveBinding(gl, active) || active != g_boundTexture) {
        return;
    }
    if (!ClaimMipmapGenerationForBoundTexture(true)) {
        return;
    }
    if (gl.glGetTexParameteriv != nullptr) {
        int gameFilter = 0;
        gl.glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &gameFilter);
        g_savedMinFilter[g_boundTexture] = (unsigned short)gameFilter;
    }
    // The game's own bind is still in effect, so this needs no bind of its own and no restore -
    // same reasoning texture_filter.cpp gives for acting inside the game's glTexParameter call.
    gl.glGenerateMipmap(GL_TEXTURE_2D);
    // Without this the new levels exist and are never sampled: the game left this texture at
    // GL_LINEAR precisely because it had no chain. texture_filter.h's own upgrade cannot do it,
    // since it only ever fires when the GAME sets a mipmap-capable filter, which for these
    // textures it never does.
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR_MIPMAP_LINEAR);

    // "Is this doing anything?" is the first question anyone asks of a feature whose effect is a
    // subtle reduction in distant shimmer, and the honest answer depends on how many textures in
    // this particular game reach here - which varies per engine and per map. Capped rather than
    // per-frame: this fires once per texture, so a couple of dozen lines covers a level load and
    // then stops, instead of growing without bound over a session.
    static int logged = 0;
    if (logged < 32) {
        ++logged;
        printf("[opengl32_enh_cpp] texture_mipmap: generated a mip chain for texture %u "
               "(%d so far)\n", g_boundTexture, logged);
    }
}

void ResetTextureMipmapState() {
    ClearTextureTable();
    // Latch the current context too, so the next bind does not immediately discard the state a
    // caller is in the middle of establishing.
    g_generation = GetGlContextGeneration();
    g_boundTexture = 0;
    g_worldPass = false;
    g_boundSincePassChange = false;
}
