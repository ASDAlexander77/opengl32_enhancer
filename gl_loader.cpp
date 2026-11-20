#include <cstdio>

#include "gl_loader.h"

// Same no-<windows.h> discipline as the rest of this DLL (see Global Constraints in the
// plan / wrapper.cpp's header comment). Resolved independently of wrapper.cpp's own
// exported wglGetProcAddress wrapper on purpose: that export only exists once this file is
// linked into the proxy DLL, but gl_loader.cpp is also linked directly into
// gl_loader_test.cpp as a standalone .exe (see Task 2's Interfaces) where no such export
// exists. Loading the real opengl32.dll and resolving wglGetProcAddress from it directly
// works identically in both cases, mirroring wrapper.cpp's own EnsureRealOpenGL32 pattern.
typedef unsigned long DWORD;
typedef void* HMODULE;
typedef void* (__stdcall *PFNWGLGETPROCADDRESSPROC)(const char* procName);

extern "C" {
    __declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
    __declspec(dllimport) DWORD __stdcall GetLastError(void);
}

namespace {

HMODULE GetRealOpenGL32Module() {
    static HMODULE real = [] {
        HMODULE result = LoadLibraryA("C:\\Windows\\System32\\opengl32.dll");
        if (result == nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: FAILED to load real opengl32.dll, GetLastError=%lu\n", GetLastError());
        }
        return result;
    }();
    return real;
}

typedef void* (__stdcall *PFNWGLGETCURRENTCONTEXTPROC)(void);

PFNWGLGETCURRENTCONTEXTPROC GetRealWglGetCurrentContext() {
    static PFNWGLGETCURRENTCONTEXTPROC fn = [] {
        HMODULE real = GetRealOpenGL32Module();
        auto result = real ? reinterpret_cast<PFNWGLGETCURRENTCONTEXTPROC>(
                                 GetProcAddress(real, "wglGetCurrentContext"))
                           : nullptr;
        if (result == nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve wglGetCurrentContext - "
                   "GL context changes will not be detected\n");
        }
        return result;
    }();
    return fn;
}

// The context the resolved entry points and every module's cached GL objects belong to, and a
// counter bumped each time it changes. See GetGlContextGeneration() in gl_loader.h.
void* g_lastContext = nullptr;
unsigned int g_contextGeneration = 0;

PFNWGLGETPROCADDRESSPROC GetRealWglGetProcAddress() {
    static PFNWGLGETPROCADDRESSPROC fn = [] {
        HMODULE real = GetRealOpenGL32Module();
        if (real == nullptr) {
            return static_cast<PFNWGLGETPROCADDRESSPROC>(nullptr);
        }
        auto result = reinterpret_cast<PFNWGLGETPROCADDRESSPROC>(GetProcAddress(real, "wglGetProcAddress"));
        if (result == nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve wglGetProcAddress itself, GetLastError=%lu\n", GetLastError());
        }
        return result;
    }();
    return fn;
}

template <typename T>
bool Resolve(const char* name, T& outFn) {
    PFNWGLGETPROCADDRESSPROC wglGetProcAddress = GetRealWglGetProcAddress();
    outFn = wglGetProcAddress ? reinterpret_cast<T>(wglGetProcAddress(name)) : nullptr;
    if (outFn == nullptr) {
        printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve '%s'\n", name);
        return false;
    }
    return true;
}

// GL 1.1 core entry points are NOT reliably resolvable via wglGetProcAddress - see this
// file's header comment. The real opengl32.dll's ordinary static export table (plain
// GetProcAddress) is the correct way to resolve them.
template <typename T>
bool ResolveLegacy(const char* name, T& outFn) {
    HMODULE real = GetRealOpenGL32Module();
    outFn = real ? reinterpret_cast<T>(GetProcAddress(real, name)) : nullptr;
    if (outFn == nullptr) {
        printf("[opengl32_enh_cpp] gl_loader: FAILED to resolve '%s'\n", name);
        return false;
    }
    return true;
}

// Texture names for every texture this DLL creates.
//
// id Tech 2-era engines never call glGenTextures: they choose their own texture numbers
// (Anachronox binds 36, 43, 94, 513... directly), which compatibility-profile GL allows - the
// first glBindTexture of an unused name creates it. glGenTextures, meanwhile, hands out the
// lowest names not yet in use. So any texture of ours created early could be given a number
// the game picks for itself LATER - a dialog's text texture, say. The game then binds our
// texture as its own, and its glTexImage2D fails with GL_INVALID_OPERATION, because ours were
// made with glTexStorage2D and are immutable. It draws whatever our texture holds instead:
// HUD dialogs whose text came out as black boxes with fragments of our pipeline images.
//
// Handing out names from a range far above anything an engine numbers its textures with makes
// the two allocators disjoint. Names are never reused, and a context only ever sees a few
// hundred of ours, so the range cannot run out. Core-profile contexts do not allow a name that
// glGenTextures did not return, and engines that number their own textures do not run on
// them, so there the driver's allocator is used as before.
PFNGLGENTEXTURESPROC g_realGenTextures = nullptr;
bool g_useReservedTextureNames = false;
unsigned int g_nextReservedTextureName = 0x40000000u;

void __stdcall GenReservedTextureNames(int n, unsigned int* textures) {
    if (!g_useReservedTextureNames) {
        if (g_realGenTextures != nullptr) {
            g_realGenTextures(n, textures);
        }
        return;
    }
    for (int i = 0; i < n; ++i) {
        textures[i] = g_nextReservedTextureName++;
    }
}

}  // namespace

bool LoadGlComputeApi(GlComputeApi& api) {
    // Intentionally non-short-circuiting (&=, not &&): every missing entry point should be
    // logged in one run, not just the first one found.
    bool ok = true;
    ok &= Resolve("glBindImageTexture", api.glBindImageTexture);
    ok &= Resolve("glDispatchCompute", api.glDispatchCompute);
    ok &= Resolve("glMemoryBarrier", api.glMemoryBarrier);
    ok &= Resolve("glCreateShader", api.glCreateShader);
    ok &= Resolve("glShaderSource", api.glShaderSource);
    ok &= Resolve("glCompileShader", api.glCompileShader);
    ok &= Resolve("glGetShaderiv", api.glGetShaderiv);
    ok &= Resolve("glGetShaderInfoLog", api.glGetShaderInfoLog);
    ok &= Resolve("glDeleteShader", api.glDeleteShader);
    ok &= Resolve("glCreateProgram", api.glCreateProgram);
    ok &= Resolve("glAttachShader", api.glAttachShader);
    ok &= Resolve("glLinkProgram", api.glLinkProgram);
    ok &= Resolve("glGetProgramiv", api.glGetProgramiv);
    ok &= Resolve("glGetProgramInfoLog", api.glGetProgramInfoLog);
    ok &= Resolve("glDeleteProgram", api.glDeleteProgram);
    ok &= Resolve("glUseProgram", api.glUseProgram);
    ok &= Resolve("glGenFramebuffers", api.glGenFramebuffers);
    ok &= Resolve("glDeleteFramebuffers", api.glDeleteFramebuffers);
    ok &= Resolve("glBindFramebuffer", api.glBindFramebuffer);
    ok &= Resolve("glFramebufferTexture2D", api.glFramebufferTexture2D);
    ok &= Resolve("glCheckFramebufferStatus", api.glCheckFramebufferStatus);
    ok &= Resolve("glBlitFramebuffer", api.glBlitFramebuffer);
    ok &= Resolve("glGenBuffers", api.glGenBuffers);
    ok &= Resolve("glDeleteBuffers", api.glDeleteBuffers);
    ok &= Resolve("glBindBuffer", api.glBindBuffer);
    ok &= Resolve("glBindBufferBase", api.glBindBufferBase);
    ok &= Resolve("glBufferData", api.glBufferData);
    ok &= Resolve("glBufferSubData", api.glBufferSubData);
    ok &= Resolve("glActiveTexture", api.glActiveTexture);
    ok &= Resolve("glTexStorage2D", api.glTexStorage2D);
    ok &= Resolve("glTexStorage3D", api.glTexStorage3D);
    ok &= Resolve("glTexSubImage3D", api.glTexSubImage3D);
    ok &= ResolveLegacy("glGenTextures", api.glGenTextures);
    ok &= ResolveLegacy("glDeleteTextures", api.glDeleteTextures);
    ok &= ResolveLegacy("glBindTexture", api.glBindTexture);
    ok &= ResolveLegacy("glTexParameteri", api.glTexParameteri);
    ok &= Resolve("glGenerateMipmap", api.glGenerateMipmap);
    ok &= ResolveLegacy("glTexParameterf", api.glTexParameterf);
    ok &= ResolveLegacy("glCopyTexSubImage2D", api.glCopyTexSubImage2D);
    ok &= ResolveLegacy("glTexSubImage2D", api.glTexSubImage2D);
    ok &= ResolveLegacy("glReadBuffer", api.glReadBuffer);
    ok &= ResolveLegacy("glGetIntegerv", api.glGetIntegerv);
    ok &= ResolveLegacy("glGetTexParameteriv", api.glGetTexParameteriv);
    ok &= ResolveLegacy("glGetError", api.glGetError);
    ok &= ResolveLegacy("glPixelStorei", api.glPixelStorei);
    ok &= ResolveLegacy("glReadPixels", api.glReadPixels);

    {
        PFNWGLGETPROCADDRESSPROC getProc = GetRealWglGetProcAddress();
        if (getProc != nullptr) {
            api.glGenQueries = reinterpret_cast<PFNGLGENQUERIESPROC>(getProc("glGenQueries"));
            api.glQueryCounter = reinterpret_cast<PFNGLQUERYCOUNTERPROC>(getProc("glQueryCounter"));
            api.glGetQueryObjectiv = reinterpret_cast<PFNGLGETQUERYOBJECTIVPROC>(getProc("glGetQueryObjectiv"));
            api.glGetQueryObjectui64v = reinterpret_cast<PFNGLGETQUERYOBJECTUI64VPROC>(getProc("glGetQueryObjectui64v"));
            api.glClearBufferfv = reinterpret_cast<PFNGLCLEARBUFFERFVPROC>(getProc("glClearBufferfv"));
        }
        HMODULE real = GetRealOpenGL32Module();
        if (real != nullptr) {
            api.glIsEnabled = reinterpret_cast<PFNGLISENABLEDPROC>(GetProcAddress(real, "glIsEnabled"));
            api.glEnable = reinterpret_cast<PFNGLCAPPROC>(GetProcAddress(real, "glEnable"));
            api.glDisable = reinterpret_cast<PFNGLCAPPROC>(GetProcAddress(real, "glDisable"));
        }
    }

    // See GenReservedTextureNames above. Decided per context, from the profile it was created
    // with; the profile-mask query is GL 3.2+, and a context that does not understand it is a
    // compatibility one by definition.
    if (api.glGenTextures != nullptr && api.glGetIntegerv != nullptr) {
        const unsigned int GL_CONTEXT_PROFILE_MASK = 0x9126;
        const int GL_CONTEXT_CORE_PROFILE_BIT = 0x1;
        int profileMask = 0;
        api.glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
        if (api.glGetError != nullptr) {
            api.glGetError();   // an old context rejecting the query must not leave an error
        }
        g_realGenTextures = api.glGenTextures;
        g_useReservedTextureNames = (profileMask & GL_CONTEXT_CORE_PROFILE_BIT) == 0;
        api.glGenTextures = &GenReservedTextureNames;
    }

    api.loaded = ok;
    if (ok) {
        printf("[opengl32_enh_cpp] gl_loader: all GL entry points resolved OK\n");
    } else {
        printf("[opengl32_enh_cpp] gl_loader: one or more GL entry points unavailable - "
               "compute-shader effects (bilinear/nvscaler/nvsharpen) are disabled on this context\n");
    }
    return ok;
}

const GlComputeApi& GetGlComputeApi() {
    // Not synchronized: like wrapper.cpp's EnsureRealOpenGL32 and other lazy-init patterns
    // in this codebase, this is only ever called from the single render thread inside the
    // wglSwapBuffers-family hooks, so no locking is needed. Resolution is attempted at most
    // once per GL context - success caches the table, failure is remembered just as firmly -
    // and a context change discards both (see below), so a failure under one context never
    // latches for the rest of the process.
    static GlComputeApi api;

    // Whether resolution has already been attempted against the context cached below. Failure
    // can only be caused by something fixed for the lifetime of a context - the driver simply
    // doesn't expose these entry points - so retrying inside one context re-runs (and re-logs)
    // forty-odd failing lookups on every single call, i.e. every frame. The context-change
    // reset below is what actually covers the case the old unconditional retry existed for:
    // being called before any context was current resolves nothing, leaves g_lastContext at
    // nullptr, and the first call that DOES have a context current counts as a change.
    static bool triedThisContext = false;

    // Every effect calls this before touching GL, so it's the one chokepoint where a context
    // switch can be caught. Entry points resolved via wglGetProcAddress are only valid for the
    // context that resolved them, so re-resolve; bumping the generation additionally tells
    // every module its cached GL objects belong to a context that is no longer current.
    PFNWGLGETCURRENTCONTEXTPROC wglGetCurrentContext = GetRealWglGetCurrentContext();
    void* currentContext = wglGetCurrentContext ? wglGetCurrentContext() : nullptr;
    if (currentContext != g_lastContext) {
        if (g_lastContext != nullptr) {
            printf("[opengl32_enh_cpp] gl_loader: GL context changed, rebuilding cached state\n");
        }
        g_lastContext = currentContext;
        ++g_contextGeneration;
        api = GlComputeApi{};
        triedThisContext = false;
    }

    if (!api.loaded && !triedThisContext) {
        triedThisContext = true;
        LoadGlComputeApi(api);
    }
    return api;
}

unsigned int GetGlContextGeneration() {
    return g_contextGeneration;
}
