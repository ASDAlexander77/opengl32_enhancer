#pragma once

// Runs the game's own textures through a post-effect stage at upload time, in place -
// width/height never change, only pixel content does. An earlier attempt at this also
// upscaled (doubled) the texture, but that silently broke rendering: the game's own UV/atlas
// math and mip chain both assume the size it uploaded, not whatever the driver's texture
// object ends up being, so changing dimensions out from under the game corrupted textures in
// real games (white/mipmap-incomplete, or half-visible from stale UV math). Keeping the size
// fixed avoids both problems entirely - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md for the wider effect pipeline
// this reuses (ApplyNVSharpen/ApplyInvert). Gated behind opengl32_enhancer.ini's
// `textureEffect=sharpen` or `textureEffect=invert` (default `none`, off) - `invert` exists
// as a debug/demo aid for spotting which draws touch which textures, same as the `invert`
// post-process stage.

typedef void (__stdcall *RealTexImage2DFn)(unsigned int target, int level, int internalformat,
    int width, int height, int border, unsigned int format, unsigned int type, void* pixels);

// Called from wrapper.cpp's generated glTexImage2D in place of forwarding straight through.
// If the config's textureEffect is Sharpen or Invert and this upload looks like a plausible
// small color texture (GL_TEXTURE_2D, GL_RGBA/GL_UNSIGNED_BYTE, an RGB/RGBA internalformat -
// including the legacy component-count spellings 3 and 4 - real pixel data, at or under the
// size cap), runs it through that same GPU pass used for the back buffer and calls realFn with
// the SAME width/height but the transformed pixels. Every other case - feature off, wrong
// format/target/internalformat, a render-target allocation (pixels == nullptr), an oversized
// texture, a bound GL_PIXEL_UNPACK_BUFFER (which makes `pixels` an offset rather than a
// pointer), or no GL 4.3 compute support - calls realFn with the original arguments, unchanged.
void ApplyTextureEffectUpload(RealTexImage2DFn realFn, unsigned int target, int level, int internalformat,
    int width, int height, int border, unsigned int format, unsigned int type, void* pixels);
