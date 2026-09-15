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
    ok &= Resolve("glBlitFramebuffer", api.glBlitFramebuffer);
    ok &= Resolve("glGenBuffers", api.glGenBuffers);
    ok &= Resolve("glDeleteBuffers", api.glDeleteBuffers);
    ok &= Resolve("glBindBuffer", api.glBindBuffer);
    ok &= Resolve("glBindBufferBase", api.glBindBufferBase);
    ok &= Resolve("glBufferData", api.glBufferData);
    ok &= Resolve("glBufferSubData", api.glBufferSubData);
    ok &= Resolve("glActiveTexture", api.glActiveTexture);
    ok &= Resolve("glTexStorage2D", api.glTexStorage2D);
    ok &= ResolveLegacy("glGenTextures", api.glGenTextures);
    ok &= ResolveLegacy("glDeleteTextures", api.glDeleteTextures);
    ok &= ResolveLegacy("glBindTexture", api.glBindTexture);
    ok &= ResolveLegacy("glTexParameteri", api.glTexParameteri);
    ok &= ResolveLegacy("glCopyTexSubImage2D", api.glCopyTexSubImage2D);
    ok &= ResolveLegacy("glTexSubImage2D", api.glTexSubImage2D);
    ok &= ResolveLegacy("glReadBuffer", api.glReadBuffer);
    ok &= ResolveLegacy("glGetIntegerv", api.glGetIntegerv);
    ok &= ResolveLegacy("glGetError", api.glGetError);
    ok &= ResolveLegacy("glPixelStorei", api.glPixelStorei);
    ok &= ResolveLegacy("glReadPixels", api.glReadPixels);

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
    // wglSwapBuffers-family hooks, so no locking is needed. Only a successful resolution is
    // cached - a failed attempt (e.g. called before any GL context is current) is retried on
    // every subsequent call instead of latching failure for the rest of the process.
    static GlComputeApi api;
    if (!api.loaded) {
        LoadGlComputeApi(api);
    }
    return api;
}
