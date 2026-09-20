#pragma once

// Renders the game into an offscreen framebuffer larger than the window and downsamples that
// image on present - true supersampling, and the one image-quality win windowWidth/windowHeight
// cannot deliver, since that setting stretches the game's image rather than resampling it.
//
// The only lever needed is glViewport. docs/enhancement-opportunities.md assumed this also
// required intercepting GetClientRect, which a proxy opengl32.dll cannot reach; it does not
// matter, because id Tech 2-family engines take their resolution from their own mode table and
// hand it to glViewport rather than asking the window how big it is.
//
// THE INVARIANT: viewport scaling and the framebuffer binding arm and disarm together. A
// scaled viewport without the target bound renders a 3200x2400 viewport into a 2400x1800 back
// buffer, and the player sees the top-left corner blown up. So the decision is latched once per
// frame, in the wglSwapBuffers hook, and read unchanged by both the viewport hook and the
// capture path for the whole of that frame.
//
// Gated behind opengl32_enhancer.ini's renderWidth/renderHeight (both 0 = off, the default -
// every call forwarded exactly as the game made it, a true no-op).

// The next glViewport the game makes is the frame's first, so it is the full-frame viewport
// this module scales everything else against. Called from the wglSwapBuffers hook.
void NotifyFrameBoundary();

// A viewport the game asked for, BEFORE scaling. Latches the reference size when it is the
// first of the frame and does nothing otherwise - recording only.
void NotifyGameViewport(int x, int y, int width, int height);

// The atomic latch. `targetReady` is whether the offscreen framebuffer exists and is usable
// (Task 2's EnsureRenderTarget); everything else the decision needs is config and the reference
// viewport. Pure with respect to GL so the whole rule is testable without a context.
void ArmSupersampleForFrame(bool targetReady);

// Whether supersampling is in force for this frame. Every caller reads this, and it does not
// change mid-frame.
bool IsSupersampleActive();

// Scales a viewport or scissor rectangle the game asked for into render-target space. A true
// identity when supersampling is not armed.
//
// Scales by EDGES, not by origin and size independently: the factor is fractional in general,
// and rounding x and width separately leaves adjacent sub-viewports disagreeing by a pixel,
// which the player sees as seams.
void ScaleGameRect(int& x, int& y, int& width, int& height);

// Drops every piece of recorded state. Called when the GL context changes, and by the tests
// between cases.
void ResetRenderTargetState();
