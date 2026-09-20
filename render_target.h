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
// AMENDMENT TO THE INVARIANT - the latch may be REVOKED exactly once per frame. As written
// above the latch was taken and never re-examined, but the reference viewport it is validated
// against is recorded by the NEXT frame's first glViewport, which is after the latch is taken:
// on a video-mode-change frame the latch was decided against the old reference and the scaling
// would then have used the new one. That can scale a rectangle DOWN (a mode change to a size
// above the configured target), which is the one thing this feature refuses outright. So:
//
//   - ArmSupersampleForFrame snapshots the reference it validated against. ScaleGameRect
//     divides by THAT snapshot, never by the live reference, so an armed frame always scales
//     by the numbers its own arming decision was made from.
//   - NotifyGameViewport revokes the latch - disarms for the remainder of the frame and
//     unbinds the render target - when the frame's first viewport is not the snapshot. Either
//     we scale by the reference we validated, or we do not scale at all.
//
// Revocation is not free, and the cost is paid by the revoking frame. Whatever the game drew
// between the swap hook's BindRenderTarget and that first glViewport - in practice a glClear,
// sometimes more - went into the render target we are now abandoning, and is discarded: the
// frame restarts against framebuffer 0 on top of whatever the back buffer already held. One
// frame, on a video mode change, where the game is about to redraw everything anyway. A scissor
// rectangle scaled by the old snapshot can also survive into the revoking frame, since only the
// viewport is re-set at the frame's start. Both are accepted: a mode-change frame that is one
// frame stale is a far better failure than a frame scaled by a reference nothing validated.
//
// Revocation can happen at most once a frame (only the first viewport of a frame is examined)
// and only ever in the safe direction: armed -> unarmed, never the reverse. The frame after a
// mode change arms against the new reference in the ordinary way.
//
// Gated behind opengl32_enhancer.ini's renderWidth/renderHeight (both 0 = off, the default -
// every call forwarded exactly as the game made it, a true no-op).

// The next glViewport the game makes is the frame's first, so it is the full-frame viewport
// this module scales everything else against. Called from the wglSwapBuffers hook.
void NotifyFrameBoundary();

// A viewport the game asked for, BEFORE scaling. Latches the reference size when it is the
// first of the frame and does nothing otherwise. NOT purely recording: a first viewport that
// differs from the size this frame's latch was validated against revokes that latch - see the
// amendment in the header comment above - which also unbinds the render target.
void NotifyGameViewport(int x, int y, int width, int height);

// The atomic latch. `targetReady` is whether the offscreen framebuffer exists and is usable
// (Task 2's EnsureRenderTarget); everything else the decision needs is config and the reference
// viewport. Pure with respect to GL so the whole rule is testable without a context.
// Also snapshots the reference viewport it validated against, which is what ScaleGameRect
// divides by for the rest of the frame.
void ArmSupersampleForFrame(bool targetReady);

// Whether supersampling is in force for this frame. Every caller reads this. Within a frame it
// changes at most once, and only from true to false, at the frame's first viewport - see the
// amendment in the header comment above.
bool IsSupersampleActive();

// Scales a viewport or scissor rectangle the game asked for into render-target space. A true
// identity when supersampling is not armed.
//
// Scales by EDGES, not by origin and size independently: the factor is fractional in general,
// and rounding x and width separately leaves adjacent sub-viewports disagreeing by a pixel,
// which the player sees as seams.
void ScaleGameRect(int& x, int& y, int& width, int& height);

// Drops every piece of recorded state. Called by the tests between cases, and by nothing else -
// in particular NOT on a GL context change, which is instead handled inside EnsureRenderTarget
// by comparing GetGlContextGeneration() against the generation the current framebuffer was
// made under and rebuilding when they differ. This function is therefore a pure test hook, and
// it deliberately does no GL work: it forgets that the render target was bound rather than
// unbinding it, so tests set whatever binding they want to start from explicitly.
void ResetRenderTargetState();

// Creates the offscreen framebuffer at the configured size for the current context, or reuses
// the existing one. Returns false when the feature is off, when GL 4.3 is unavailable, when the
// size exceeds what the driver allows, or when the framebuffer is not complete - and a false
// return is what keeps ArmSupersampleForFrame from arming, so a failure here degrades to exactly
// today's rendering rather than to a broken image.
bool EnsureRenderTarget();

// Binds the offscreen framebuffer so the game's subsequent drawing lands in it. Called from the
// wglSwapBuffers hook after the real SwapBuffers returns.
//
// When the frame is not armed this restores framebuffer 0 IF this module had bound the target
// before, and does nothing at all otherwise. Both halves matter: post_effects.cpp restores
// whatever framebuffer was bound when it was entered - our target, on a supersampled frame - so
// without the restore an armed-then-disarmed run would leave the target bound forever while
// every capture site reads framebuffer 0. And an unconditional glBindFramebuffer(0) would make
// the feature-off path stop being a true no-op, overwriting a binding the game itself set.
void BindRenderTarget();

// The framebuffer the game's drawing is in: the offscreen one when armed, 0 otherwise. Every
// site that captures what the game rendered asks this instead of hardcoding 0.
unsigned int GetGameFramebuffer();

// The buffer to read from it: GL_COLOR_ATTACHMENT0 when armed, GL_BACK otherwise.
// glReadBuffer(GL_BACK) is INVALID against a framebuffer object, so a capture site that forgets
// this one starts throwing GL_INVALID_OPERATION rather than failing visibly.
unsigned int GetGameReadBuffer();

// The offscreen framebuffer's size, for the present blit's source rectangle.
void GetRenderTargetSize(int& width, int& height);

// The game's own full-frame viewport, as latched by NotifyGameViewport. Used for the aspect
// check below; leaves its arguments alone when no reference has been seen yet.
void GetReferenceViewport(int& width, int& height);
