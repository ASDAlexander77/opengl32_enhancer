#pragma once

// Forces trilinear (GL_LINEAR_MIPMAP_LINEAR) minification filtering and anisotropic filtering
// on the game's own mipmapped textures - a real fix for the "blurry floor a few feet ahead"
// look id Tech 2-era renderers are known for, since anisotropic filtering didn't widely exist
// when they were written and most only ever request GL_LINEAR_MIPMAP_NEAREST ("bilinear mip",
// not true trilinear).
//
// The question is which textures. World/model textures come with a full software-generated
// mip chain and a mipmap-capable GL_TEXTURE_MIN_FILTER from upload - that's what makes them
// safe to sharpen at oblique angles. 2D/UI/HUD/font elements are conventionally uploaded with
// a NON-mipmap min filter (GL_NEAREST or GL_LINEAR) since they're meant to render pixel-exact;
// forcing trilinear/anisotropic on those would blur text and icons that were never supposed to
// be filtered across mip levels. Texture size or upload order aren't reliable ways to tell
// those apart (GL doesn't require glTexParameter calls in any particular order relative to
// glTexImage2D, and either category can be any size).
//
// So this hooks glTexParameteri/glTexParameterf directly instead: whenever the GAME ITSELF
// sets a mipmap-capable GL_TEXTURE_MIN_FILTER on GL_TEXTURE_2D, that IS the engine's own
// per-texture signal "this one uses mip-mapping" - upgrade it to full trilinear and layer
// anisotropic filtering on top, on that exact currently-bound texture, right then, synchronously
// inside the same call (no separate bind needed - the game's own bind is still in effect).
// Every other glTexParameter call (a different pname, a non-mipmap min filter, a non-2D target)
// passes through completely unchanged. Gated behind opengl32_enhancer.ini's `anisotropy`
// (0 = off, the default - every call forwarded exactly as the game made it, a true no-op).

typedef void (__stdcall *RealTexParameteriFn)(unsigned int target, unsigned int pname, int param);
typedef void (__stdcall *RealTexParameterfFn)(unsigned int target, unsigned int pname, float param);

// Called from wrapper.cpp's generated glTexParameteri in place of forwarding straight through.
void ApplyTextureFilterOverride(RealTexParameteriFn realFn, unsigned int target, unsigned int pname, int param);

// Called from wrapper.cpp's generated glTexParameterf in place of forwarding straight through -
// the float-parameter twin of the above, needed because GL_TEXTURE_MIN_FILTER (like most GL
// texture parameters) can legally be set via either the i or f entry point, and real code uses
// both.
void ApplyTextureFilterOverrideF(RealTexParameterfFn realFn, unsigned int target, unsigned int pname, float param);
