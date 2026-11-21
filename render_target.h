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
//
// KNOWN LIMITATION - no stencil. The target carries colour and depth only, and never asks the
// game's own pixel format what it wanted. A game that requested cStencilBits and uses stencil
// gets a framebuffer without one, so every stencil test passes: a stencil-shadow pass loses its
// mask and its darkening quad covers the whole frame, and a stencil-masked mirror or portal
// draws over everything. There is no partial failure and nothing logs it - the frame is simply
// wrong while the feature is on.
//
// Not fixed here because fixing it properly is wider than this attachment. Switching to a
// packed GL_DEPTH24_STENCIL8 makes this framebuffer's depth format disagree with the
// GL_DEPTH_COMPONENT24 texture post_effects.cpp blits the game's depth INTO, and a depth blit
// between mismatched formats is GL_INVALID_OPERATION - so the depth-consuming half of the post
// chain would have to move to the packed format in the same change. That is a real regression
// risk across every depth stage, traded against a fault no available testbed can reproduce:
// Anachronox requests stencil=0 (see the Tier 3 spike in docs/enhancement-opportunities.md),
// which is why this shipped unnoticed and why it is documented rather than guessed at.
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

// The game's own glDrawBuffer/glReadBuffer argument, made valid for wherever its drawing is
// going. While supersampling is armed the game is drawing into our framebuffer object, where
// GL_BACK, GL_FRONT and the other window-system buffers are GL_INVALID_OPERATION - an engine
// that calls glDrawBuffer(GL_BACK) every frame (id Tech 2 does) would raise that error every
// frame, and it would surface as a stray 0x0502 blamed on whatever checks glGetError next.
// Those map to GL_COLOR_ATTACHMENT0, the one buffer the target has. Anything else, and every
// value when not armed, passes through unchanged.
unsigned int TranslateGameColorBuffer(unsigned int mode);

// The offscreen framebuffer's size, for the present blit's source rectangle.
void GetRenderTargetSize(int& width, int& height);

// The game's own full-frame viewport, as latched by NotifyGameViewport. Leaves its arguments
// alone when no reference has been seen yet, so a caller passes its own fallback in and gets it
// back untouched.
//
// post_effects.cpp is the consumer: it is the size the present blit targets when the real
// window's client size cannot be obtained, which on a supersampled frame must NOT be the render
// target's own (larger) size. The aspect-ratio warning that used to be the other consumer now
// reads the reference directly, from the arming path - see ReportSupersampleVerdict.
void GetReferenceViewport(int& width, int& height);

// For the config editor only: makes ApplySelectedEffect() read the frame from `fbo` (colour
// attachment 0 plus its depth attachment, `width` x `height`) instead of the window's back
// buffer, and present the result into the window's bottom-left `presentWidth` x
// `presentHeight` rectangle. While set, IsSupersampleActive() reports true, because that is
// exactly the "the frame lives in an offscreen buffer of known size and is resampled on present"
// path the pipeline already has.
//
// It exists because the editor used to copy a loaded dump into its default framebuffer with one
// colour+depth glBlitFramebuffer, and a depth blit requires identical depth formats on both
// sides - a DEPTH_COMPONENT24 texture against a D24S8 window is GL_INVALID_OPERATION, which
// dropped the colour half too and left the dumped frame never appearing at all.
void SetExternalSourceFramebuffer(unsigned int fbo, int width, int height, int presentWidth,
                                  int presentHeight);
void ClearExternalSourceFramebuffer();
// Overwrites width/height with the external present size when one is set; untouched otherwise.
void GetExternalPresentSize(int& width, int& height);
bool IsExternalSourceActive();

// ---- Widescreen (config.widescreen) ----
//
// For a game that only renders 4:3, with supersampling armed into a WIDER target (3200x2000
// for a 640x480 game). Active only while armed, with the option on and the target at least 1%
// wider than the game's own aspect. While active:
//
//   - A viewport or scissor covering the game's whole frame maps to the whole target; any other
//     rectangle maps into the centred "pillar" - the largest rectangle at the game's aspect -
//     so a small 3D view (a portrait, a model in a menu) keeps its shape and place.
//   - glFrustum issued under a full-frame viewport is widened horizontally by the aspect
//     ratio of the two (1.2 for 4:3 into 16:10): Hor+, more scene at the sides at the same
//     vertical field of view. Under a smaller viewport it is left alone.
//   - glOrtho under a full-frame viewport is widened about its centre by the same factor, so
//     the HUD and menus land in the pillar at their own shape instead of being stretched.
//   - The target's colour is cleared to black at the start of each frame, since a 2D-only
//     frame (a menu) no longer covers the sides.
//
// The game still culls against its own 4:3 view, so geometry that only the widened sides show
// can be missing: Quake II-family engines take `+set r_nocull 1` for the world; entities are
// culled regardless.
//
// The target's WIDTH follows the window: with widescreen on and the window's client size known
// (NotifyWindowSize, from the swap hook), the target is renderHeight x (renderHeight * window
// aspect), so the widening factor is always window aspect / game aspect - renderWidth then only
// has to be positive. Before the first swap, or with the size unknown, renderWidth is used.
void NotifyWindowSize(int width, int height);
bool IsWidescreenActive();
// The horizontal widening factor while active (target aspect / game aspect), 1 otherwise.
double WidescreenFactor();
// Called from the glFrustum and glOrtho hooks before they forward; no-ops unless active and
// the last game viewport covered the whole frame.
void WidenFrustumForWidescreen(double& left, double& right);
void WidenOrthoForWidescreen(double& left, double& right);
// Clears the bound target to black when active - called right after BindRenderTarget().
void ClearRenderTargetForWidescreen();

// Full-width fills under the widened ortho. A 2D fill the game means to cover the whole
// screen - a fade (Draw_FadeScreen), cinematic letterbox bars, a black backdrop - spans exactly
// the game's ortho edges (0..640), so after widening it would only cover the centre pillar and
// leave the sides showing. While the widened ortho is current, an untextured quad / fan /
// polygon whose first two vertices sit on the ortho's left and right edges has its edge
// vertices pushed out to the widened edges; anything else (text, pictures, boxes that do not
// span the screen) passes through untouched. Textured quads are never stretched, so a
// full-screen picture keeps its shape.
//
// The first vertex of a watched primitive has to be held back until the second one shows
// whether the pair spans the screen, so the vertex hooks forward what these return instead of
// their own arguments:
//   NotifyPrimitiveBegin(mode)      from glBegin (queries GL_TEXTURE_2D only when it matters)
//   FilterFillVertex(x, y, z, out)  from every glVertex2*/3* hook while IsFillWatchActive();
//                                   writes 0-2 vertices (x,y,z triples) to out, returns count
//   FlushFillVertex(out)            from glEnd: a still-held vertex (0 or 1) to emit
void NotifyPrimitiveBegin(unsigned int mode);
bool IsFillWatchActive();
int FilterFillVertex(float x, float y, float z, float out[6]);
int FlushFillVertex(float out[3]);
// Test hook: NotifyPrimitiveBegin with the texture state supplied instead of queried.
void BeginFillPrimitiveForTest(unsigned int mode, bool textured);
// Number of fills stretched so far (for the log and tests).
int GetStretchedFillCount();
