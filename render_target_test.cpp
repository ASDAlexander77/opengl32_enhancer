// CPU-only checks of render_target.h's scaling and arming rules. No GL context is created here,
// so this target is deliberately absent from CMakeLists.txt's "gpu"-labelled list.
#include <cstdio>

#include "config.h"
#include "render_target.h"

namespace {

int g_failures = 0;

bool Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
    return ok;
}

// The tests drive config directly rather than through an ini file: this target has no fixture,
// and the rule under test is arithmetic over these three numbers.
void Configure(int renderWidth, int renderHeight) {
    GetMutableAnaxConfig().renderWidth = renderWidth;
    GetMutableAnaxConfig().renderHeight = renderHeight;
}

// Puts the module in the state a frame reaches after the game has set its full-frame viewport.
void ArmWithReference(int renderWidth, int renderHeight, int refWidth, int refHeight) {
    ResetRenderTargetState();
    Configure(renderWidth, renderHeight);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, refWidth, refHeight);
    ArmSupersampleForFrame(true);
}

bool ScalesFullFrameViewportToTheConfiguredSize() {
    ArmWithReference(3200, 2400, 640, 480);
    int x = 0, y = 0, w = 640, h = 480;
    ScaleGameRect(x, y, w, h);
    return Check(x == 0 && y == 0 && w == 3200 && h == 2400,
                 "a full-frame viewport scales to exactly the configured size");
}

bool ScalesSubViewportProportionally() {
    ArmWithReference(3200, 2400, 640, 480);      // x5 on both axes
    int x = 100, y = 50, w = 200, h = 100;
    ScaleGameRect(x, y, w, h);
    return Check(x == 500 && y == 250 && w == 1000 && h == 500,
                 "a sub-viewport scales proportionally");
}

// The factor is fractional in general. Scaling origin and size independently and rounding each
// makes adjacent rectangles disagree by a pixel; scaling by edges makes them abut exactly.
//
// 1000/640 = 1.5625, and the origins are deliberately not multiples of anything that makes
// the intermediates whole. An earlier version used x=0,100,200 at x3.75, where every
// intermediate was an exact integer and a width-scaled-independently mutant produced
// identical output - the mutation table said this test pinned edge scaling, and it did not.
bool ScalesByEdgesSoAdjacentRectsAbut() {
    ArmWithReference(1000, 750, 640, 480);
    int ax = 1, ay = 0, aw = 3, ah = 480;
    int bx = 4, by = 0, bw = 3, bh = 480;
    ScaleGameRect(ax, ay, aw, ah);
    ScaleGameRect(bx, by, bw, bh);
    return Check(ax + aw == bx,
                 "adjacent rectangles still abut exactly under a fractional factor");
}

bool IsIdentityWhenNotArmed() {
    ResetRenderTargetState();
    Configure(0, 0);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 640, 480);
    ArmSupersampleForFrame(true);
    int x = 10, y = 20, w = 30, h = 40;
    ScaleGameRect(x, y, w, h);
    return Check(x == 10 && y == 20 && w == 30 && h == 40,
                 "an unconfigured feature leaves the rectangle untouched");
}

// The whole fail-safe: if the framebuffer could not be made, nothing may be scaled, or the game
// renders an oversized viewport into the real back buffer and the player sees a corner of it.
bool DoesNotArmWhenTheTargetIsNotReady() {
    ResetRenderTargetState();
    Configure(3200, 2400);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 640, 480);
    ArmSupersampleForFrame(false);
    int x = 0, y = 0, w = 640, h = 480;
    ScaleGameRect(x, y, w, h);
    return Check(!IsSupersampleActive() && w == 640 && h == 480,
                 "an unusable render target arms nothing and scales nothing");
}

bool TakesTheFirstViewportOfTheFrameAsReference() {
    ResetRenderTargetState();
    Configure(3200, 2400);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 640, 480);          // the full-frame one
    NotifyGameViewport(0, 0, 320, 240);          // a sub-viewport later in the same frame
    ArmSupersampleForFrame(true);
    int x = 0, y = 0, w = 640, h = 480;
    ScaleGameRect(x, y, w, h);
    return Check(w == 3200 && h == 2400,
                 "a later viewport in the same frame does not become the reference");
}

// The ORDER here is the whole test. wrapper.cpp's wglSwapBuffers hook calls
// NotifyFrameBoundary() and ArmSupersampleForFrame() together, and the frame's first
// glViewport - and therefore NotifyGameViewport() - only arrives afterwards, inside the frame
// that was just armed. An earlier version of this test called NotifyGameViewport first, which
// is the one order production never produces, and it is why it could not see that the mode-
// change frame was armed against the old reference and would have scaled against the new one.
bool RelatchesAfterAVideoModeChange() {
    ArmWithReference(3200, 2400, 640, 480);

    // The mode-change frame: armed against 640x480, then told the game is now 1600x1200. The
    // latch is revoked rather than honoured against a reference it was never validated with.
    NotifyFrameBoundary();
    ArmSupersampleForFrame(true);
    NotifyGameViewport(0, 0, 1600, 1200);        // the game changed mode
    int mx = 0, my = 0, mw = 1600, mh = 1200;
    ScaleGameRect(mx, my, mw, mh);
    bool revoked = Check(!IsSupersampleActive() && mw == 1600 && mh == 1200,
                         "the mode-change frame itself disarms rather than scaling by a "
                         "reference it was not armed against");

    // The NEXT frame arms against the new reference in the ordinary way, and its first viewport
    // matches the snapshot, so the latch stands.
    NotifyFrameBoundary();
    ArmSupersampleForFrame(true);
    NotifyGameViewport(0, 0, 1600, 1200);
    int x = 0, y = 0, w = 1600, h = 1200;
    ScaleGameRect(x, y, w, h);
    return Check(w == 3200 && h == 2400,
                 "the frame after a mode change re-latches against the new reference") && revoked;
}

// The reason revocation exists rather than being a tidy-up. A mode change to a size ABOVE the
// configured target, scaled by the stale 640x480 reference, would take a 4000x3000 viewport to
// 20000x15000; scaled by the new reference it would take it DOWN to 3200x2400, which is
// rendering below native - the one thing this feature refuses outright (see
// RefusesATargetNarrowerThanTheGameViewport). Neither is allowed to happen: the frame disarms.
bool DisarmsWhenTheModeChangesAboveTheConfiguredTarget() {
    ArmWithReference(3200, 2400, 640, 480);
    NotifyFrameBoundary();
    ArmSupersampleForFrame(true);
    NotifyGameViewport(0, 0, 4000, 3000);        // a mode change past the configured target
    int x = 0, y = 0, w = 4000, h = 3000;
    ScaleGameRect(x, y, w, h);
    bool disarmed = Check(!IsSupersampleActive() && w == 4000 && h == 3000,
                          "a mode change above the configured target disarms rather than "
                          "scaling by a stale reference");

    // And the frame after it refuses to arm at all, because 3200x2400 is now below the game's
    // own resolution.
    NotifyFrameBoundary();
    ArmSupersampleForFrame(true);
    return Check(!IsSupersampleActive(),
                 "and the frame after it refuses to arm against the larger new mode") && disarmed;
}

// Rendering SMALLER than the game asked for is a different feature wearing this one's name.
//
// Each of these undersizes exactly ONE axis. The original single test undersized both, so
// whichever guard survived a mutation still blocked arming and the mutation was invisible.
bool RefusesATargetNarrowerThanTheGameViewport() {
    ResetRenderTargetState();
    Configure(320, 2400);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 640, 480);
    ArmSupersampleForFrame(true);
    return Check(!IsSupersampleActive(),
                 "a target narrower than the game's own viewport is refused");
}

bool RefusesATargetShorterThanTheGameViewport() {
    ResetRenderTargetState();
    Configure(3200, 240);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 640, 480);
    ArmSupersampleForFrame(true);
    return Check(!IsSupersampleActive(),
                 "a target shorter than the game's own viewport is refused");
}

bool RefusesWhenOnlyOneDimensionIsSet() {
    ResetRenderTargetState();
    Configure(3200, 0);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 640, 480);
    ArmSupersampleForFrame(true);
    return Check(!IsSupersampleActive(),
                 "a half-configured render size is refused rather than guessed");
}

bool DoesNotArmBeforeAnyViewportIsSeen() {
    ResetRenderTargetState();
    Configure(3200, 2400);
    ArmSupersampleForFrame(true);
    return Check(!IsSupersampleActive(),
                 "arming needs a reference viewport, not just a configured size");
}

// A transient degenerate viewport - a window minimise/restore, or any engine codepath that
// zeroes a dimension for a frame - must not become the reference. If it did, arming would
// succeed (renderWidth >= 0 is true for any configured value) while ScaleEdge's own
// denominator guard silently returned every edge unscaled: armed, but not scaling, which is
// the split-brain state this module exists to prevent.
bool IgnoresADegenerateFirstViewport() {
    ResetRenderTargetState();
    Configure(3200, 2400);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 0, 480);
    ArmSupersampleForFrame(true);
    return Check(!IsSupersampleActive(),
                 "a degenerate first viewport does not become the reference");
}

}  // namespace

int main() {
    ScalesFullFrameViewportToTheConfiguredSize();
    ScalesSubViewportProportionally();
    ScalesByEdgesSoAdjacentRectsAbut();
    IsIdentityWhenNotArmed();
    DoesNotArmWhenTheTargetIsNotReady();
    TakesTheFirstViewportOfTheFrameAsReference();
    RelatchesAfterAVideoModeChange();
    DisarmsWhenTheModeChangesAboveTheConfiguredTarget();
    RefusesATargetNarrowerThanTheGameViewport();
    RefusesATargetShorterThanTheGameViewport();
    RefusesWhenOnlyOneDimensionIsSet();
    DoesNotArmBeforeAnyViewportIsSeen();
    IgnoresADegenerateFirstViewport();

    printf("%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
