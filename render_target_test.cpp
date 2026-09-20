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

bool RelatchesAfterAVideoModeChange() {
    ArmWithReference(3200, 2400, 640, 480);
    NotifyFrameBoundary();
    NotifyGameViewport(0, 0, 1600, 1200);        // the game changed mode
    ArmSupersampleForFrame(true);
    int x = 0, y = 0, w = 1600, h = 1200;
    ScaleGameRect(x, y, w, h);
    return Check(w == 3200 && h == 2400,
                 "a new frame's first viewport re-latches the reference");
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

}  // namespace

int main() {
    ScalesFullFrameViewportToTheConfiguredSize();
    ScalesSubViewportProportionally();
    ScalesByEdgesSoAdjacentRectsAbut();
    IsIdentityWhenNotArmed();
    DoesNotArmWhenTheTargetIsNotReady();
    TakesTheFirstViewportOfTheFrameAsReference();
    RelatchesAfterAVideoModeChange();
    RefusesATargetNarrowerThanTheGameViewport();
    RefusesATargetShorterThanTheGameViewport();
    RefusesWhenOnlyOneDimensionIsSet();
    DoesNotArmBeforeAnyViewportIsSeen();

    printf("%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
