#pragma once

// Generates a mip chain for the game's own textures that were uploaded WITHOUT one, but only
// for the textures actually drawn in the world pass - a real fix for the shimmer id Tech 2-era
// renderers show on any surface they happened not to mip.
//
// texture_filter.h covers the opposite case: textures the game DID mip, whose min filter it
// upgrades to trilinear + anisotropic. The two are complementary and share a hazard. Filtering
// a 2D/UI/HUD/font element across mip levels blurs artwork that was meant to be pixel-exact,
// and an unmipped texture is EXACTLY the signal texture_filter.h uses to identify those - so
// "upload had no mip chain" cannot by itself justify generating one.
//
// The discrimination that does work was measured rather than assumed. A census of Anachronox
// (docs/enhancement-opportunities.md, Tier 4) tagged every texture by the projection in force
// when it was DRAWN: 12 of the 49 textures drawn in the world pass had no mip chain, against 5
// that were drawn only under glOrtho. So the world pass is the signal, and the first version
// of that census - which tagged textures at BIND time - reported zero world-only textures,
// because glTexImage2D binds its texture to upload it and those uploads happen under the
// loading screen's ortho projection. Tag on draw, never on bind.
//
// That has a consequence for the design: the evidence does not exist at upload time. Nothing
// about a glTexImage2D call says whether the texture is a wall or a health bar; only a later
// draw does. So generation is LAZY - the upload is recorded, and the chain is built the first
// time that texture is drawn while the world projection is in force. The cost is a one-frame
// pop per texture the first time it becomes visible; the benefit is that a texture drawn only
// under glOrtho is never touched at all, which is the whole safety property.
//
// Gated behind opengl32_enhancer.ini's `autoMipmap` (false = off, the default - every call
// forwarded exactly as the game made it, a true no-op).

// Records a glTexImage2D upload the game made against GL_TEXTURE_2D. Level 0 makes the texture
// a candidate; ANY level above 0 means the game supplied its own chain and permanently
// disqualifies it, since replacing an engine's deliberate mip chain is not this module's job.
// A later level-0 upload re-opens the question, because engines reuse texture names.
void NotifyTextureLevelUploaded(unsigned int texture, int level);

// The same record, for the texture currently bound to GL_TEXTURE_2D. glTexImage2D names no
// texture - it applies to whatever is bound - so this is what the wrapper actually calls; the
// explicit form above exists so the rule stays testable without a binding to set up.
void NotifyBoundTextureLevelUploaded(int level);

// Records glDeleteTextures, so a recycled texture name cannot inherit the previous texture's
// state. Names are handed out by the driver and id Tech 2-era engines reuse them freely.
void NotifyTexturesDeleted(const unsigned int* textures, int count);

// Records the current GL_TEXTURE_2D binding. The draw-time decision needs to know which
// texture a draw is about to use, and the game's own glBindTexture is the only thing that
// knows. Recording only - this never generates anything, for the bind-time reason above.
void NotifyTextureBound(unsigned int texture);

// Which projection the game last established: glFrustum means the world pass has begun,
// glOrtho the 2D/HUD pass. Same discrimination world_capture.h already relies on, tracked
// separately here so this module stays testable without pulling in frame-capture state.
void NotifyMipmapWorldPass();
void NotifyMipmapTwoDPass();

// The decision, and the claim, in one call: returns true if `texture` should have a mip chain
// generated right now, and marks it done in the same breath so the cost is paid exactly once.
// A second call for the same texture returns false.
//
// Pure with respect to GL - it reads only the state the Notify* calls above recorded - so the
// whole rule is testable with no context. The caller is responsible for actually being able to
// generate (see ApplyAutoMipmapForDraw), because a claim that is granted and then not honoured
// leaves the texture marked done and permanently unmipped.
bool ClaimMipmapGeneration(unsigned int texture, bool worldPass, bool enabled);

// The same claim for the texture currently bound, and what the draw hook actually calls. Adds
// one condition the explicit form cannot express: the binding must have been established since
// the last projection change.
//
// That matters because a draw uses whatever is still bound, which need not be anything this
// draw touches. A frame runs glFrustum -> world draws -> glOrtho -> HUD draws, so when the next
// frame's world pass opens, the leftover binding is a HUD texture; any draw issued before the
// engine rebinds would otherwise hand that HUD artwork to glGenerateMipmap. The same condition
// disposes of upload-time bindings, which happen under the loading screen's ortho projection.
bool ClaimMipmapGenerationForBoundTexture(bool enabled);

// Called from wrapper.cpp's generated draw entry points. Generates the chain for the currently
// bound texture when the claim above is granted, and upgrades its min filter to trilinear so
// the new levels are actually sampled (the game left it at GL_LINEAR, which ignores them).
// Does nothing when the feature is off, when the 2D projection is in force, when the texture
// already has the game's own chain, or when GL 4.3 support is missing.
void ApplyAutoMipmapForDraw();

// Drops every texture's recorded state. Called when the GL context changes - texture names
// from a dead context mean nothing - and by the tests between cases.
void ResetTextureMipmapState();
