#pragma once

#include <cstddef>

// Resolves the extra GL function pointers the post-process compute-shader effects need
// beyond what wrapper.cpp's generated per-function forwarding already covers
// (BilinearUpscale/NVScaler/NVSharpen - see
// docs/superpowers/specs/2026-09-15-nis-post-effects-design.md). Two categories:
//   - GL 4.3+ functions (compute shaders, image load/store, framebuffer blit, buffers):
//     not part of opengl32.dll's static export table, resolved via wglGetProcAddress.
//   - A handful of GL 1.1 functions (glGenTextures, glGetIntegerv, ...): part of the
//     static export table, but NOT reliably resolvable via wglGetProcAddress (the WGL
//     spec only guarantees it for functions beyond GL 1.1) - resolved via plain
//     GetProcAddress on the real opengl32.dll module instead.
// Both categories are resolved independently of wrapper.cpp's own exported forwarding
// (see gl_loader.cpp's header comment for why) and both live in the same GlComputeApi
// struct/loaded flag, since callers need both together and a context-capability check
// that covers only one category would be meaningless.

typedef void (__stdcall *PFNGLBINDIMAGETEXTUREPROC)(unsigned int unit, unsigned int texture, int level,
    unsigned char layered, int layer, unsigned int access, unsigned int format);
typedef void (__stdcall *PFNGLDISPATCHCOMPUTEPROC)(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ);
typedef void (__stdcall *PFNGLMEMORYBARRIERPROC)(unsigned int barriers);
typedef unsigned int (__stdcall *PFNGLCREATESHADERPROC)(unsigned int type);
typedef void (__stdcall *PFNGLSHADERSOURCEPROC)(unsigned int shader, int count, const char* const* string, const int* length);
typedef void (__stdcall *PFNGLCOMPILESHADERPROC)(unsigned int shader);
typedef void (__stdcall *PFNGLGETSHADERIVPROC)(unsigned int shader, unsigned int pname, int* params);
typedef void (__stdcall *PFNGLGETSHADERINFOLOGPROC)(unsigned int shader, int bufSize, int* length, char* infoLog);
typedef void (__stdcall *PFNGLDELETESHADERPROC)(unsigned int shader);
typedef unsigned int (__stdcall *PFNGLCREATEPROGRAMPROC)(void);
typedef void (__stdcall *PFNGLATTACHSHADERPROC)(unsigned int program, unsigned int shader);
typedef void (__stdcall *PFNGLLINKPROGRAMPROC)(unsigned int program);
typedef void (__stdcall *PFNGLGETPROGRAMIVPROC)(unsigned int program, unsigned int pname, int* params);
typedef void (__stdcall *PFNGLGETPROGRAMINFOLOGPROC)(unsigned int program, int bufSize, int* length, char* infoLog);
typedef void (__stdcall *PFNGLDELETEPROGRAMPROC)(unsigned int program);
typedef void (__stdcall *PFNGLUSEPROGRAMPROC)(unsigned int program);
typedef void (__stdcall *PFNGLGENFRAMEBUFFERSPROC)(int n, unsigned int* framebuffers);
typedef void (__stdcall *PFNGLDELETEFRAMEBUFFERSPROC)(int n, const unsigned int* framebuffers);
typedef void (__stdcall *PFNGLBINDFRAMEBUFFERPROC)(unsigned int target, unsigned int framebuffer);
typedef void (__stdcall *PFNGLFRAMEBUFFERTEXTURE2DPROC)(unsigned int target, unsigned int attachment,
    unsigned int textarget, unsigned int texture, int level);
typedef void (__stdcall *PFNGLBLITFRAMEBUFFERPROC)(int srcX0, int srcY0, int srcX1, int srcY1,
    int dstX0, int dstY0, int dstX1, int dstY1, unsigned int mask, unsigned int filter);
typedef void (__stdcall *PFNGLGENBUFFERSPROC)(int n, unsigned int* buffers);
typedef void (__stdcall *PFNGLDELETEBUFFERSPROC)(int n, const unsigned int* buffers);
typedef void (__stdcall *PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);
typedef void (__stdcall *PFNGLBINDBUFFERBASEPROC)(unsigned int target, unsigned int index, unsigned int buffer);
typedef void (__stdcall *PFNGLBUFFERDATAPROC)(unsigned int target, ptrdiff_t size, const void* data, unsigned int usage);
typedef void (__stdcall *PFNGLBUFFERSUBDATAPROC)(unsigned int target, ptrdiff_t offset, ptrdiff_t size, const void* data);
typedef void (__stdcall *PFNGLACTIVETEXTUREPROC)(unsigned int texture);
typedef void (__stdcall *PFNGLTEXSTORAGE2DPROC)(unsigned int target, int levels, unsigned int internalformat, int width, int height);
typedef void (__stdcall *PFNGLTEXSTORAGE3DPROC)(unsigned int target, int levels, unsigned int internalformat, int width, int height, int depth);
typedef void (__stdcall *PFNGLTEXSUBIMAGE3DPROC)(unsigned int target, int level, int xoffset, int yoffset, int zoffset,
    int width, int height, int depth, unsigned int format, unsigned int type, const void* pixels);
typedef void (__stdcall *PFNGLGENTEXTURESPROC)(int n, unsigned int* textures);
typedef void (__stdcall *PFNGLDELETETEXTURESPROC)(int n, const unsigned int* textures);
typedef void (__stdcall *PFNGLBINDTEXTUREPROC)(unsigned int target, unsigned int texture);
typedef void (__stdcall *PFNGLTEXPARAMETERIPROC)(unsigned int target, unsigned int pname, int param);
typedef void (__stdcall *PFNGLCOPYTEXSUBIMAGE2DPROC)(unsigned int target, int level, int xoffset, int yoffset,
    int x, int y, int width, int height);
typedef void (__stdcall *PFNGLTEXSUBIMAGE2DPROC)(unsigned int target, int level, int xoffset, int yoffset,
    int width, int height, unsigned int format, unsigned int type, const void* pixels);
typedef void (__stdcall *PFNGLREADBUFFERPROC)(unsigned int mode);
typedef void (__stdcall *PFNGLGETINTEGERVPROC)(unsigned int pname, int* params);
typedef unsigned int (__stdcall *PFNGLGETERRORPROC)(void);
typedef void (__stdcall *PFNGLPIXELSTOREIPROC)(unsigned int pname, int param);
typedef void (__stdcall *PFNGLREADPIXELSPROC)(int x, int y, int width, int height,
    unsigned int format, unsigned int type, void* pixels);

struct GlComputeApi {
    PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = nullptr;
    PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = nullptr;
    PFNGLMEMORYBARRIERPROC glMemoryBarrier = nullptr;
    PFNGLCREATESHADERPROC glCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC glDeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC glAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
    PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = nullptr;
    PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
    PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
    PFNGLBINDBUFFERBASEPROC glBindBufferBase = nullptr;
    PFNGLBUFFERDATAPROC glBufferData = nullptr;
    PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
    PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
    PFNGLTEXSTORAGE2DPROC glTexStorage2D = nullptr;
    PFNGLTEXSTORAGE3DPROC glTexStorage3D = nullptr;
    PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D = nullptr;
    PFNGLGENTEXTURESPROC glGenTextures = nullptr;
    PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
    PFNGLBINDTEXTUREPROC glBindTexture = nullptr;
    PFNGLTEXPARAMETERIPROC glTexParameteri = nullptr;
    PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D = nullptr;
    PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = nullptr;
    PFNGLREADBUFFERPROC glReadBuffer = nullptr;
    PFNGLGETINTEGERVPROC glGetIntegerv = nullptr;
    PFNGLGETERRORPROC glGetError = nullptr;
    PFNGLPIXELSTOREIPROC glPixelStorei = nullptr;
    PFNGLREADPIXELSPROC glReadPixels = nullptr;

    bool loaded = false;
};

// Resolves every pointer above via wglGetProcAddress against the real system opengl32.dll.
// A context must already be current (true at every call site - see post_effects.cpp).
// Returns false (leaving api.loaded false) if any pointer comes back null; callers must
// then treat every compute-shader effect as unavailable and fall back to effect=none.
bool LoadGlComputeApi(GlComputeApi& api);

// Cached wrapper around LoadGlComputeApi(): once resolution succeeds, the result is cached
// for the rest of the process and returned as-is on every subsequent call. Until then (e.g.
// if called before any GL context is current), each call re-attempts resolution from
// scratch and re-logs any failures - a failed attempt is never cached, so a later call made
// once a capable context is current can still succeed. Not synchronized; assumes all calls
// come from the single render thread (see gl_loader.cpp).
const GlComputeApi& GetGlComputeApi();
