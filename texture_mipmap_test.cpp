// CPU-only checks of texture_mipmap.h's decision rule. No GL context is created here, so this
// target is deliberately absent from CMakeLists.txt's "gpu"-labelled list.
//
// Every case below names the production behaviour it pins. The one that matters most is
// RejectsTwoDOnlyTexture: it is the entire safety property of the feature, and the reason the
// module exists in the shape it does rather than generating at upload time.
#include <cstdio>

#include "texture_mipmap.h"

namespace {

int g_failures = 0;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
    return ok;
}

const bool kWorldPass = true;
const bool kTwoDPass = false;
const bool kEnabled = true;
const bool kDisabled = false;

// A texture the game uploaded with level 0 and nothing else - the whole target population.
bool ClaimsLevelZeroOnlyTextureInWorldPass() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    return Check(ClaimMipmapGeneration(7, kWorldPass, kEnabled),
                 "a level-0-only texture drawn in the world pass is claimed");
}

// Generation must happen once, not once per draw call. Without this the hot path would rebuild
// the chain for every glBegin that touches the texture.
bool ClaimsOnlyOnce() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    ClaimMipmapGeneration(7, kWorldPass, kEnabled);
    return Check(!ClaimMipmapGeneration(7, kWorldPass, kEnabled),
                 "a second claim for the same texture is refused");
}

// THE safety property: a texture only ever drawn under glOrtho is HUD/UI artwork, and mipping
// it blurs exactly what texture_filter.h protects.
bool RejectsTwoDOnlyTexture() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    return Check(!ClaimMipmapGeneration(7, kTwoDPass, kEnabled),
                 "a level-0-only texture drawn in the 2D pass is NEVER claimed");
}

// The game supplying any level above 0 is it declaring ownership of the chain.
bool RejectsTextureWithItsOwnChain() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    NotifyTextureLevelUploaded(7, 1);
    return Check(!ClaimMipmapGeneration(7, kWorldPass, kEnabled),
                 "a texture the game mipped itself is not claimed");
}

// Off means off - a true no-op, same contract every other feature in this DLL states.
bool RejectsWhenDisabled() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    return Check(!ClaimMipmapGeneration(7, kWorldPass, kDisabled),
                 "nothing is claimed while the feature is off");
}

// A texture this module never saw uploaded says nothing about whether it has a chain, so the
// safe answer is no. Covers render targets and anything allocated before the hook existed.
bool RejectsUnknownTexture() {
    ResetTextureMipmapState();
    return Check(!ClaimMipmapGeneration(99, kWorldPass, kEnabled),
                 "a texture with no recorded upload is not claimed");
}

// Texture names are recycled by the driver; state must not survive the object it described.
bool ForgetsDeletedTexture() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    const unsigned int deleted[] = {7};
    NotifyTexturesDeleted(deleted, 1);
    return Check(!ClaimMipmapGeneration(7, kWorldPass, kEnabled),
                 "a deleted texture's candidacy does not survive into the recycled name");
}

// Re-uploading level 0 over a texture we already mipped is new content in the same name, so
// the question re-opens rather than staying answered from the previous contents.
bool ReopensAfterLevelZeroReupload() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    ClaimMipmapGeneration(7, kWorldPass, kEnabled);
    NotifyTextureLevelUploaded(7, 0);
    return Check(ClaimMipmapGeneration(7, kWorldPass, kEnabled),
                 "re-uploading level 0 makes the texture a candidate again");
}

// A texture drawn in the 2D pass first and the world pass later is still world content - the
// rejection above must be per-decision, not a permanent blacklisting.
bool ClaimsAfterEarlierTwoDRejection() {
    ResetTextureMipmapState();
    NotifyTextureLevelUploaded(7, 0);
    ClaimMipmapGeneration(7, kTwoDPass, kEnabled);
    return Check(ClaimMipmapGeneration(7, kWorldPass, kEnabled),
                 "a 2D-pass rejection does not disqualify a later world-pass draw");
}

// The draw hook sees whatever texture is STILL bound, which need not be one this draw uses.
// A frame runs glFrustum (world) -> world draws -> glOrtho (HUD) -> HUD draws, so at the top of
// the next frame's world pass the binding left over is a HUD texture. If the engine issues any
// draw before rebinding, a naive "bound texture + world pass" rule would mip that HUD artwork -
// the exact outcome this module exists to avoid. Hence: the binding must have been established
// since the world pass began.
bool RejectsBindingLeftOverFromTheTwoDPass() {
    ResetTextureMipmapState();
    NotifyMipmapTwoDPass();
    NotifyTextureBound(7);                  // a HUD texture, bound under ortho
    NotifyBoundTextureLevelUploaded(0);
    NotifyMipmapWorldPass();                // next frame's world pass begins, no rebind
    return Check(!ClaimMipmapGenerationForBoundTexture(kEnabled),
                 "a binding left over from the 2D pass is not claimed in the world pass");
}

bool ClaimsBindingEstablishedInsideTheWorldPass() {
    ResetTextureMipmapState();
    NotifyMipmapWorldPass();
    NotifyTextureBound(7);
    NotifyBoundTextureLevelUploaded(0);
    return Check(ClaimMipmapGenerationForBoundTexture(kEnabled),
                 "a texture bound inside the world pass is claimed");
}

// An upload binds its texture, and uploads happen under the loading screen's ortho projection -
// the contamination that broke the first census. The rule must not treat that bind as evidence.
bool RejectsUploadTimeBindingUnderOrtho() {
    ResetTextureMipmapState();
    NotifyMipmapTwoDPass();
    NotifyTextureBound(7);
    NotifyBoundTextureLevelUploaded(0);
    return Check(!ClaimMipmapGenerationForBoundTexture(kEnabled),
                 "an upload-time binding under the 2D projection is not claimed");
}

}  // namespace

int main() {
    ClaimsLevelZeroOnlyTextureInWorldPass();
    ClaimsOnlyOnce();
    RejectsTwoDOnlyTexture();
    RejectsTextureWithItsOwnChain();
    RejectsWhenDisabled();
    RejectsUnknownTexture();
    ForgetsDeletedTexture();
    ReopensAfterLevelZeroReupload();
    ClaimsAfterEarlierTwoDRejection();
    RejectsBindingLeftOverFromTheTwoDPass();
    ClaimsBindingEstablishedInsideTheWorldPass();
    RejectsUploadTimeBindingUnderOrtho();

    printf("%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
