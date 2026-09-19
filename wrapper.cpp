// AUTO-GENERATED opengl32 interception/passthrough wrapper - C++ x86 proof of concept.
//
// Same forwarding logic and debug tracing as wrapper.ts (see generators/gen_wrapper.py),
// but plain C++ so it can be built with the ordinary MSVC x86 toolchain: the tslang
// toolchain only has x64 runtime libraries (TypeScriptAsyncRuntime.lib, LLVMSupport.lib)
// installed right now, so a real wrapper.ts x86 build needs an x86 LLVM/MLIR rebuild
// first. This file exists to prove the 32-bit build is needed/works before investing in
// that rebuild - see generators/gen_wrapper_cpp.py.
//
// NOT wrapped (signature genuinely unknown/undocumented): GlmfBeginGlsBlock, GlmfCloseMetaFile, GlmfEndGlsBlock, GlmfEndPlayback, GlmfInitPlayback, GlmfPlayGlsRecord, glDebugEntry.

#include <cstdio>
#include "pixel_invert.h"
#include "post_effects.h"
#include "texture_effect.h"
#include "texture_filter.h"
#include "projection_capture.h"
#include "modelview_capture.h"
#include "world_capture.h"
#include "taa_jitter.h"
#include "window_override.h"

// Same-size stand-ins for the Windows/GL typedefs this file needs, defined by hand so we
// never #include <windows.h> (it drags in <wingdi.h>, which declares these wgl*/gl*
// functions itself - as dllimport - and that conflicts with our own dllexport definitions
// of the identical names).
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned int UINT;
typedef float FLOAT;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;

const int MAX_PATH = 260;

// Per-call tracing, OFF by default and deliberately so.
//
// Every exported entry point below can announce itself, which is invaluable when working out
// what a game actually calls - but there are ~370 of them and an id Tech 2-era engine drives
// immediate mode, so glVertex3f alone runs per vertex per frame. Once debug_log.h started
// routing stdout to a real file, an unconditional trace wrote ~3.5 MB/s (about 12 GB/hour) and
// put a formatted file write in the hottest path in the process. It was only ever harmless
// because the output had nowhere to go.
//
// Compile-time rather than a config key: this must cost exactly nothing when off, and a runtime
// check inside glVertex3f is not nothing. Build with -DANAX_TRACE_CALLS=1 when you need it.
#ifndef ANAX_TRACE_CALLS
#define ANAX_TRACE_CALLS 0
#endif

#if ANAX_TRACE_CALLS
#define ANAX_TRACE(name) printf("[opengl32_enh_cpp] call " name "\n")
#else
#define ANAX_TRACE(name) ((void)0)
#endif

extern "C" {
    __declspec(dllimport) void* __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
    __declspec(dllimport) DWORD __stdcall GetLastError(void);
    __declspec(dllimport) UINT __stdcall GetSystemDirectoryA(char* lpBuffer, UINT uSize);
}

static void* g_real = nullptr;

// Resolves the real opengl32.dll's directory via the GetSystemDirectoryA API instead
// of a hardcoded "C:\\Windows\\System32\\..." literal - Windows doesn't have to be
// installed on the C: drive or named "Windows". GetSystemDirectoryA itself always
// returns the textual "...\\System32" path, on WOW64 included: it's LoadLibraryA
// opening that path from this 32-bit process that gets transparently redirected to
// SysWOW64 by the OS's WOW64 file-system redirector, the same redirection a hardcoded
// System32 literal would have ridden too - so this change isn't what makes a 32-bit
// opengl32.dll get found (the redirector already did that), it's what makes the path
// come from Windows' own API instead of an assumption about where Windows lives.
static void* EnsureRealOpenGL32() {
    if (g_real == nullptr) {
        char sysDir[MAX_PATH];
        UINT sysDirLen = GetSystemDirectoryA(sysDir, sizeof(sysDir));
        if (sysDirLen == 0 || sysDirLen >= sizeof(sysDir)) {
            printf("[opengl32_enh_cpp] FAILED to get system directory, GetLastError=%lu\n", GetLastError());
            return nullptr;
        }
        char dllPath[MAX_PATH + 16];
        snprintf(dllPath, sizeof(dllPath), "%s\\opengl32.dll", sysDir);
        printf("[opengl32_enh_cpp] loading real opengl32.dll from %s ...\n", dllPath);
        g_real = LoadLibraryA(dllPath);
        if (g_real == nullptr) {
            printf("[opengl32_enh_cpp] FAILED to load real opengl32.dll, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp] real opengl32.dll loaded OK\n");
        }
    }
    return g_real;
}

typedef void (__stdcall *__pfn_glAccum)(GLenum, GLfloat);
static __pfn_glAccum __proc_glAccum = nullptr;

extern "C" __declspec(dllexport) void __stdcall glAccum(GLenum op, GLfloat value) {
    ANAX_TRACE("glAccum");
    if (__proc_glAccum == nullptr) {
        __proc_glAccum = (__pfn_glAccum)GetProcAddress(EnsureRealOpenGL32(), "glAccum");
        if (__proc_glAccum == nullptr) {
            printf("[opengl32_enh_cpp]   glAccum: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glAccum: resolved OK\n");
        }
    }
    __proc_glAccum(op, value);
}

typedef void (__stdcall *__pfn_glAlphaFunc)(GLenum, GLclampf);
static __pfn_glAlphaFunc __proc_glAlphaFunc = nullptr;

extern "C" __declspec(dllexport) void __stdcall glAlphaFunc(GLenum func, GLclampf ref) {
    ANAX_TRACE("glAlphaFunc");
    if (__proc_glAlphaFunc == nullptr) {
        __proc_glAlphaFunc = (__pfn_glAlphaFunc)GetProcAddress(EnsureRealOpenGL32(), "glAlphaFunc");
        if (__proc_glAlphaFunc == nullptr) {
            printf("[opengl32_enh_cpp]   glAlphaFunc: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glAlphaFunc: resolved OK\n");
        }
    }
    __proc_glAlphaFunc(func, ref);
}

typedef GLboolean (__stdcall *__pfn_glAreTexturesResident)(GLsizei, void*, void*);
static __pfn_glAreTexturesResident __proc_glAreTexturesResident = nullptr;

extern "C" __declspec(dllexport) GLboolean __stdcall glAreTexturesResident(GLsizei n, void* textures, void* residences) {
    ANAX_TRACE("glAreTexturesResident");
    if (__proc_glAreTexturesResident == nullptr) {
        __proc_glAreTexturesResident = (__pfn_glAreTexturesResident)GetProcAddress(EnsureRealOpenGL32(), "glAreTexturesResident");
        if (__proc_glAreTexturesResident == nullptr) {
            printf("[opengl32_enh_cpp]   glAreTexturesResident: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glAreTexturesResident: resolved OK\n");
        }
    }
    return __proc_glAreTexturesResident(n, textures, residences);
}

typedef void (__stdcall *__pfn_glArrayElement)(GLint);
static __pfn_glArrayElement __proc_glArrayElement = nullptr;

extern "C" __declspec(dllexport) void __stdcall glArrayElement(GLint i) {
    ANAX_TRACE("glArrayElement");
    if (__proc_glArrayElement == nullptr) {
        __proc_glArrayElement = (__pfn_glArrayElement)GetProcAddress(EnsureRealOpenGL32(), "glArrayElement");
        if (__proc_glArrayElement == nullptr) {
            printf("[opengl32_enh_cpp]   glArrayElement: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glArrayElement: resolved OK\n");
        }
    }
    __proc_glArrayElement(i);
}

typedef void (__stdcall *__pfn_glBegin)(GLenum);
static __pfn_glBegin __proc_glBegin = nullptr;

extern "C" __declspec(dllexport) void __stdcall glBegin(GLenum mode) {
    ANAX_TRACE("glBegin");
    if (__proc_glBegin == nullptr) {
        __proc_glBegin = (__pfn_glBegin)GetProcAddress(EnsureRealOpenGL32(), "glBegin");
        if (__proc_glBegin == nullptr) {
            printf("[opengl32_enh_cpp]   glBegin: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glBegin: resolved OK\n");
        }
    }
    __proc_glBegin(mode);
}

typedef void (__stdcall *__pfn_glBindTexture)(GLenum, GLuint);
static __pfn_glBindTexture __proc_glBindTexture = nullptr;

extern "C" __declspec(dllexport) void __stdcall glBindTexture(GLenum target, GLuint texture) {
    ANAX_TRACE("glBindTexture");
    if (__proc_glBindTexture == nullptr) {
        __proc_glBindTexture = (__pfn_glBindTexture)GetProcAddress(EnsureRealOpenGL32(), "glBindTexture");
        if (__proc_glBindTexture == nullptr) {
            printf("[opengl32_enh_cpp]   glBindTexture: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glBindTexture: resolved OK\n");
        }
    }
    __proc_glBindTexture(target, texture);
}

typedef void (__stdcall *__pfn_glBitmap)(GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, void*);
static __pfn_glBitmap __proc_glBitmap = nullptr;

extern "C" __declspec(dllexport) void __stdcall glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, void* bitmap) {
    ANAX_TRACE("glBitmap");
    if (__proc_glBitmap == nullptr) {
        __proc_glBitmap = (__pfn_glBitmap)GetProcAddress(EnsureRealOpenGL32(), "glBitmap");
        if (__proc_glBitmap == nullptr) {
            printf("[opengl32_enh_cpp]   glBitmap: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glBitmap: resolved OK\n");
        }
    }
    __proc_glBitmap(width, height, xorig, yorig, xmove, ymove, bitmap);
}

typedef void (__stdcall *__pfn_glBlendFunc)(GLenum, GLenum);
static __pfn_glBlendFunc __proc_glBlendFunc = nullptr;

extern "C" __declspec(dllexport) void __stdcall glBlendFunc(GLenum sfactor, GLenum dfactor) {
    ANAX_TRACE("glBlendFunc");
    if (__proc_glBlendFunc == nullptr) {
        __proc_glBlendFunc = (__pfn_glBlendFunc)GetProcAddress(EnsureRealOpenGL32(), "glBlendFunc");
        if (__proc_glBlendFunc == nullptr) {
            printf("[opengl32_enh_cpp]   glBlendFunc: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glBlendFunc: resolved OK\n");
        }
    }
    __proc_glBlendFunc(sfactor, dfactor);
}

typedef void (__stdcall *__pfn_glCallList)(GLuint);
static __pfn_glCallList __proc_glCallList = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCallList(GLuint list) {
    ANAX_TRACE("glCallList");
    if (__proc_glCallList == nullptr) {
        __proc_glCallList = (__pfn_glCallList)GetProcAddress(EnsureRealOpenGL32(), "glCallList");
        if (__proc_glCallList == nullptr) {
            printf("[opengl32_enh_cpp]   glCallList: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCallList: resolved OK\n");
        }
    }
    __proc_glCallList(list);
}

typedef void (__stdcall *__pfn_glCallLists)(GLsizei, GLenum, void*);
static __pfn_glCallLists __proc_glCallLists = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCallLists(GLsizei n, GLenum type, void* lists) {
    ANAX_TRACE("glCallLists");
    if (__proc_glCallLists == nullptr) {
        __proc_glCallLists = (__pfn_glCallLists)GetProcAddress(EnsureRealOpenGL32(), "glCallLists");
        if (__proc_glCallLists == nullptr) {
            printf("[opengl32_enh_cpp]   glCallLists: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCallLists: resolved OK\n");
        }
    }
    __proc_glCallLists(n, type, lists);
}

typedef void (__stdcall *__pfn_glClear)(GLbitfield);
static __pfn_glClear __proc_glClear = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClear(GLbitfield mask) {
    ANAX_TRACE("glClear");
    if (__proc_glClear == nullptr) {
        __proc_glClear = (__pfn_glClear)GetProcAddress(EnsureRealOpenGL32(), "glClear");
        if (__proc_glClear == nullptr) {
            printf("[opengl32_enh_cpp]   glClear: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClear: resolved OK\n");
        }
    }
    __proc_glClear(mask);
}

typedef void (__stdcall *__pfn_glClearAccum)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glClearAccum __proc_glClearAccum = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    ANAX_TRACE("glClearAccum");
    if (__proc_glClearAccum == nullptr) {
        __proc_glClearAccum = (__pfn_glClearAccum)GetProcAddress(EnsureRealOpenGL32(), "glClearAccum");
        if (__proc_glClearAccum == nullptr) {
            printf("[opengl32_enh_cpp]   glClearAccum: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClearAccum: resolved OK\n");
        }
    }
    __proc_glClearAccum(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glClearColor)(GLclampf, GLclampf, GLclampf, GLclampf);
static __pfn_glClearColor __proc_glClearColor = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    ANAX_TRACE("glClearColor");
    if (__proc_glClearColor == nullptr) {
        __proc_glClearColor = (__pfn_glClearColor)GetProcAddress(EnsureRealOpenGL32(), "glClearColor");
        if (__proc_glClearColor == nullptr) {
            printf("[opengl32_enh_cpp]   glClearColor: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClearColor: resolved OK\n");
        }
    }
    __proc_glClearColor(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glClearDepth)(GLclampd);
static __pfn_glClearDepth __proc_glClearDepth = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClearDepth(GLclampd depth) {
    ANAX_TRACE("glClearDepth");
    if (__proc_glClearDepth == nullptr) {
        __proc_glClearDepth = (__pfn_glClearDepth)GetProcAddress(EnsureRealOpenGL32(), "glClearDepth");
        if (__proc_glClearDepth == nullptr) {
            printf("[opengl32_enh_cpp]   glClearDepth: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClearDepth: resolved OK\n");
        }
    }
    __proc_glClearDepth(depth);
}

typedef void (__stdcall *__pfn_glClearIndex)(GLfloat);
static __pfn_glClearIndex __proc_glClearIndex = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClearIndex(GLfloat c) {
    ANAX_TRACE("glClearIndex");
    if (__proc_glClearIndex == nullptr) {
        __proc_glClearIndex = (__pfn_glClearIndex)GetProcAddress(EnsureRealOpenGL32(), "glClearIndex");
        if (__proc_glClearIndex == nullptr) {
            printf("[opengl32_enh_cpp]   glClearIndex: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClearIndex: resolved OK\n");
        }
    }
    __proc_glClearIndex(c);
}

typedef void (__stdcall *__pfn_glClearStencil)(GLint);
static __pfn_glClearStencil __proc_glClearStencil = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClearStencil(GLint s) {
    ANAX_TRACE("glClearStencil");
    if (__proc_glClearStencil == nullptr) {
        __proc_glClearStencil = (__pfn_glClearStencil)GetProcAddress(EnsureRealOpenGL32(), "glClearStencil");
        if (__proc_glClearStencil == nullptr) {
            printf("[opengl32_enh_cpp]   glClearStencil: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClearStencil: resolved OK\n");
        }
    }
    __proc_glClearStencil(s);
}

typedef void (__stdcall *__pfn_glClipPlane)(GLenum, void*);
static __pfn_glClipPlane __proc_glClipPlane = nullptr;

extern "C" __declspec(dllexport) void __stdcall glClipPlane(GLenum plane, void* equation) {
    ANAX_TRACE("glClipPlane");
    if (__proc_glClipPlane == nullptr) {
        __proc_glClipPlane = (__pfn_glClipPlane)GetProcAddress(EnsureRealOpenGL32(), "glClipPlane");
        if (__proc_glClipPlane == nullptr) {
            printf("[opengl32_enh_cpp]   glClipPlane: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glClipPlane: resolved OK\n");
        }
    }
    __proc_glClipPlane(plane, equation);
}

typedef void (__stdcall *__pfn_glColor3b)(GLbyte, GLbyte, GLbyte);
static __pfn_glColor3b __proc_glColor3b = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    ANAX_TRACE("glColor3b");
    if (__proc_glColor3b == nullptr) {
        __proc_glColor3b = (__pfn_glColor3b)GetProcAddress(EnsureRealOpenGL32(), "glColor3b");
        if (__proc_glColor3b == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3b: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3b: resolved OK\n");
        }
    }
    __proc_glColor3b(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3bv)(void*);
static __pfn_glColor3bv __proc_glColor3bv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3bv(void* v) {
    ANAX_TRACE("glColor3bv");
    if (__proc_glColor3bv == nullptr) {
        __proc_glColor3bv = (__pfn_glColor3bv)GetProcAddress(EnsureRealOpenGL32(), "glColor3bv");
        if (__proc_glColor3bv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3bv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3bv: resolved OK\n");
        }
    }
    __proc_glColor3bv(v);
}

typedef void (__stdcall *__pfn_glColor3d)(GLdouble, GLdouble, GLdouble);
static __pfn_glColor3d __proc_glColor3d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    ANAX_TRACE("glColor3d");
    if (__proc_glColor3d == nullptr) {
        __proc_glColor3d = (__pfn_glColor3d)GetProcAddress(EnsureRealOpenGL32(), "glColor3d");
        if (__proc_glColor3d == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3d: resolved OK\n");
        }
    }
    __proc_glColor3d(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3dv)(void*);
static __pfn_glColor3dv __proc_glColor3dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3dv(void* v) {
    ANAX_TRACE("glColor3dv");
    if (__proc_glColor3dv == nullptr) {
        __proc_glColor3dv = (__pfn_glColor3dv)GetProcAddress(EnsureRealOpenGL32(), "glColor3dv");
        if (__proc_glColor3dv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3dv: resolved OK\n");
        }
    }
    __proc_glColor3dv(v);
}

typedef void (__stdcall *__pfn_glColor3f)(GLfloat, GLfloat, GLfloat);
static __pfn_glColor3f __proc_glColor3f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    ANAX_TRACE("glColor3f");
    if (__proc_glColor3f == nullptr) {
        __proc_glColor3f = (__pfn_glColor3f)GetProcAddress(EnsureRealOpenGL32(), "glColor3f");
        if (__proc_glColor3f == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3f: resolved OK\n");
        }
    }
    __proc_glColor3f(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3fv)(void*);
static __pfn_glColor3fv __proc_glColor3fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3fv(void* v) {
    ANAX_TRACE("glColor3fv");
    if (__proc_glColor3fv == nullptr) {
        __proc_glColor3fv = (__pfn_glColor3fv)GetProcAddress(EnsureRealOpenGL32(), "glColor3fv");
        if (__proc_glColor3fv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3fv: resolved OK\n");
        }
    }
    __proc_glColor3fv(v);
}

typedef void (__stdcall *__pfn_glColor3i)(GLint, GLint, GLint);
static __pfn_glColor3i __proc_glColor3i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3i(GLint red, GLint green, GLint blue) {
    ANAX_TRACE("glColor3i");
    if (__proc_glColor3i == nullptr) {
        __proc_glColor3i = (__pfn_glColor3i)GetProcAddress(EnsureRealOpenGL32(), "glColor3i");
        if (__proc_glColor3i == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3i: resolved OK\n");
        }
    }
    __proc_glColor3i(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3iv)(void*);
static __pfn_glColor3iv __proc_glColor3iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3iv(void* v) {
    ANAX_TRACE("glColor3iv");
    if (__proc_glColor3iv == nullptr) {
        __proc_glColor3iv = (__pfn_glColor3iv)GetProcAddress(EnsureRealOpenGL32(), "glColor3iv");
        if (__proc_glColor3iv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3iv: resolved OK\n");
        }
    }
    __proc_glColor3iv(v);
}

typedef void (__stdcall *__pfn_glColor3s)(GLshort, GLshort, GLshort);
static __pfn_glColor3s __proc_glColor3s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3s(GLshort red, GLshort green, GLshort blue) {
    ANAX_TRACE("glColor3s");
    if (__proc_glColor3s == nullptr) {
        __proc_glColor3s = (__pfn_glColor3s)GetProcAddress(EnsureRealOpenGL32(), "glColor3s");
        if (__proc_glColor3s == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3s: resolved OK\n");
        }
    }
    __proc_glColor3s(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3sv)(void*);
static __pfn_glColor3sv __proc_glColor3sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3sv(void* v) {
    ANAX_TRACE("glColor3sv");
    if (__proc_glColor3sv == nullptr) {
        __proc_glColor3sv = (__pfn_glColor3sv)GetProcAddress(EnsureRealOpenGL32(), "glColor3sv");
        if (__proc_glColor3sv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3sv: resolved OK\n");
        }
    }
    __proc_glColor3sv(v);
}

typedef void (__stdcall *__pfn_glColor3ub)(GLubyte, GLubyte, GLubyte);
static __pfn_glColor3ub __proc_glColor3ub = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    ANAX_TRACE("glColor3ub");
    if (__proc_glColor3ub == nullptr) {
        __proc_glColor3ub = (__pfn_glColor3ub)GetProcAddress(EnsureRealOpenGL32(), "glColor3ub");
        if (__proc_glColor3ub == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3ub: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3ub: resolved OK\n");
        }
    }
    __proc_glColor3ub(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3ubv)(void*);
static __pfn_glColor3ubv __proc_glColor3ubv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3ubv(void* v) {
    ANAX_TRACE("glColor3ubv");
    if (__proc_glColor3ubv == nullptr) {
        __proc_glColor3ubv = (__pfn_glColor3ubv)GetProcAddress(EnsureRealOpenGL32(), "glColor3ubv");
        if (__proc_glColor3ubv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3ubv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3ubv: resolved OK\n");
        }
    }
    __proc_glColor3ubv(v);
}

typedef void (__stdcall *__pfn_glColor3ui)(GLuint, GLuint, GLuint);
static __pfn_glColor3ui __proc_glColor3ui = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3ui(GLuint red, GLuint green, GLuint blue) {
    ANAX_TRACE("glColor3ui");
    if (__proc_glColor3ui == nullptr) {
        __proc_glColor3ui = (__pfn_glColor3ui)GetProcAddress(EnsureRealOpenGL32(), "glColor3ui");
        if (__proc_glColor3ui == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3ui: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3ui: resolved OK\n");
        }
    }
    __proc_glColor3ui(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3uiv)(void*);
static __pfn_glColor3uiv __proc_glColor3uiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3uiv(void* v) {
    ANAX_TRACE("glColor3uiv");
    if (__proc_glColor3uiv == nullptr) {
        __proc_glColor3uiv = (__pfn_glColor3uiv)GetProcAddress(EnsureRealOpenGL32(), "glColor3uiv");
        if (__proc_glColor3uiv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3uiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3uiv: resolved OK\n");
        }
    }
    __proc_glColor3uiv(v);
}

typedef void (__stdcall *__pfn_glColor3us)(GLushort, GLushort, GLushort);
static __pfn_glColor3us __proc_glColor3us = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3us(GLushort red, GLushort green, GLushort blue) {
    ANAX_TRACE("glColor3us");
    if (__proc_glColor3us == nullptr) {
        __proc_glColor3us = (__pfn_glColor3us)GetProcAddress(EnsureRealOpenGL32(), "glColor3us");
        if (__proc_glColor3us == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3us: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3us: resolved OK\n");
        }
    }
    __proc_glColor3us(red, green, blue);
}

typedef void (__stdcall *__pfn_glColor3usv)(void*);
static __pfn_glColor3usv __proc_glColor3usv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor3usv(void* v) {
    ANAX_TRACE("glColor3usv");
    if (__proc_glColor3usv == nullptr) {
        __proc_glColor3usv = (__pfn_glColor3usv)GetProcAddress(EnsureRealOpenGL32(), "glColor3usv");
        if (__proc_glColor3usv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor3usv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor3usv: resolved OK\n");
        }
    }
    __proc_glColor3usv(v);
}

typedef void (__stdcall *__pfn_glColor4b)(GLbyte, GLbyte, GLbyte, GLbyte);
static __pfn_glColor4b __proc_glColor4b = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) {
    ANAX_TRACE("glColor4b");
    if (__proc_glColor4b == nullptr) {
        __proc_glColor4b = (__pfn_glColor4b)GetProcAddress(EnsureRealOpenGL32(), "glColor4b");
        if (__proc_glColor4b == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4b: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4b: resolved OK\n");
        }
    }
    __proc_glColor4b(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4bv)(void*);
static __pfn_glColor4bv __proc_glColor4bv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4bv(void* v) {
    ANAX_TRACE("glColor4bv");
    if (__proc_glColor4bv == nullptr) {
        __proc_glColor4bv = (__pfn_glColor4bv)GetProcAddress(EnsureRealOpenGL32(), "glColor4bv");
        if (__proc_glColor4bv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4bv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4bv: resolved OK\n");
        }
    }
    __proc_glColor4bv(v);
}

typedef void (__stdcall *__pfn_glColor4d)(GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glColor4d __proc_glColor4d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) {
    ANAX_TRACE("glColor4d");
    if (__proc_glColor4d == nullptr) {
        __proc_glColor4d = (__pfn_glColor4d)GetProcAddress(EnsureRealOpenGL32(), "glColor4d");
        if (__proc_glColor4d == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4d: resolved OK\n");
        }
    }
    __proc_glColor4d(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4dv)(void*);
static __pfn_glColor4dv __proc_glColor4dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4dv(void* v) {
    ANAX_TRACE("glColor4dv");
    if (__proc_glColor4dv == nullptr) {
        __proc_glColor4dv = (__pfn_glColor4dv)GetProcAddress(EnsureRealOpenGL32(), "glColor4dv");
        if (__proc_glColor4dv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4dv: resolved OK\n");
        }
    }
    __proc_glColor4dv(v);
}

typedef void (__stdcall *__pfn_glColor4f)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glColor4f __proc_glColor4f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    ANAX_TRACE("glColor4f");
    if (__proc_glColor4f == nullptr) {
        __proc_glColor4f = (__pfn_glColor4f)GetProcAddress(EnsureRealOpenGL32(), "glColor4f");
        if (__proc_glColor4f == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4f: resolved OK\n");
        }
    }
    __proc_glColor4f(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4fv)(void*);
static __pfn_glColor4fv __proc_glColor4fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4fv(void* v) {
    ANAX_TRACE("glColor4fv");
    if (__proc_glColor4fv == nullptr) {
        __proc_glColor4fv = (__pfn_glColor4fv)GetProcAddress(EnsureRealOpenGL32(), "glColor4fv");
        if (__proc_glColor4fv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4fv: resolved OK\n");
        }
    }
    __proc_glColor4fv(v);
}

typedef void (__stdcall *__pfn_glColor4i)(GLint, GLint, GLint, GLint);
static __pfn_glColor4i __proc_glColor4i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4i(GLint red, GLint green, GLint blue, GLint alpha) {
    ANAX_TRACE("glColor4i");
    if (__proc_glColor4i == nullptr) {
        __proc_glColor4i = (__pfn_glColor4i)GetProcAddress(EnsureRealOpenGL32(), "glColor4i");
        if (__proc_glColor4i == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4i: resolved OK\n");
        }
    }
    __proc_glColor4i(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4iv)(void*);
static __pfn_glColor4iv __proc_glColor4iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4iv(void* v) {
    ANAX_TRACE("glColor4iv");
    if (__proc_glColor4iv == nullptr) {
        __proc_glColor4iv = (__pfn_glColor4iv)GetProcAddress(EnsureRealOpenGL32(), "glColor4iv");
        if (__proc_glColor4iv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4iv: resolved OK\n");
        }
    }
    __proc_glColor4iv(v);
}

typedef void (__stdcall *__pfn_glColor4s)(GLshort, GLshort, GLshort, GLshort);
static __pfn_glColor4s __proc_glColor4s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha) {
    ANAX_TRACE("glColor4s");
    if (__proc_glColor4s == nullptr) {
        __proc_glColor4s = (__pfn_glColor4s)GetProcAddress(EnsureRealOpenGL32(), "glColor4s");
        if (__proc_glColor4s == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4s: resolved OK\n");
        }
    }
    __proc_glColor4s(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4sv)(void*);
static __pfn_glColor4sv __proc_glColor4sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4sv(void* v) {
    ANAX_TRACE("glColor4sv");
    if (__proc_glColor4sv == nullptr) {
        __proc_glColor4sv = (__pfn_glColor4sv)GetProcAddress(EnsureRealOpenGL32(), "glColor4sv");
        if (__proc_glColor4sv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4sv: resolved OK\n");
        }
    }
    __proc_glColor4sv(v);
}

typedef void (__stdcall *__pfn_glColor4ub)(GLubyte, GLubyte, GLubyte, GLubyte);
static __pfn_glColor4ub __proc_glColor4ub = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    ANAX_TRACE("glColor4ub");
    if (__proc_glColor4ub == nullptr) {
        __proc_glColor4ub = (__pfn_glColor4ub)GetProcAddress(EnsureRealOpenGL32(), "glColor4ub");
        if (__proc_glColor4ub == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4ub: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4ub: resolved OK\n");
        }
    }
    __proc_glColor4ub(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4ubv)(void*);
static __pfn_glColor4ubv __proc_glColor4ubv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4ubv(void* v) {
    ANAX_TRACE("glColor4ubv");
    if (__proc_glColor4ubv == nullptr) {
        __proc_glColor4ubv = (__pfn_glColor4ubv)GetProcAddress(EnsureRealOpenGL32(), "glColor4ubv");
        if (__proc_glColor4ubv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4ubv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4ubv: resolved OK\n");
        }
    }
    __proc_glColor4ubv(v);
}

typedef void (__stdcall *__pfn_glColor4ui)(GLuint, GLuint, GLuint, GLuint);
static __pfn_glColor4ui __proc_glColor4ui = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha) {
    ANAX_TRACE("glColor4ui");
    if (__proc_glColor4ui == nullptr) {
        __proc_glColor4ui = (__pfn_glColor4ui)GetProcAddress(EnsureRealOpenGL32(), "glColor4ui");
        if (__proc_glColor4ui == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4ui: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4ui: resolved OK\n");
        }
    }
    __proc_glColor4ui(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4uiv)(void*);
static __pfn_glColor4uiv __proc_glColor4uiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4uiv(void* v) {
    ANAX_TRACE("glColor4uiv");
    if (__proc_glColor4uiv == nullptr) {
        __proc_glColor4uiv = (__pfn_glColor4uiv)GetProcAddress(EnsureRealOpenGL32(), "glColor4uiv");
        if (__proc_glColor4uiv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4uiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4uiv: resolved OK\n");
        }
    }
    __proc_glColor4uiv(v);
}

typedef void (__stdcall *__pfn_glColor4us)(GLushort, GLushort, GLushort, GLushort);
static __pfn_glColor4us __proc_glColor4us = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha) {
    ANAX_TRACE("glColor4us");
    if (__proc_glColor4us == nullptr) {
        __proc_glColor4us = (__pfn_glColor4us)GetProcAddress(EnsureRealOpenGL32(), "glColor4us");
        if (__proc_glColor4us == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4us: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4us: resolved OK\n");
        }
    }
    __proc_glColor4us(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColor4usv)(void*);
static __pfn_glColor4usv __proc_glColor4usv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColor4usv(void* v) {
    ANAX_TRACE("glColor4usv");
    if (__proc_glColor4usv == nullptr) {
        __proc_glColor4usv = (__pfn_glColor4usv)GetProcAddress(EnsureRealOpenGL32(), "glColor4usv");
        if (__proc_glColor4usv == nullptr) {
            printf("[opengl32_enh_cpp]   glColor4usv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColor4usv: resolved OK\n");
        }
    }
    __proc_glColor4usv(v);
}

typedef void (__stdcall *__pfn_glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
static __pfn_glColorMask __proc_glColorMask = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    ANAX_TRACE("glColorMask");
    if (__proc_glColorMask == nullptr) {
        __proc_glColorMask = (__pfn_glColorMask)GetProcAddress(EnsureRealOpenGL32(), "glColorMask");
        if (__proc_glColorMask == nullptr) {
            printf("[opengl32_enh_cpp]   glColorMask: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColorMask: resolved OK\n");
        }
    }
    __proc_glColorMask(red, green, blue, alpha);
}

typedef void (__stdcall *__pfn_glColorMaterial)(GLenum, GLenum);
static __pfn_glColorMaterial __proc_glColorMaterial = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColorMaterial(GLenum face, GLenum mode) {
    ANAX_TRACE("glColorMaterial");
    if (__proc_glColorMaterial == nullptr) {
        __proc_glColorMaterial = (__pfn_glColorMaterial)GetProcAddress(EnsureRealOpenGL32(), "glColorMaterial");
        if (__proc_glColorMaterial == nullptr) {
            printf("[opengl32_enh_cpp]   glColorMaterial: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColorMaterial: resolved OK\n");
        }
    }
    __proc_glColorMaterial(face, mode);
}

typedef void (__stdcall *__pfn_glColorPointer)(GLint, GLenum, GLsizei, void*);
static __pfn_glColorPointer __proc_glColorPointer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glColorPointer(GLint size, GLenum type, GLsizei stride, void* pointer) {
    ANAX_TRACE("glColorPointer");
    if (__proc_glColorPointer == nullptr) {
        __proc_glColorPointer = (__pfn_glColorPointer)GetProcAddress(EnsureRealOpenGL32(), "glColorPointer");
        if (__proc_glColorPointer == nullptr) {
            printf("[opengl32_enh_cpp]   glColorPointer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glColorPointer: resolved OK\n");
        }
    }
    __proc_glColorPointer(size, type, stride, pointer);
}

typedef void (__stdcall *__pfn_glCopyPixels)(GLint, GLint, GLsizei, GLsizei, GLenum);
static __pfn_glCopyPixels __proc_glCopyPixels = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) {
    ANAX_TRACE("glCopyPixels");
    if (__proc_glCopyPixels == nullptr) {
        __proc_glCopyPixels = (__pfn_glCopyPixels)GetProcAddress(EnsureRealOpenGL32(), "glCopyPixels");
        if (__proc_glCopyPixels == nullptr) {
            printf("[opengl32_enh_cpp]   glCopyPixels: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCopyPixels: resolved OK\n");
        }
    }
    __proc_glCopyPixels(x, y, width, height, type);
}

typedef void (__stdcall *__pfn_glCopyTexImage1D)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint);
static __pfn_glCopyTexImage1D __proc_glCopyTexImage1D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border) {
    ANAX_TRACE("glCopyTexImage1D");
    if (__proc_glCopyTexImage1D == nullptr) {
        __proc_glCopyTexImage1D = (__pfn_glCopyTexImage1D)GetProcAddress(EnsureRealOpenGL32(), "glCopyTexImage1D");
        if (__proc_glCopyTexImage1D == nullptr) {
            printf("[opengl32_enh_cpp]   glCopyTexImage1D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCopyTexImage1D: resolved OK\n");
        }
    }
    __proc_glCopyTexImage1D(target, level, internalFormat, x, y, width, border);
}

typedef void (__stdcall *__pfn_glCopyTexImage2D)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
static __pfn_glCopyTexImage2D __proc_glCopyTexImage2D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    ANAX_TRACE("glCopyTexImage2D");
    if (__proc_glCopyTexImage2D == nullptr) {
        __proc_glCopyTexImage2D = (__pfn_glCopyTexImage2D)GetProcAddress(EnsureRealOpenGL32(), "glCopyTexImage2D");
        if (__proc_glCopyTexImage2D == nullptr) {
            printf("[opengl32_enh_cpp]   glCopyTexImage2D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCopyTexImage2D: resolved OK\n");
        }
    }
    __proc_glCopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

typedef void (__stdcall *__pfn_glCopyTexSubImage1D)(GLenum, GLint, GLint, GLint, GLint, GLsizei);
static __pfn_glCopyTexSubImage1D __proc_glCopyTexSubImage1D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    ANAX_TRACE("glCopyTexSubImage1D");
    if (__proc_glCopyTexSubImage1D == nullptr) {
        __proc_glCopyTexSubImage1D = (__pfn_glCopyTexSubImage1D)GetProcAddress(EnsureRealOpenGL32(), "glCopyTexSubImage1D");
        if (__proc_glCopyTexSubImage1D == nullptr) {
            printf("[opengl32_enh_cpp]   glCopyTexSubImage1D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCopyTexSubImage1D: resolved OK\n");
        }
    }
    __proc_glCopyTexSubImage1D(target, level, xoffset, x, y, width);
}

typedef void (__stdcall *__pfn_glCopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
static __pfn_glCopyTexSubImage2D __proc_glCopyTexSubImage2D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    ANAX_TRACE("glCopyTexSubImage2D");
    if (__proc_glCopyTexSubImage2D == nullptr) {
        __proc_glCopyTexSubImage2D = (__pfn_glCopyTexSubImage2D)GetProcAddress(EnsureRealOpenGL32(), "glCopyTexSubImage2D");
        if (__proc_glCopyTexSubImage2D == nullptr) {
            printf("[opengl32_enh_cpp]   glCopyTexSubImage2D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCopyTexSubImage2D: resolved OK\n");
        }
    }
    __proc_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

typedef void (__stdcall *__pfn_glCullFace)(GLenum);
static __pfn_glCullFace __proc_glCullFace = nullptr;

extern "C" __declspec(dllexport) void __stdcall glCullFace(GLenum mode) {
    ANAX_TRACE("glCullFace");
    if (__proc_glCullFace == nullptr) {
        __proc_glCullFace = (__pfn_glCullFace)GetProcAddress(EnsureRealOpenGL32(), "glCullFace");
        if (__proc_glCullFace == nullptr) {
            printf("[opengl32_enh_cpp]   glCullFace: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glCullFace: resolved OK\n");
        }
    }
    __proc_glCullFace(mode);
}

typedef void (__stdcall *__pfn_glDeleteLists)(GLuint, GLsizei);
static __pfn_glDeleteLists __proc_glDeleteLists = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDeleteLists(GLuint list, GLsizei range) {
    ANAX_TRACE("glDeleteLists");
    if (__proc_glDeleteLists == nullptr) {
        __proc_glDeleteLists = (__pfn_glDeleteLists)GetProcAddress(EnsureRealOpenGL32(), "glDeleteLists");
        if (__proc_glDeleteLists == nullptr) {
            printf("[opengl32_enh_cpp]   glDeleteLists: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDeleteLists: resolved OK\n");
        }
    }
    __proc_glDeleteLists(list, range);
}

typedef void (__stdcall *__pfn_glDeleteTextures)(GLsizei, void*);
static __pfn_glDeleteTextures __proc_glDeleteTextures = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDeleteTextures(GLsizei n, void* textures) {
    ANAX_TRACE("glDeleteTextures");
    if (__proc_glDeleteTextures == nullptr) {
        __proc_glDeleteTextures = (__pfn_glDeleteTextures)GetProcAddress(EnsureRealOpenGL32(), "glDeleteTextures");
        if (__proc_glDeleteTextures == nullptr) {
            printf("[opengl32_enh_cpp]   glDeleteTextures: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDeleteTextures: resolved OK\n");
        }
    }
    __proc_glDeleteTextures(n, textures);
}

typedef void (__stdcall *__pfn_glDepthFunc)(GLenum);
static __pfn_glDepthFunc __proc_glDepthFunc = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDepthFunc(GLenum func) {
    ANAX_TRACE("glDepthFunc");
    if (__proc_glDepthFunc == nullptr) {
        __proc_glDepthFunc = (__pfn_glDepthFunc)GetProcAddress(EnsureRealOpenGL32(), "glDepthFunc");
        if (__proc_glDepthFunc == nullptr) {
            printf("[opengl32_enh_cpp]   glDepthFunc: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDepthFunc: resolved OK\n");
        }
    }
    __proc_glDepthFunc(func);
}

typedef void (__stdcall *__pfn_glDepthMask)(GLboolean);
static __pfn_glDepthMask __proc_glDepthMask = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDepthMask(GLboolean flag) {
    ANAX_TRACE("glDepthMask");
    if (__proc_glDepthMask == nullptr) {
        __proc_glDepthMask = (__pfn_glDepthMask)GetProcAddress(EnsureRealOpenGL32(), "glDepthMask");
        if (__proc_glDepthMask == nullptr) {
            printf("[opengl32_enh_cpp]   glDepthMask: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDepthMask: resolved OK\n");
        }
    }
    __proc_glDepthMask(flag);
}

typedef void (__stdcall *__pfn_glDepthRange)(GLclampd, GLclampd);
static __pfn_glDepthRange __proc_glDepthRange = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDepthRange(GLclampd zNear, GLclampd zFar) {
    ANAX_TRACE("glDepthRange");
    if (__proc_glDepthRange == nullptr) {
        __proc_glDepthRange = (__pfn_glDepthRange)GetProcAddress(EnsureRealOpenGL32(), "glDepthRange");
        if (__proc_glDepthRange == nullptr) {
            printf("[opengl32_enh_cpp]   glDepthRange: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDepthRange: resolved OK\n");
        }
    }
    __proc_glDepthRange(zNear, zFar);
}

typedef void (__stdcall *__pfn_glDisable)(GLenum);
static __pfn_glDisable __proc_glDisable = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDisable(GLenum cap) {
    ANAX_TRACE("glDisable");
    if (__proc_glDisable == nullptr) {
        __proc_glDisable = (__pfn_glDisable)GetProcAddress(EnsureRealOpenGL32(), "glDisable");
        if (__proc_glDisable == nullptr) {
            printf("[opengl32_enh_cpp]   glDisable: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDisable: resolved OK\n");
        }
    }
    __proc_glDisable(cap);
}

typedef void (__stdcall *__pfn_glDisableClientState)(GLenum);
static __pfn_glDisableClientState __proc_glDisableClientState = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDisableClientState(GLenum array) {
    ANAX_TRACE("glDisableClientState");
    if (__proc_glDisableClientState == nullptr) {
        __proc_glDisableClientState = (__pfn_glDisableClientState)GetProcAddress(EnsureRealOpenGL32(), "glDisableClientState");
        if (__proc_glDisableClientState == nullptr) {
            printf("[opengl32_enh_cpp]   glDisableClientState: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDisableClientState: resolved OK\n");
        }
    }
    __proc_glDisableClientState(array);
}

typedef void (__stdcall *__pfn_glDrawArrays)(GLenum, GLint, GLsizei);
static __pfn_glDrawArrays __proc_glDrawArrays = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    ANAX_TRACE("glDrawArrays");
    if (__proc_glDrawArrays == nullptr) {
        __proc_glDrawArrays = (__pfn_glDrawArrays)GetProcAddress(EnsureRealOpenGL32(), "glDrawArrays");
        if (__proc_glDrawArrays == nullptr) {
            printf("[opengl32_enh_cpp]   glDrawArrays: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDrawArrays: resolved OK\n");
        }
    }
    __proc_glDrawArrays(mode, first, count);
}

typedef void (__stdcall *__pfn_glDrawBuffer)(GLenum);
static __pfn_glDrawBuffer __proc_glDrawBuffer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDrawBuffer(GLenum mode) {
    ANAX_TRACE("glDrawBuffer");
    if (__proc_glDrawBuffer == nullptr) {
        __proc_glDrawBuffer = (__pfn_glDrawBuffer)GetProcAddress(EnsureRealOpenGL32(), "glDrawBuffer");
        if (__proc_glDrawBuffer == nullptr) {
            printf("[opengl32_enh_cpp]   glDrawBuffer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDrawBuffer: resolved OK\n");
        }
    }
    __proc_glDrawBuffer(mode);
}

typedef void (__stdcall *__pfn_glDrawElements)(GLenum, GLsizei, GLenum, void*);
static __pfn_glDrawElements __proc_glDrawElements = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDrawElements(GLenum mode, GLsizei count, GLenum type, void* indices) {
    ANAX_TRACE("glDrawElements");
    if (__proc_glDrawElements == nullptr) {
        __proc_glDrawElements = (__pfn_glDrawElements)GetProcAddress(EnsureRealOpenGL32(), "glDrawElements");
        if (__proc_glDrawElements == nullptr) {
            printf("[opengl32_enh_cpp]   glDrawElements: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDrawElements: resolved OK\n");
        }
    }
    __proc_glDrawElements(mode, count, type, indices);
}

typedef void (__stdcall *__pfn_glDrawPixels)(GLsizei, GLsizei, GLenum, GLenum, void*);
static __pfn_glDrawPixels __proc_glDrawPixels = nullptr;

extern "C" __declspec(dllexport) void __stdcall glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glDrawPixels");
    if (__proc_glDrawPixels == nullptr) {
        __proc_glDrawPixels = (__pfn_glDrawPixels)GetProcAddress(EnsureRealOpenGL32(), "glDrawPixels");
        if (__proc_glDrawPixels == nullptr) {
            printf("[opengl32_enh_cpp]   glDrawPixels: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glDrawPixels: resolved OK\n");
        }
    }
    __proc_glDrawPixels(width, height, format, type, pixels);
}

typedef void (__stdcall *__pfn_glEdgeFlag)(GLboolean);
static __pfn_glEdgeFlag __proc_glEdgeFlag = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEdgeFlag(GLboolean flag) {
    ANAX_TRACE("glEdgeFlag");
    if (__proc_glEdgeFlag == nullptr) {
        __proc_glEdgeFlag = (__pfn_glEdgeFlag)GetProcAddress(EnsureRealOpenGL32(), "glEdgeFlag");
        if (__proc_glEdgeFlag == nullptr) {
            printf("[opengl32_enh_cpp]   glEdgeFlag: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEdgeFlag: resolved OK\n");
        }
    }
    __proc_glEdgeFlag(flag);
}

typedef void (__stdcall *__pfn_glEdgeFlagPointer)(GLsizei, void*);
static __pfn_glEdgeFlagPointer __proc_glEdgeFlagPointer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEdgeFlagPointer(GLsizei stride, void* pointer) {
    ANAX_TRACE("glEdgeFlagPointer");
    if (__proc_glEdgeFlagPointer == nullptr) {
        __proc_glEdgeFlagPointer = (__pfn_glEdgeFlagPointer)GetProcAddress(EnsureRealOpenGL32(), "glEdgeFlagPointer");
        if (__proc_glEdgeFlagPointer == nullptr) {
            printf("[opengl32_enh_cpp]   glEdgeFlagPointer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEdgeFlagPointer: resolved OK\n");
        }
    }
    __proc_glEdgeFlagPointer(stride, pointer);
}

typedef void (__stdcall *__pfn_glEdgeFlagv)(void*);
static __pfn_glEdgeFlagv __proc_glEdgeFlagv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEdgeFlagv(void* flag) {
    ANAX_TRACE("glEdgeFlagv");
    if (__proc_glEdgeFlagv == nullptr) {
        __proc_glEdgeFlagv = (__pfn_glEdgeFlagv)GetProcAddress(EnsureRealOpenGL32(), "glEdgeFlagv");
        if (__proc_glEdgeFlagv == nullptr) {
            printf("[opengl32_enh_cpp]   glEdgeFlagv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEdgeFlagv: resolved OK\n");
        }
    }
    __proc_glEdgeFlagv(flag);
}

typedef void (__stdcall *__pfn_glEnable)(GLenum);
static __pfn_glEnable __proc_glEnable = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEnable(GLenum cap) {
    ANAX_TRACE("glEnable");
    if (__proc_glEnable == nullptr) {
        __proc_glEnable = (__pfn_glEnable)GetProcAddress(EnsureRealOpenGL32(), "glEnable");
        if (__proc_glEnable == nullptr) {
            printf("[opengl32_enh_cpp]   glEnable: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEnable: resolved OK\n");
        }
    }
    __proc_glEnable(cap);
}

typedef void (__stdcall *__pfn_glEnableClientState)(GLenum);
static __pfn_glEnableClientState __proc_glEnableClientState = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEnableClientState(GLenum array) {
    ANAX_TRACE("glEnableClientState");
    if (__proc_glEnableClientState == nullptr) {
        __proc_glEnableClientState = (__pfn_glEnableClientState)GetProcAddress(EnsureRealOpenGL32(), "glEnableClientState");
        if (__proc_glEnableClientState == nullptr) {
            printf("[opengl32_enh_cpp]   glEnableClientState: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEnableClientState: resolved OK\n");
        }
    }
    __proc_glEnableClientState(array);
}

typedef void (__stdcall *__pfn_glEnd)(void);
static __pfn_glEnd __proc_glEnd = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEnd(void) {
    ANAX_TRACE("glEnd");
    if (__proc_glEnd == nullptr) {
        __proc_glEnd = (__pfn_glEnd)GetProcAddress(EnsureRealOpenGL32(), "glEnd");
        if (__proc_glEnd == nullptr) {
            printf("[opengl32_enh_cpp]   glEnd: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEnd: resolved OK\n");
        }
    }
    __proc_glEnd();
}

typedef void (__stdcall *__pfn_glEndList)(void);
static __pfn_glEndList __proc_glEndList = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEndList(void) {
    ANAX_TRACE("glEndList");
    if (__proc_glEndList == nullptr) {
        __proc_glEndList = (__pfn_glEndList)GetProcAddress(EnsureRealOpenGL32(), "glEndList");
        if (__proc_glEndList == nullptr) {
            printf("[opengl32_enh_cpp]   glEndList: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEndList: resolved OK\n");
        }
    }
    __proc_glEndList();
}

typedef void (__stdcall *__pfn_glEvalCoord1d)(GLdouble);
static __pfn_glEvalCoord1d __proc_glEvalCoord1d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord1d(GLdouble u) {
    ANAX_TRACE("glEvalCoord1d");
    if (__proc_glEvalCoord1d == nullptr) {
        __proc_glEvalCoord1d = (__pfn_glEvalCoord1d)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord1d");
        if (__proc_glEvalCoord1d == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord1d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord1d: resolved OK\n");
        }
    }
    __proc_glEvalCoord1d(u);
}

typedef void (__stdcall *__pfn_glEvalCoord1dv)(void*);
static __pfn_glEvalCoord1dv __proc_glEvalCoord1dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord1dv(void* u) {
    ANAX_TRACE("glEvalCoord1dv");
    if (__proc_glEvalCoord1dv == nullptr) {
        __proc_glEvalCoord1dv = (__pfn_glEvalCoord1dv)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord1dv");
        if (__proc_glEvalCoord1dv == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord1dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord1dv: resolved OK\n");
        }
    }
    __proc_glEvalCoord1dv(u);
}

typedef void (__stdcall *__pfn_glEvalCoord1f)(GLfloat);
static __pfn_glEvalCoord1f __proc_glEvalCoord1f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord1f(GLfloat u) {
    ANAX_TRACE("glEvalCoord1f");
    if (__proc_glEvalCoord1f == nullptr) {
        __proc_glEvalCoord1f = (__pfn_glEvalCoord1f)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord1f");
        if (__proc_glEvalCoord1f == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord1f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord1f: resolved OK\n");
        }
    }
    __proc_glEvalCoord1f(u);
}

typedef void (__stdcall *__pfn_glEvalCoord1fv)(void*);
static __pfn_glEvalCoord1fv __proc_glEvalCoord1fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord1fv(void* u) {
    ANAX_TRACE("glEvalCoord1fv");
    if (__proc_glEvalCoord1fv == nullptr) {
        __proc_glEvalCoord1fv = (__pfn_glEvalCoord1fv)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord1fv");
        if (__proc_glEvalCoord1fv == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord1fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord1fv: resolved OK\n");
        }
    }
    __proc_glEvalCoord1fv(u);
}

typedef void (__stdcall *__pfn_glEvalCoord2d)(GLdouble, GLdouble);
static __pfn_glEvalCoord2d __proc_glEvalCoord2d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord2d(GLdouble u, GLdouble v) {
    ANAX_TRACE("glEvalCoord2d");
    if (__proc_glEvalCoord2d == nullptr) {
        __proc_glEvalCoord2d = (__pfn_glEvalCoord2d)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord2d");
        if (__proc_glEvalCoord2d == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord2d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord2d: resolved OK\n");
        }
    }
    __proc_glEvalCoord2d(u, v);
}

typedef void (__stdcall *__pfn_glEvalCoord2dv)(void*);
static __pfn_glEvalCoord2dv __proc_glEvalCoord2dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord2dv(void* u) {
    ANAX_TRACE("glEvalCoord2dv");
    if (__proc_glEvalCoord2dv == nullptr) {
        __proc_glEvalCoord2dv = (__pfn_glEvalCoord2dv)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord2dv");
        if (__proc_glEvalCoord2dv == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord2dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord2dv: resolved OK\n");
        }
    }
    __proc_glEvalCoord2dv(u);
}

typedef void (__stdcall *__pfn_glEvalCoord2f)(GLfloat, GLfloat);
static __pfn_glEvalCoord2f __proc_glEvalCoord2f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord2f(GLfloat u, GLfloat v) {
    ANAX_TRACE("glEvalCoord2f");
    if (__proc_glEvalCoord2f == nullptr) {
        __proc_glEvalCoord2f = (__pfn_glEvalCoord2f)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord2f");
        if (__proc_glEvalCoord2f == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord2f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord2f: resolved OK\n");
        }
    }
    __proc_glEvalCoord2f(u, v);
}

typedef void (__stdcall *__pfn_glEvalCoord2fv)(void*);
static __pfn_glEvalCoord2fv __proc_glEvalCoord2fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalCoord2fv(void* u) {
    ANAX_TRACE("glEvalCoord2fv");
    if (__proc_glEvalCoord2fv == nullptr) {
        __proc_glEvalCoord2fv = (__pfn_glEvalCoord2fv)GetProcAddress(EnsureRealOpenGL32(), "glEvalCoord2fv");
        if (__proc_glEvalCoord2fv == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalCoord2fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalCoord2fv: resolved OK\n");
        }
    }
    __proc_glEvalCoord2fv(u);
}

typedef void (__stdcall *__pfn_glEvalMesh1)(GLenum, GLint, GLint);
static __pfn_glEvalMesh1 __proc_glEvalMesh1 = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalMesh1(GLenum mode, GLint i1, GLint i2) {
    ANAX_TRACE("glEvalMesh1");
    if (__proc_glEvalMesh1 == nullptr) {
        __proc_glEvalMesh1 = (__pfn_glEvalMesh1)GetProcAddress(EnsureRealOpenGL32(), "glEvalMesh1");
        if (__proc_glEvalMesh1 == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalMesh1: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalMesh1: resolved OK\n");
        }
    }
    __proc_glEvalMesh1(mode, i1, i2);
}

typedef void (__stdcall *__pfn_glEvalMesh2)(GLenum, GLint, GLint, GLint, GLint);
static __pfn_glEvalMesh2 __proc_glEvalMesh2 = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {
    ANAX_TRACE("glEvalMesh2");
    if (__proc_glEvalMesh2 == nullptr) {
        __proc_glEvalMesh2 = (__pfn_glEvalMesh2)GetProcAddress(EnsureRealOpenGL32(), "glEvalMesh2");
        if (__proc_glEvalMesh2 == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalMesh2: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalMesh2: resolved OK\n");
        }
    }
    __proc_glEvalMesh2(mode, i1, i2, j1, j2);
}

typedef void (__stdcall *__pfn_glEvalPoint1)(GLint);
static __pfn_glEvalPoint1 __proc_glEvalPoint1 = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalPoint1(GLint i) {
    ANAX_TRACE("glEvalPoint1");
    if (__proc_glEvalPoint1 == nullptr) {
        __proc_glEvalPoint1 = (__pfn_glEvalPoint1)GetProcAddress(EnsureRealOpenGL32(), "glEvalPoint1");
        if (__proc_glEvalPoint1 == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalPoint1: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalPoint1: resolved OK\n");
        }
    }
    __proc_glEvalPoint1(i);
}

typedef void (__stdcall *__pfn_glEvalPoint2)(GLint, GLint);
static __pfn_glEvalPoint2 __proc_glEvalPoint2 = nullptr;

extern "C" __declspec(dllexport) void __stdcall glEvalPoint2(GLint i, GLint j) {
    ANAX_TRACE("glEvalPoint2");
    if (__proc_glEvalPoint2 == nullptr) {
        __proc_glEvalPoint2 = (__pfn_glEvalPoint2)GetProcAddress(EnsureRealOpenGL32(), "glEvalPoint2");
        if (__proc_glEvalPoint2 == nullptr) {
            printf("[opengl32_enh_cpp]   glEvalPoint2: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glEvalPoint2: resolved OK\n");
        }
    }
    __proc_glEvalPoint2(i, j);
}

typedef void (__stdcall *__pfn_glFeedbackBuffer)(GLsizei, GLenum, void*);
static __pfn_glFeedbackBuffer __proc_glFeedbackBuffer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFeedbackBuffer(GLsizei size, GLenum type, void* buffer) {
    ANAX_TRACE("glFeedbackBuffer");
    if (__proc_glFeedbackBuffer == nullptr) {
        __proc_glFeedbackBuffer = (__pfn_glFeedbackBuffer)GetProcAddress(EnsureRealOpenGL32(), "glFeedbackBuffer");
        if (__proc_glFeedbackBuffer == nullptr) {
            printf("[opengl32_enh_cpp]   glFeedbackBuffer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFeedbackBuffer: resolved OK\n");
        }
    }
    __proc_glFeedbackBuffer(size, type, buffer);
}

typedef void (__stdcall *__pfn_glFinish)(void);
static __pfn_glFinish __proc_glFinish = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFinish(void) {
    ANAX_TRACE("glFinish");
    if (__proc_glFinish == nullptr) {
        __proc_glFinish = (__pfn_glFinish)GetProcAddress(EnsureRealOpenGL32(), "glFinish");
        if (__proc_glFinish == nullptr) {
            printf("[opengl32_enh_cpp]   glFinish: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFinish: resolved OK\n");
        }
    }
    __proc_glFinish();
}

typedef void (__stdcall *__pfn_glFlush)(void);
static __pfn_glFlush __proc_glFlush = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFlush(void) {
    ANAX_TRACE("glFlush");
    if (__proc_glFlush == nullptr) {
        __proc_glFlush = (__pfn_glFlush)GetProcAddress(EnsureRealOpenGL32(), "glFlush");
        if (__proc_glFlush == nullptr) {
            printf("[opengl32_enh_cpp]   glFlush: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFlush: resolved OK\n");
        }
    }
    __proc_glFlush();
}

typedef void (__stdcall *__pfn_glFogf)(GLenum, GLfloat);
static __pfn_glFogf __proc_glFogf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFogf(GLenum pname, GLfloat param) {
    ANAX_TRACE("glFogf");
    if (__proc_glFogf == nullptr) {
        __proc_glFogf = (__pfn_glFogf)GetProcAddress(EnsureRealOpenGL32(), "glFogf");
        if (__proc_glFogf == nullptr) {
            printf("[opengl32_enh_cpp]   glFogf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFogf: resolved OK\n");
        }
    }
    __proc_glFogf(pname, param);
}

typedef void (__stdcall *__pfn_glFogfv)(GLenum, void*);
static __pfn_glFogfv __proc_glFogfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFogfv(GLenum pname, void* params) {
    ANAX_TRACE("glFogfv");
    if (__proc_glFogfv == nullptr) {
        __proc_glFogfv = (__pfn_glFogfv)GetProcAddress(EnsureRealOpenGL32(), "glFogfv");
        if (__proc_glFogfv == nullptr) {
            printf("[opengl32_enh_cpp]   glFogfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFogfv: resolved OK\n");
        }
    }
    __proc_glFogfv(pname, params);
}

typedef void (__stdcall *__pfn_glFogi)(GLenum, GLint);
static __pfn_glFogi __proc_glFogi = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFogi(GLenum pname, GLint param) {
    ANAX_TRACE("glFogi");
    if (__proc_glFogi == nullptr) {
        __proc_glFogi = (__pfn_glFogi)GetProcAddress(EnsureRealOpenGL32(), "glFogi");
        if (__proc_glFogi == nullptr) {
            printf("[opengl32_enh_cpp]   glFogi: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFogi: resolved OK\n");
        }
    }
    __proc_glFogi(pname, param);
}

typedef void (__stdcall *__pfn_glFogiv)(GLenum, void*);
static __pfn_glFogiv __proc_glFogiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFogiv(GLenum pname, void* params) {
    ANAX_TRACE("glFogiv");
    if (__proc_glFogiv == nullptr) {
        __proc_glFogiv = (__pfn_glFogiv)GetProcAddress(EnsureRealOpenGL32(), "glFogiv");
        if (__proc_glFogiv == nullptr) {
            printf("[opengl32_enh_cpp]   glFogiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFogiv: resolved OK\n");
        }
    }
    __proc_glFogiv(pname, params);
}

typedef void (__stdcall *__pfn_glFrontFace)(GLenum);
static __pfn_glFrontFace __proc_glFrontFace = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFrontFace(GLenum mode) {
    ANAX_TRACE("glFrontFace");
    if (__proc_glFrontFace == nullptr) {
        __proc_glFrontFace = (__pfn_glFrontFace)GetProcAddress(EnsureRealOpenGL32(), "glFrontFace");
        if (__proc_glFrontFace == nullptr) {
            printf("[opengl32_enh_cpp]   glFrontFace: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFrontFace: resolved OK\n");
        }
    }
    __proc_glFrontFace(mode);
}

typedef void (__stdcall *__pfn_glFrustum)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glFrustum __proc_glFrustum = nullptr;

extern "C" __declspec(dllexport) void __stdcall glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    ANAX_TRACE("glFrustum");
    if (__proc_glFrustum == nullptr) {
        __proc_glFrustum = (__pfn_glFrustum)GetProcAddress(EnsureRealOpenGL32(), "glFrustum");
        if (__proc_glFrustum == nullptr) {
            printf("[opengl32_enh_cpp]   glFrustum: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glFrustum: resolved OK\n");
        }
    }
    ApplyTaaJitterToFrustumFromCurrentViewport(left, right, bottom, top);
    CaptureProjectionFrustum(left, right, bottom, top, zNear, zFar);
    NotifyWorldProjection();
    NotifyWorldPassBegan();
    __proc_glFrustum(left, right, bottom, top, zNear, zFar);
}

typedef GLuint (__stdcall *__pfn_glGenLists)(GLsizei);
static __pfn_glGenLists __proc_glGenLists = nullptr;

extern "C" __declspec(dllexport) GLuint __stdcall glGenLists(GLsizei range) {
    ANAX_TRACE("glGenLists");
    if (__proc_glGenLists == nullptr) {
        __proc_glGenLists = (__pfn_glGenLists)GetProcAddress(EnsureRealOpenGL32(), "glGenLists");
        if (__proc_glGenLists == nullptr) {
            printf("[opengl32_enh_cpp]   glGenLists: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGenLists: resolved OK\n");
        }
    }
    return __proc_glGenLists(range);
}

typedef void (__stdcall *__pfn_glGenTextures)(GLsizei, void*);
static __pfn_glGenTextures __proc_glGenTextures = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGenTextures(GLsizei n, void* textures) {
    ANAX_TRACE("glGenTextures");
    if (__proc_glGenTextures == nullptr) {
        __proc_glGenTextures = (__pfn_glGenTextures)GetProcAddress(EnsureRealOpenGL32(), "glGenTextures");
        if (__proc_glGenTextures == nullptr) {
            printf("[opengl32_enh_cpp]   glGenTextures: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGenTextures: resolved OK\n");
        }
    }
    __proc_glGenTextures(n, textures);
}

typedef void (__stdcall *__pfn_glGetBooleanv)(GLenum, void*);
static __pfn_glGetBooleanv __proc_glGetBooleanv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetBooleanv(GLenum pname, void* params) {
    ANAX_TRACE("glGetBooleanv");
    if (__proc_glGetBooleanv == nullptr) {
        __proc_glGetBooleanv = (__pfn_glGetBooleanv)GetProcAddress(EnsureRealOpenGL32(), "glGetBooleanv");
        if (__proc_glGetBooleanv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetBooleanv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetBooleanv: resolved OK\n");
        }
    }
    __proc_glGetBooleanv(pname, params);
}

typedef void (__stdcall *__pfn_glGetClipPlane)(GLenum, void*);
static __pfn_glGetClipPlane __proc_glGetClipPlane = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetClipPlane(GLenum plane, void* equation) {
    ANAX_TRACE("glGetClipPlane");
    if (__proc_glGetClipPlane == nullptr) {
        __proc_glGetClipPlane = (__pfn_glGetClipPlane)GetProcAddress(EnsureRealOpenGL32(), "glGetClipPlane");
        if (__proc_glGetClipPlane == nullptr) {
            printf("[opengl32_enh_cpp]   glGetClipPlane: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetClipPlane: resolved OK\n");
        }
    }
    __proc_glGetClipPlane(plane, equation);
}

typedef void (__stdcall *__pfn_glGetDoublev)(GLenum, void*);
static __pfn_glGetDoublev __proc_glGetDoublev = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetDoublev(GLenum pname, void* params) {
    ANAX_TRACE("glGetDoublev");
    if (__proc_glGetDoublev == nullptr) {
        __proc_glGetDoublev = (__pfn_glGetDoublev)GetProcAddress(EnsureRealOpenGL32(), "glGetDoublev");
        if (__proc_glGetDoublev == nullptr) {
            printf("[opengl32_enh_cpp]   glGetDoublev: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetDoublev: resolved OK\n");
        }
    }
    __proc_glGetDoublev(pname, params);
}

typedef GLenum (__stdcall *__pfn_glGetError)(void);
static __pfn_glGetError __proc_glGetError = nullptr;

extern "C" __declspec(dllexport) GLenum __stdcall glGetError(void) {
    ANAX_TRACE("glGetError");
    if (__proc_glGetError == nullptr) {
        __proc_glGetError = (__pfn_glGetError)GetProcAddress(EnsureRealOpenGL32(), "glGetError");
        if (__proc_glGetError == nullptr) {
            printf("[opengl32_enh_cpp]   glGetError: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetError: resolved OK\n");
        }
    }
    return __proc_glGetError();
}

typedef void (__stdcall *__pfn_glGetFloatv)(GLenum, void*);
static __pfn_glGetFloatv __proc_glGetFloatv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetFloatv(GLenum pname, void* params) {
    ANAX_TRACE("glGetFloatv");
    if (__proc_glGetFloatv == nullptr) {
        __proc_glGetFloatv = (__pfn_glGetFloatv)GetProcAddress(EnsureRealOpenGL32(), "glGetFloatv");
        if (__proc_glGetFloatv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetFloatv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetFloatv: resolved OK\n");
        }
    }
    __proc_glGetFloatv(pname, params);
}

typedef void (__stdcall *__pfn_glGetIntegerv)(GLenum, void*);
static __pfn_glGetIntegerv __proc_glGetIntegerv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetIntegerv(GLenum pname, void* params) {
    ANAX_TRACE("glGetIntegerv");
    if (__proc_glGetIntegerv == nullptr) {
        __proc_glGetIntegerv = (__pfn_glGetIntegerv)GetProcAddress(EnsureRealOpenGL32(), "glGetIntegerv");
        if (__proc_glGetIntegerv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetIntegerv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetIntegerv: resolved OK\n");
        }
    }
    __proc_glGetIntegerv(pname, params);
}

typedef void (__stdcall *__pfn_glGetLightfv)(GLenum, GLenum, void*);
static __pfn_glGetLightfv __proc_glGetLightfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetLightfv(GLenum light, GLenum pname, void* params) {
    ANAX_TRACE("glGetLightfv");
    if (__proc_glGetLightfv == nullptr) {
        __proc_glGetLightfv = (__pfn_glGetLightfv)GetProcAddress(EnsureRealOpenGL32(), "glGetLightfv");
        if (__proc_glGetLightfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetLightfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetLightfv: resolved OK\n");
        }
    }
    __proc_glGetLightfv(light, pname, params);
}

typedef void (__stdcall *__pfn_glGetLightiv)(GLenum, GLenum, void*);
static __pfn_glGetLightiv __proc_glGetLightiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetLightiv(GLenum light, GLenum pname, void* params) {
    ANAX_TRACE("glGetLightiv");
    if (__proc_glGetLightiv == nullptr) {
        __proc_glGetLightiv = (__pfn_glGetLightiv)GetProcAddress(EnsureRealOpenGL32(), "glGetLightiv");
        if (__proc_glGetLightiv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetLightiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetLightiv: resolved OK\n");
        }
    }
    __proc_glGetLightiv(light, pname, params);
}

typedef void (__stdcall *__pfn_glGetMapdv)(GLenum, GLenum, void*);
static __pfn_glGetMapdv __proc_glGetMapdv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetMapdv(GLenum target, GLenum query, void* v) {
    ANAX_TRACE("glGetMapdv");
    if (__proc_glGetMapdv == nullptr) {
        __proc_glGetMapdv = (__pfn_glGetMapdv)GetProcAddress(EnsureRealOpenGL32(), "glGetMapdv");
        if (__proc_glGetMapdv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetMapdv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetMapdv: resolved OK\n");
        }
    }
    __proc_glGetMapdv(target, query, v);
}

typedef void (__stdcall *__pfn_glGetMapfv)(GLenum, GLenum, void*);
static __pfn_glGetMapfv __proc_glGetMapfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetMapfv(GLenum target, GLenum query, void* v) {
    ANAX_TRACE("glGetMapfv");
    if (__proc_glGetMapfv == nullptr) {
        __proc_glGetMapfv = (__pfn_glGetMapfv)GetProcAddress(EnsureRealOpenGL32(), "glGetMapfv");
        if (__proc_glGetMapfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetMapfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetMapfv: resolved OK\n");
        }
    }
    __proc_glGetMapfv(target, query, v);
}

typedef void (__stdcall *__pfn_glGetMapiv)(GLenum, GLenum, void*);
static __pfn_glGetMapiv __proc_glGetMapiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetMapiv(GLenum target, GLenum query, void* v) {
    ANAX_TRACE("glGetMapiv");
    if (__proc_glGetMapiv == nullptr) {
        __proc_glGetMapiv = (__pfn_glGetMapiv)GetProcAddress(EnsureRealOpenGL32(), "glGetMapiv");
        if (__proc_glGetMapiv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetMapiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetMapiv: resolved OK\n");
        }
    }
    __proc_glGetMapiv(target, query, v);
}

typedef void (__stdcall *__pfn_glGetMaterialfv)(GLenum, GLenum, void*);
static __pfn_glGetMaterialfv __proc_glGetMaterialfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetMaterialfv(GLenum face, GLenum pname, void* params) {
    ANAX_TRACE("glGetMaterialfv");
    if (__proc_glGetMaterialfv == nullptr) {
        __proc_glGetMaterialfv = (__pfn_glGetMaterialfv)GetProcAddress(EnsureRealOpenGL32(), "glGetMaterialfv");
        if (__proc_glGetMaterialfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetMaterialfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetMaterialfv: resolved OK\n");
        }
    }
    __proc_glGetMaterialfv(face, pname, params);
}

typedef void (__stdcall *__pfn_glGetMaterialiv)(GLenum, GLenum, void*);
static __pfn_glGetMaterialiv __proc_glGetMaterialiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetMaterialiv(GLenum face, GLenum pname, void* params) {
    ANAX_TRACE("glGetMaterialiv");
    if (__proc_glGetMaterialiv == nullptr) {
        __proc_glGetMaterialiv = (__pfn_glGetMaterialiv)GetProcAddress(EnsureRealOpenGL32(), "glGetMaterialiv");
        if (__proc_glGetMaterialiv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetMaterialiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetMaterialiv: resolved OK\n");
        }
    }
    __proc_glGetMaterialiv(face, pname, params);
}

typedef void (__stdcall *__pfn_glGetPixelMapfv)(GLenum, void*);
static __pfn_glGetPixelMapfv __proc_glGetPixelMapfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetPixelMapfv(GLenum map, void* values) {
    ANAX_TRACE("glGetPixelMapfv");
    if (__proc_glGetPixelMapfv == nullptr) {
        __proc_glGetPixelMapfv = (__pfn_glGetPixelMapfv)GetProcAddress(EnsureRealOpenGL32(), "glGetPixelMapfv");
        if (__proc_glGetPixelMapfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetPixelMapfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetPixelMapfv: resolved OK\n");
        }
    }
    __proc_glGetPixelMapfv(map, values);
}

typedef void (__stdcall *__pfn_glGetPixelMapuiv)(GLenum, void*);
static __pfn_glGetPixelMapuiv __proc_glGetPixelMapuiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetPixelMapuiv(GLenum map, void* values) {
    ANAX_TRACE("glGetPixelMapuiv");
    if (__proc_glGetPixelMapuiv == nullptr) {
        __proc_glGetPixelMapuiv = (__pfn_glGetPixelMapuiv)GetProcAddress(EnsureRealOpenGL32(), "glGetPixelMapuiv");
        if (__proc_glGetPixelMapuiv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetPixelMapuiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetPixelMapuiv: resolved OK\n");
        }
    }
    __proc_glGetPixelMapuiv(map, values);
}

typedef void (__stdcall *__pfn_glGetPixelMapusv)(GLenum, void*);
static __pfn_glGetPixelMapusv __proc_glGetPixelMapusv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetPixelMapusv(GLenum map, void* values) {
    ANAX_TRACE("glGetPixelMapusv");
    if (__proc_glGetPixelMapusv == nullptr) {
        __proc_glGetPixelMapusv = (__pfn_glGetPixelMapusv)GetProcAddress(EnsureRealOpenGL32(), "glGetPixelMapusv");
        if (__proc_glGetPixelMapusv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetPixelMapusv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetPixelMapusv: resolved OK\n");
        }
    }
    __proc_glGetPixelMapusv(map, values);
}

typedef void (__stdcall *__pfn_glGetPointerv)(GLenum, void*);
static __pfn_glGetPointerv __proc_glGetPointerv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetPointerv(GLenum pname, void* params) {
    ANAX_TRACE("glGetPointerv");
    if (__proc_glGetPointerv == nullptr) {
        __proc_glGetPointerv = (__pfn_glGetPointerv)GetProcAddress(EnsureRealOpenGL32(), "glGetPointerv");
        if (__proc_glGetPointerv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetPointerv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetPointerv: resolved OK\n");
        }
    }
    __proc_glGetPointerv(pname, params);
}

typedef void (__stdcall *__pfn_glGetPolygonStipple)(void*);
static __pfn_glGetPolygonStipple __proc_glGetPolygonStipple = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetPolygonStipple(void* mask) {
    ANAX_TRACE("glGetPolygonStipple");
    if (__proc_glGetPolygonStipple == nullptr) {
        __proc_glGetPolygonStipple = (__pfn_glGetPolygonStipple)GetProcAddress(EnsureRealOpenGL32(), "glGetPolygonStipple");
        if (__proc_glGetPolygonStipple == nullptr) {
            printf("[opengl32_enh_cpp]   glGetPolygonStipple: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetPolygonStipple: resolved OK\n");
        }
    }
    __proc_glGetPolygonStipple(mask);
}

typedef void* (__stdcall *__pfn_glGetString)(GLenum);
static __pfn_glGetString __proc_glGetString = nullptr;

extern "C" __declspec(dllexport) void* __stdcall glGetString(GLenum name) {
    ANAX_TRACE("glGetString");
    if (__proc_glGetString == nullptr) {
        __proc_glGetString = (__pfn_glGetString)GetProcAddress(EnsureRealOpenGL32(), "glGetString");
        if (__proc_glGetString == nullptr) {
            printf("[opengl32_enh_cpp]   glGetString: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetString: resolved OK\n");
        }
    }
    return __proc_glGetString(name);
}

typedef void (__stdcall *__pfn_glGetTexEnvfv)(GLenum, GLenum, void*);
static __pfn_glGetTexEnvfv __proc_glGetTexEnvfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexEnvfv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexEnvfv");
    if (__proc_glGetTexEnvfv == nullptr) {
        __proc_glGetTexEnvfv = (__pfn_glGetTexEnvfv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexEnvfv");
        if (__proc_glGetTexEnvfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexEnvfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexEnvfv: resolved OK\n");
        }
    }
    __proc_glGetTexEnvfv(target, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexEnviv)(GLenum, GLenum, void*);
static __pfn_glGetTexEnviv __proc_glGetTexEnviv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexEnviv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexEnviv");
    if (__proc_glGetTexEnviv == nullptr) {
        __proc_glGetTexEnviv = (__pfn_glGetTexEnviv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexEnviv");
        if (__proc_glGetTexEnviv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexEnviv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexEnviv: resolved OK\n");
        }
    }
    __proc_glGetTexEnviv(target, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexGendv)(GLenum, GLenum, void*);
static __pfn_glGetTexGendv __proc_glGetTexGendv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexGendv(GLenum coord, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexGendv");
    if (__proc_glGetTexGendv == nullptr) {
        __proc_glGetTexGendv = (__pfn_glGetTexGendv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexGendv");
        if (__proc_glGetTexGendv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexGendv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexGendv: resolved OK\n");
        }
    }
    __proc_glGetTexGendv(coord, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexGenfv)(GLenum, GLenum, void*);
static __pfn_glGetTexGenfv __proc_glGetTexGenfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexGenfv(GLenum coord, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexGenfv");
    if (__proc_glGetTexGenfv == nullptr) {
        __proc_glGetTexGenfv = (__pfn_glGetTexGenfv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexGenfv");
        if (__proc_glGetTexGenfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexGenfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexGenfv: resolved OK\n");
        }
    }
    __proc_glGetTexGenfv(coord, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexGeniv)(GLenum, GLenum, void*);
static __pfn_glGetTexGeniv __proc_glGetTexGeniv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexGeniv(GLenum coord, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexGeniv");
    if (__proc_glGetTexGeniv == nullptr) {
        __proc_glGetTexGeniv = (__pfn_glGetTexGeniv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexGeniv");
        if (__proc_glGetTexGeniv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexGeniv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexGeniv: resolved OK\n");
        }
    }
    __proc_glGetTexGeniv(coord, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexImage)(GLenum, GLint, GLenum, GLenum, void*);
static __pfn_glGetTexImage __proc_glGetTexImage = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glGetTexImage");
    if (__proc_glGetTexImage == nullptr) {
        __proc_glGetTexImage = (__pfn_glGetTexImage)GetProcAddress(EnsureRealOpenGL32(), "glGetTexImage");
        if (__proc_glGetTexImage == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexImage: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexImage: resolved OK\n");
        }
    }
    __proc_glGetTexImage(target, level, format, type, pixels);
}

typedef void (__stdcall *__pfn_glGetTexLevelParameterfv)(GLenum, GLint, GLenum, void*);
static __pfn_glGetTexLevelParameterfv __proc_glGetTexLevelParameterfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexLevelParameterfv");
    if (__proc_glGetTexLevelParameterfv == nullptr) {
        __proc_glGetTexLevelParameterfv = (__pfn_glGetTexLevelParameterfv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexLevelParameterfv");
        if (__proc_glGetTexLevelParameterfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexLevelParameterfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexLevelParameterfv: resolved OK\n");
        }
    }
    __proc_glGetTexLevelParameterfv(target, level, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexLevelParameteriv)(GLenum, GLint, GLenum, void*);
static __pfn_glGetTexLevelParameteriv __proc_glGetTexLevelParameteriv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexLevelParameteriv");
    if (__proc_glGetTexLevelParameteriv == nullptr) {
        __proc_glGetTexLevelParameteriv = (__pfn_glGetTexLevelParameteriv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexLevelParameteriv");
        if (__proc_glGetTexLevelParameteriv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexLevelParameteriv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexLevelParameteriv: resolved OK\n");
        }
    }
    __proc_glGetTexLevelParameteriv(target, level, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexParameterfv)(GLenum, GLenum, void*);
static __pfn_glGetTexParameterfv __proc_glGetTexParameterfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexParameterfv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexParameterfv");
    if (__proc_glGetTexParameterfv == nullptr) {
        __proc_glGetTexParameterfv = (__pfn_glGetTexParameterfv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexParameterfv");
        if (__proc_glGetTexParameterfv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexParameterfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexParameterfv: resolved OK\n");
        }
    }
    __proc_glGetTexParameterfv(target, pname, params);
}

typedef void (__stdcall *__pfn_glGetTexParameteriv)(GLenum, GLenum, void*);
static __pfn_glGetTexParameteriv __proc_glGetTexParameteriv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glGetTexParameteriv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glGetTexParameteriv");
    if (__proc_glGetTexParameteriv == nullptr) {
        __proc_glGetTexParameteriv = (__pfn_glGetTexParameteriv)GetProcAddress(EnsureRealOpenGL32(), "glGetTexParameteriv");
        if (__proc_glGetTexParameteriv == nullptr) {
            printf("[opengl32_enh_cpp]   glGetTexParameteriv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glGetTexParameteriv: resolved OK\n");
        }
    }
    __proc_glGetTexParameteriv(target, pname, params);
}

typedef void (__stdcall *__pfn_glHint)(GLenum, GLenum);
static __pfn_glHint __proc_glHint = nullptr;

extern "C" __declspec(dllexport) void __stdcall glHint(GLenum target, GLenum mode) {
    ANAX_TRACE("glHint");
    if (__proc_glHint == nullptr) {
        __proc_glHint = (__pfn_glHint)GetProcAddress(EnsureRealOpenGL32(), "glHint");
        if (__proc_glHint == nullptr) {
            printf("[opengl32_enh_cpp]   glHint: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glHint: resolved OK\n");
        }
    }
    __proc_glHint(target, mode);
}

typedef void (__stdcall *__pfn_glIndexMask)(GLuint);
static __pfn_glIndexMask __proc_glIndexMask = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexMask(GLuint mask) {
    ANAX_TRACE("glIndexMask");
    if (__proc_glIndexMask == nullptr) {
        __proc_glIndexMask = (__pfn_glIndexMask)GetProcAddress(EnsureRealOpenGL32(), "glIndexMask");
        if (__proc_glIndexMask == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexMask: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexMask: resolved OK\n");
        }
    }
    __proc_glIndexMask(mask);
}

typedef void (__stdcall *__pfn_glIndexPointer)(GLenum, GLsizei, void*);
static __pfn_glIndexPointer __proc_glIndexPointer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexPointer(GLenum type, GLsizei stride, void* pointer) {
    ANAX_TRACE("glIndexPointer");
    if (__proc_glIndexPointer == nullptr) {
        __proc_glIndexPointer = (__pfn_glIndexPointer)GetProcAddress(EnsureRealOpenGL32(), "glIndexPointer");
        if (__proc_glIndexPointer == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexPointer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexPointer: resolved OK\n");
        }
    }
    __proc_glIndexPointer(type, stride, pointer);
}

typedef void (__stdcall *__pfn_glIndexd)(GLdouble);
static __pfn_glIndexd __proc_glIndexd = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexd(GLdouble c) {
    ANAX_TRACE("glIndexd");
    if (__proc_glIndexd == nullptr) {
        __proc_glIndexd = (__pfn_glIndexd)GetProcAddress(EnsureRealOpenGL32(), "glIndexd");
        if (__proc_glIndexd == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexd: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexd: resolved OK\n");
        }
    }
    __proc_glIndexd(c);
}

typedef void (__stdcall *__pfn_glIndexdv)(void*);
static __pfn_glIndexdv __proc_glIndexdv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexdv(void* c) {
    ANAX_TRACE("glIndexdv");
    if (__proc_glIndexdv == nullptr) {
        __proc_glIndexdv = (__pfn_glIndexdv)GetProcAddress(EnsureRealOpenGL32(), "glIndexdv");
        if (__proc_glIndexdv == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexdv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexdv: resolved OK\n");
        }
    }
    __proc_glIndexdv(c);
}

typedef void (__stdcall *__pfn_glIndexf)(GLfloat);
static __pfn_glIndexf __proc_glIndexf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexf(GLfloat c) {
    ANAX_TRACE("glIndexf");
    if (__proc_glIndexf == nullptr) {
        __proc_glIndexf = (__pfn_glIndexf)GetProcAddress(EnsureRealOpenGL32(), "glIndexf");
        if (__proc_glIndexf == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexf: resolved OK\n");
        }
    }
    __proc_glIndexf(c);
}

typedef void (__stdcall *__pfn_glIndexfv)(void*);
static __pfn_glIndexfv __proc_glIndexfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexfv(void* c) {
    ANAX_TRACE("glIndexfv");
    if (__proc_glIndexfv == nullptr) {
        __proc_glIndexfv = (__pfn_glIndexfv)GetProcAddress(EnsureRealOpenGL32(), "glIndexfv");
        if (__proc_glIndexfv == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexfv: resolved OK\n");
        }
    }
    __proc_glIndexfv(c);
}

typedef void (__stdcall *__pfn_glIndexi)(GLint);
static __pfn_glIndexi __proc_glIndexi = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexi(GLint c) {
    ANAX_TRACE("glIndexi");
    if (__proc_glIndexi == nullptr) {
        __proc_glIndexi = (__pfn_glIndexi)GetProcAddress(EnsureRealOpenGL32(), "glIndexi");
        if (__proc_glIndexi == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexi: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexi: resolved OK\n");
        }
    }
    __proc_glIndexi(c);
}

typedef void (__stdcall *__pfn_glIndexiv)(void*);
static __pfn_glIndexiv __proc_glIndexiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexiv(void* c) {
    ANAX_TRACE("glIndexiv");
    if (__proc_glIndexiv == nullptr) {
        __proc_glIndexiv = (__pfn_glIndexiv)GetProcAddress(EnsureRealOpenGL32(), "glIndexiv");
        if (__proc_glIndexiv == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexiv: resolved OK\n");
        }
    }
    __proc_glIndexiv(c);
}

typedef void (__stdcall *__pfn_glIndexs)(GLshort);
static __pfn_glIndexs __proc_glIndexs = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexs(GLshort c) {
    ANAX_TRACE("glIndexs");
    if (__proc_glIndexs == nullptr) {
        __proc_glIndexs = (__pfn_glIndexs)GetProcAddress(EnsureRealOpenGL32(), "glIndexs");
        if (__proc_glIndexs == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexs: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexs: resolved OK\n");
        }
    }
    __proc_glIndexs(c);
}

typedef void (__stdcall *__pfn_glIndexsv)(void*);
static __pfn_glIndexsv __proc_glIndexsv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexsv(void* c) {
    ANAX_TRACE("glIndexsv");
    if (__proc_glIndexsv == nullptr) {
        __proc_glIndexsv = (__pfn_glIndexsv)GetProcAddress(EnsureRealOpenGL32(), "glIndexsv");
        if (__proc_glIndexsv == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexsv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexsv: resolved OK\n");
        }
    }
    __proc_glIndexsv(c);
}

typedef void (__stdcall *__pfn_glIndexub)(GLubyte);
static __pfn_glIndexub __proc_glIndexub = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexub(GLubyte c) {
    ANAX_TRACE("glIndexub");
    if (__proc_glIndexub == nullptr) {
        __proc_glIndexub = (__pfn_glIndexub)GetProcAddress(EnsureRealOpenGL32(), "glIndexub");
        if (__proc_glIndexub == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexub: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexub: resolved OK\n");
        }
    }
    __proc_glIndexub(c);
}

typedef void (__stdcall *__pfn_glIndexubv)(void*);
static __pfn_glIndexubv __proc_glIndexubv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glIndexubv(void* c) {
    ANAX_TRACE("glIndexubv");
    if (__proc_glIndexubv == nullptr) {
        __proc_glIndexubv = (__pfn_glIndexubv)GetProcAddress(EnsureRealOpenGL32(), "glIndexubv");
        if (__proc_glIndexubv == nullptr) {
            printf("[opengl32_enh_cpp]   glIndexubv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIndexubv: resolved OK\n");
        }
    }
    __proc_glIndexubv(c);
}

typedef void (__stdcall *__pfn_glInitNames)(void);
static __pfn_glInitNames __proc_glInitNames = nullptr;

extern "C" __declspec(dllexport) void __stdcall glInitNames(void) {
    ANAX_TRACE("glInitNames");
    if (__proc_glInitNames == nullptr) {
        __proc_glInitNames = (__pfn_glInitNames)GetProcAddress(EnsureRealOpenGL32(), "glInitNames");
        if (__proc_glInitNames == nullptr) {
            printf("[opengl32_enh_cpp]   glInitNames: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glInitNames: resolved OK\n");
        }
    }
    __proc_glInitNames();
}

typedef void (__stdcall *__pfn_glInterleavedArrays)(GLenum, GLsizei, void*);
static __pfn_glInterleavedArrays __proc_glInterleavedArrays = nullptr;

extern "C" __declspec(dllexport) void __stdcall glInterleavedArrays(GLenum format, GLsizei stride, void* pointer) {
    ANAX_TRACE("glInterleavedArrays");
    if (__proc_glInterleavedArrays == nullptr) {
        __proc_glInterleavedArrays = (__pfn_glInterleavedArrays)GetProcAddress(EnsureRealOpenGL32(), "glInterleavedArrays");
        if (__proc_glInterleavedArrays == nullptr) {
            printf("[opengl32_enh_cpp]   glInterleavedArrays: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glInterleavedArrays: resolved OK\n");
        }
    }
    __proc_glInterleavedArrays(format, stride, pointer);
}

typedef GLboolean (__stdcall *__pfn_glIsEnabled)(GLenum);
static __pfn_glIsEnabled __proc_glIsEnabled = nullptr;

extern "C" __declspec(dllexport) GLboolean __stdcall glIsEnabled(GLenum cap) {
    ANAX_TRACE("glIsEnabled");
    if (__proc_glIsEnabled == nullptr) {
        __proc_glIsEnabled = (__pfn_glIsEnabled)GetProcAddress(EnsureRealOpenGL32(), "glIsEnabled");
        if (__proc_glIsEnabled == nullptr) {
            printf("[opengl32_enh_cpp]   glIsEnabled: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIsEnabled: resolved OK\n");
        }
    }
    return __proc_glIsEnabled(cap);
}

typedef GLboolean (__stdcall *__pfn_glIsList)(GLuint);
static __pfn_glIsList __proc_glIsList = nullptr;

extern "C" __declspec(dllexport) GLboolean __stdcall glIsList(GLuint list) {
    ANAX_TRACE("glIsList");
    if (__proc_glIsList == nullptr) {
        __proc_glIsList = (__pfn_glIsList)GetProcAddress(EnsureRealOpenGL32(), "glIsList");
        if (__proc_glIsList == nullptr) {
            printf("[opengl32_enh_cpp]   glIsList: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIsList: resolved OK\n");
        }
    }
    return __proc_glIsList(list);
}

typedef GLboolean (__stdcall *__pfn_glIsTexture)(GLuint);
static __pfn_glIsTexture __proc_glIsTexture = nullptr;

extern "C" __declspec(dllexport) GLboolean __stdcall glIsTexture(GLuint texture) {
    ANAX_TRACE("glIsTexture");
    if (__proc_glIsTexture == nullptr) {
        __proc_glIsTexture = (__pfn_glIsTexture)GetProcAddress(EnsureRealOpenGL32(), "glIsTexture");
        if (__proc_glIsTexture == nullptr) {
            printf("[opengl32_enh_cpp]   glIsTexture: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glIsTexture: resolved OK\n");
        }
    }
    return __proc_glIsTexture(texture);
}

typedef void (__stdcall *__pfn_glLightModelf)(GLenum, GLfloat);
static __pfn_glLightModelf __proc_glLightModelf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightModelf(GLenum pname, GLfloat param) {
    ANAX_TRACE("glLightModelf");
    if (__proc_glLightModelf == nullptr) {
        __proc_glLightModelf = (__pfn_glLightModelf)GetProcAddress(EnsureRealOpenGL32(), "glLightModelf");
        if (__proc_glLightModelf == nullptr) {
            printf("[opengl32_enh_cpp]   glLightModelf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightModelf: resolved OK\n");
        }
    }
    __proc_glLightModelf(pname, param);
}

typedef void (__stdcall *__pfn_glLightModelfv)(GLenum, void*);
static __pfn_glLightModelfv __proc_glLightModelfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightModelfv(GLenum pname, void* params) {
    ANAX_TRACE("glLightModelfv");
    if (__proc_glLightModelfv == nullptr) {
        __proc_glLightModelfv = (__pfn_glLightModelfv)GetProcAddress(EnsureRealOpenGL32(), "glLightModelfv");
        if (__proc_glLightModelfv == nullptr) {
            printf("[opengl32_enh_cpp]   glLightModelfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightModelfv: resolved OK\n");
        }
    }
    __proc_glLightModelfv(pname, params);
}

typedef void (__stdcall *__pfn_glLightModeli)(GLenum, GLint);
static __pfn_glLightModeli __proc_glLightModeli = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightModeli(GLenum pname, GLint param) {
    ANAX_TRACE("glLightModeli");
    if (__proc_glLightModeli == nullptr) {
        __proc_glLightModeli = (__pfn_glLightModeli)GetProcAddress(EnsureRealOpenGL32(), "glLightModeli");
        if (__proc_glLightModeli == nullptr) {
            printf("[opengl32_enh_cpp]   glLightModeli: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightModeli: resolved OK\n");
        }
    }
    __proc_glLightModeli(pname, param);
}

typedef void (__stdcall *__pfn_glLightModeliv)(GLenum, void*);
static __pfn_glLightModeliv __proc_glLightModeliv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightModeliv(GLenum pname, void* params) {
    ANAX_TRACE("glLightModeliv");
    if (__proc_glLightModeliv == nullptr) {
        __proc_glLightModeliv = (__pfn_glLightModeliv)GetProcAddress(EnsureRealOpenGL32(), "glLightModeliv");
        if (__proc_glLightModeliv == nullptr) {
            printf("[opengl32_enh_cpp]   glLightModeliv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightModeliv: resolved OK\n");
        }
    }
    __proc_glLightModeliv(pname, params);
}

typedef void (__stdcall *__pfn_glLightf)(GLenum, GLenum, GLfloat);
static __pfn_glLightf __proc_glLightf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightf(GLenum light, GLenum pname, GLfloat param) {
    ANAX_TRACE("glLightf");
    if (__proc_glLightf == nullptr) {
        __proc_glLightf = (__pfn_glLightf)GetProcAddress(EnsureRealOpenGL32(), "glLightf");
        if (__proc_glLightf == nullptr) {
            printf("[opengl32_enh_cpp]   glLightf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightf: resolved OK\n");
        }
    }
    __proc_glLightf(light, pname, param);
}

typedef void (__stdcall *__pfn_glLightfv)(GLenum, GLenum, void*);
static __pfn_glLightfv __proc_glLightfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightfv(GLenum light, GLenum pname, void* params) {
    ANAX_TRACE("glLightfv");
    if (__proc_glLightfv == nullptr) {
        __proc_glLightfv = (__pfn_glLightfv)GetProcAddress(EnsureRealOpenGL32(), "glLightfv");
        if (__proc_glLightfv == nullptr) {
            printf("[opengl32_enh_cpp]   glLightfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightfv: resolved OK\n");
        }
    }
    __proc_glLightfv(light, pname, params);
}

typedef void (__stdcall *__pfn_glLighti)(GLenum, GLenum, GLint);
static __pfn_glLighti __proc_glLighti = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLighti(GLenum light, GLenum pname, GLint param) {
    ANAX_TRACE("glLighti");
    if (__proc_glLighti == nullptr) {
        __proc_glLighti = (__pfn_glLighti)GetProcAddress(EnsureRealOpenGL32(), "glLighti");
        if (__proc_glLighti == nullptr) {
            printf("[opengl32_enh_cpp]   glLighti: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLighti: resolved OK\n");
        }
    }
    __proc_glLighti(light, pname, param);
}

typedef void (__stdcall *__pfn_glLightiv)(GLenum, GLenum, void*);
static __pfn_glLightiv __proc_glLightiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLightiv(GLenum light, GLenum pname, void* params) {
    ANAX_TRACE("glLightiv");
    if (__proc_glLightiv == nullptr) {
        __proc_glLightiv = (__pfn_glLightiv)GetProcAddress(EnsureRealOpenGL32(), "glLightiv");
        if (__proc_glLightiv == nullptr) {
            printf("[opengl32_enh_cpp]   glLightiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLightiv: resolved OK\n");
        }
    }
    __proc_glLightiv(light, pname, params);
}

typedef void (__stdcall *__pfn_glLineStipple)(GLint, GLushort);
static __pfn_glLineStipple __proc_glLineStipple = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLineStipple(GLint factor, GLushort pattern) {
    ANAX_TRACE("glLineStipple");
    if (__proc_glLineStipple == nullptr) {
        __proc_glLineStipple = (__pfn_glLineStipple)GetProcAddress(EnsureRealOpenGL32(), "glLineStipple");
        if (__proc_glLineStipple == nullptr) {
            printf("[opengl32_enh_cpp]   glLineStipple: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLineStipple: resolved OK\n");
        }
    }
    __proc_glLineStipple(factor, pattern);
}

typedef void (__stdcall *__pfn_glLineWidth)(GLfloat);
static __pfn_glLineWidth __proc_glLineWidth = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLineWidth(GLfloat width) {
    ANAX_TRACE("glLineWidth");
    if (__proc_glLineWidth == nullptr) {
        __proc_glLineWidth = (__pfn_glLineWidth)GetProcAddress(EnsureRealOpenGL32(), "glLineWidth");
        if (__proc_glLineWidth == nullptr) {
            printf("[opengl32_enh_cpp]   glLineWidth: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLineWidth: resolved OK\n");
        }
    }
    __proc_glLineWidth(width);
}

typedef void (__stdcall *__pfn_glListBase)(GLuint);
static __pfn_glListBase __proc_glListBase = nullptr;

extern "C" __declspec(dllexport) void __stdcall glListBase(GLuint base) {
    ANAX_TRACE("glListBase");
    if (__proc_glListBase == nullptr) {
        __proc_glListBase = (__pfn_glListBase)GetProcAddress(EnsureRealOpenGL32(), "glListBase");
        if (__proc_glListBase == nullptr) {
            printf("[opengl32_enh_cpp]   glListBase: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glListBase: resolved OK\n");
        }
    }
    __proc_glListBase(base);
}

typedef void (__stdcall *__pfn_glLoadIdentity)(void);
static __pfn_glLoadIdentity __proc_glLoadIdentity = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLoadIdentity(void) {
    ANAX_TRACE("glLoadIdentity");
    if (__proc_glLoadIdentity == nullptr) {
        __proc_glLoadIdentity = (__pfn_glLoadIdentity)GetProcAddress(EnsureRealOpenGL32(), "glLoadIdentity");
        if (__proc_glLoadIdentity == nullptr) {
            printf("[opengl32_enh_cpp]   glLoadIdentity: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLoadIdentity: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glLoadIdentity();
}

typedef void (__stdcall *__pfn_glLoadMatrixd)(void*);
static __pfn_glLoadMatrixd __proc_glLoadMatrixd = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLoadMatrixd(void* m) {
    ANAX_TRACE("glLoadMatrixd");
    if (__proc_glLoadMatrixd == nullptr) {
        __proc_glLoadMatrixd = (__pfn_glLoadMatrixd)GetProcAddress(EnsureRealOpenGL32(), "glLoadMatrixd");
        if (__proc_glLoadMatrixd == nullptr) {
            printf("[opengl32_enh_cpp]   glLoadMatrixd: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLoadMatrixd: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glLoadMatrixd(m);
}

typedef void (__stdcall *__pfn_glLoadMatrixf)(void*);
static __pfn_glLoadMatrixf __proc_glLoadMatrixf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLoadMatrixf(void* m) {
    ANAX_TRACE("glLoadMatrixf");
    if (__proc_glLoadMatrixf == nullptr) {
        __proc_glLoadMatrixf = (__pfn_glLoadMatrixf)GetProcAddress(EnsureRealOpenGL32(), "glLoadMatrixf");
        if (__proc_glLoadMatrixf == nullptr) {
            printf("[opengl32_enh_cpp]   glLoadMatrixf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLoadMatrixf: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glLoadMatrixf(m);
}

typedef void (__stdcall *__pfn_glLoadName)(GLuint);
static __pfn_glLoadName __proc_glLoadName = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLoadName(GLuint name) {
    ANAX_TRACE("glLoadName");
    if (__proc_glLoadName == nullptr) {
        __proc_glLoadName = (__pfn_glLoadName)GetProcAddress(EnsureRealOpenGL32(), "glLoadName");
        if (__proc_glLoadName == nullptr) {
            printf("[opengl32_enh_cpp]   glLoadName: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLoadName: resolved OK\n");
        }
    }
    __proc_glLoadName(name);
}

typedef void (__stdcall *__pfn_glLogicOp)(GLenum);
static __pfn_glLogicOp __proc_glLogicOp = nullptr;

extern "C" __declspec(dllexport) void __stdcall glLogicOp(GLenum opcode) {
    ANAX_TRACE("glLogicOp");
    if (__proc_glLogicOp == nullptr) {
        __proc_glLogicOp = (__pfn_glLogicOp)GetProcAddress(EnsureRealOpenGL32(), "glLogicOp");
        if (__proc_glLogicOp == nullptr) {
            printf("[opengl32_enh_cpp]   glLogicOp: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glLogicOp: resolved OK\n");
        }
    }
    __proc_glLogicOp(opcode);
}

typedef void (__stdcall *__pfn_glMap1d)(GLenum, GLdouble, GLdouble, GLint, GLint, void*);
static __pfn_glMap1d __proc_glMap1d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, void* points) {
    ANAX_TRACE("glMap1d");
    if (__proc_glMap1d == nullptr) {
        __proc_glMap1d = (__pfn_glMap1d)GetProcAddress(EnsureRealOpenGL32(), "glMap1d");
        if (__proc_glMap1d == nullptr) {
            printf("[opengl32_enh_cpp]   glMap1d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMap1d: resolved OK\n");
        }
    }
    __proc_glMap1d(target, u1, u2, stride, order, points);
}

typedef void (__stdcall *__pfn_glMap1f)(GLenum, GLfloat, GLfloat, GLint, GLint, void*);
static __pfn_glMap1f __proc_glMap1f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, void* points) {
    ANAX_TRACE("glMap1f");
    if (__proc_glMap1f == nullptr) {
        __proc_glMap1f = (__pfn_glMap1f)GetProcAddress(EnsureRealOpenGL32(), "glMap1f");
        if (__proc_glMap1f == nullptr) {
            printf("[opengl32_enh_cpp]   glMap1f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMap1f: resolved OK\n");
        }
    }
    __proc_glMap1f(target, u1, u2, stride, order, points);
}

typedef void (__stdcall *__pfn_glMap2d)(GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, void*);
static __pfn_glMap2d __proc_glMap2d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, void* points) {
    ANAX_TRACE("glMap2d");
    if (__proc_glMap2d == nullptr) {
        __proc_glMap2d = (__pfn_glMap2d)GetProcAddress(EnsureRealOpenGL32(), "glMap2d");
        if (__proc_glMap2d == nullptr) {
            printf("[opengl32_enh_cpp]   glMap2d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMap2d: resolved OK\n");
        }
    }
    __proc_glMap2d(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

typedef void (__stdcall *__pfn_glMap2f)(GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, void*);
static __pfn_glMap2f __proc_glMap2f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, void* points) {
    ANAX_TRACE("glMap2f");
    if (__proc_glMap2f == nullptr) {
        __proc_glMap2f = (__pfn_glMap2f)GetProcAddress(EnsureRealOpenGL32(), "glMap2f");
        if (__proc_glMap2f == nullptr) {
            printf("[opengl32_enh_cpp]   glMap2f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMap2f: resolved OK\n");
        }
    }
    __proc_glMap2f(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

typedef void (__stdcall *__pfn_glMapGrid1d)(GLint, GLdouble, GLdouble);
static __pfn_glMapGrid1d __proc_glMapGrid1d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) {
    ANAX_TRACE("glMapGrid1d");
    if (__proc_glMapGrid1d == nullptr) {
        __proc_glMapGrid1d = (__pfn_glMapGrid1d)GetProcAddress(EnsureRealOpenGL32(), "glMapGrid1d");
        if (__proc_glMapGrid1d == nullptr) {
            printf("[opengl32_enh_cpp]   glMapGrid1d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMapGrid1d: resolved OK\n");
        }
    }
    __proc_glMapGrid1d(un, u1, u2);
}

typedef void (__stdcall *__pfn_glMapGrid1f)(GLint, GLfloat, GLfloat);
static __pfn_glMapGrid1f __proc_glMapGrid1f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMapGrid1f(GLint un, GLfloat u1, GLfloat u2) {
    ANAX_TRACE("glMapGrid1f");
    if (__proc_glMapGrid1f == nullptr) {
        __proc_glMapGrid1f = (__pfn_glMapGrid1f)GetProcAddress(EnsureRealOpenGL32(), "glMapGrid1f");
        if (__proc_glMapGrid1f == nullptr) {
            printf("[opengl32_enh_cpp]   glMapGrid1f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMapGrid1f: resolved OK\n");
        }
    }
    __proc_glMapGrid1f(un, u1, u2);
}

typedef void (__stdcall *__pfn_glMapGrid2d)(GLint, GLdouble, GLdouble, GLint, GLdouble, GLdouble);
static __pfn_glMapGrid2d __proc_glMapGrid2d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) {
    ANAX_TRACE("glMapGrid2d");
    if (__proc_glMapGrid2d == nullptr) {
        __proc_glMapGrid2d = (__pfn_glMapGrid2d)GetProcAddress(EnsureRealOpenGL32(), "glMapGrid2d");
        if (__proc_glMapGrid2d == nullptr) {
            printf("[opengl32_enh_cpp]   glMapGrid2d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMapGrid2d: resolved OK\n");
        }
    }
    __proc_glMapGrid2d(un, u1, u2, vn, v1, v2);
}

typedef void (__stdcall *__pfn_glMapGrid2f)(GLint, GLfloat, GLfloat, GLint, GLfloat, GLfloat);
static __pfn_glMapGrid2f __proc_glMapGrid2f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) {
    ANAX_TRACE("glMapGrid2f");
    if (__proc_glMapGrid2f == nullptr) {
        __proc_glMapGrid2f = (__pfn_glMapGrid2f)GetProcAddress(EnsureRealOpenGL32(), "glMapGrid2f");
        if (__proc_glMapGrid2f == nullptr) {
            printf("[opengl32_enh_cpp]   glMapGrid2f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMapGrid2f: resolved OK\n");
        }
    }
    __proc_glMapGrid2f(un, u1, u2, vn, v1, v2);
}

typedef void (__stdcall *__pfn_glMaterialf)(GLenum, GLenum, GLfloat);
static __pfn_glMaterialf __proc_glMaterialf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    ANAX_TRACE("glMaterialf");
    if (__proc_glMaterialf == nullptr) {
        __proc_glMaterialf = (__pfn_glMaterialf)GetProcAddress(EnsureRealOpenGL32(), "glMaterialf");
        if (__proc_glMaterialf == nullptr) {
            printf("[opengl32_enh_cpp]   glMaterialf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMaterialf: resolved OK\n");
        }
    }
    __proc_glMaterialf(face, pname, param);
}

typedef void (__stdcall *__pfn_glMaterialfv)(GLenum, GLenum, void*);
static __pfn_glMaterialfv __proc_glMaterialfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMaterialfv(GLenum face, GLenum pname, void* params) {
    ANAX_TRACE("glMaterialfv");
    if (__proc_glMaterialfv == nullptr) {
        __proc_glMaterialfv = (__pfn_glMaterialfv)GetProcAddress(EnsureRealOpenGL32(), "glMaterialfv");
        if (__proc_glMaterialfv == nullptr) {
            printf("[opengl32_enh_cpp]   glMaterialfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMaterialfv: resolved OK\n");
        }
    }
    __proc_glMaterialfv(face, pname, params);
}

typedef void (__stdcall *__pfn_glMateriali)(GLenum, GLenum, GLint);
static __pfn_glMateriali __proc_glMateriali = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMateriali(GLenum face, GLenum pname, GLint param) {
    ANAX_TRACE("glMateriali");
    if (__proc_glMateriali == nullptr) {
        __proc_glMateriali = (__pfn_glMateriali)GetProcAddress(EnsureRealOpenGL32(), "glMateriali");
        if (__proc_glMateriali == nullptr) {
            printf("[opengl32_enh_cpp]   glMateriali: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMateriali: resolved OK\n");
        }
    }
    __proc_glMateriali(face, pname, param);
}

typedef void (__stdcall *__pfn_glMaterialiv)(GLenum, GLenum, void*);
static __pfn_glMaterialiv __proc_glMaterialiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMaterialiv(GLenum face, GLenum pname, void* params) {
    ANAX_TRACE("glMaterialiv");
    if (__proc_glMaterialiv == nullptr) {
        __proc_glMaterialiv = (__pfn_glMaterialiv)GetProcAddress(EnsureRealOpenGL32(), "glMaterialiv");
        if (__proc_glMaterialiv == nullptr) {
            printf("[opengl32_enh_cpp]   glMaterialiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMaterialiv: resolved OK\n");
        }
    }
    __proc_glMaterialiv(face, pname, params);
}

typedef void (__stdcall *__pfn_glMatrixMode)(GLenum);
static __pfn_glMatrixMode __proc_glMatrixMode = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMatrixMode(GLenum mode) {
    ANAX_TRACE("glMatrixMode");
    if (__proc_glMatrixMode == nullptr) {
        __proc_glMatrixMode = (__pfn_glMatrixMode)GetProcAddress(EnsureRealOpenGL32(), "glMatrixMode");
        if (__proc_glMatrixMode == nullptr) {
            printf("[opengl32_enh_cpp]   glMatrixMode: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMatrixMode: resolved OK\n");
        }
    }
    NotifyMatrixMode(mode);
    __proc_glMatrixMode(mode);
}

typedef void (__stdcall *__pfn_glMultMatrixd)(void*);
static __pfn_glMultMatrixd __proc_glMultMatrixd = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMultMatrixd(void* m) {
    ANAX_TRACE("glMultMatrixd");
    if (__proc_glMultMatrixd == nullptr) {
        __proc_glMultMatrixd = (__pfn_glMultMatrixd)GetProcAddress(EnsureRealOpenGL32(), "glMultMatrixd");
        if (__proc_glMultMatrixd == nullptr) {
            printf("[opengl32_enh_cpp]   glMultMatrixd: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMultMatrixd: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glMultMatrixd(m);
}

typedef void (__stdcall *__pfn_glMultMatrixf)(void*);
static __pfn_glMultMatrixf __proc_glMultMatrixf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glMultMatrixf(void* m) {
    ANAX_TRACE("glMultMatrixf");
    if (__proc_glMultMatrixf == nullptr) {
        __proc_glMultMatrixf = (__pfn_glMultMatrixf)GetProcAddress(EnsureRealOpenGL32(), "glMultMatrixf");
        if (__proc_glMultMatrixf == nullptr) {
            printf("[opengl32_enh_cpp]   glMultMatrixf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glMultMatrixf: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glMultMatrixf(m);
}

typedef void (__stdcall *__pfn_glNewList)(GLuint, GLenum);
static __pfn_glNewList __proc_glNewList = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNewList(GLuint list, GLenum mode) {
    ANAX_TRACE("glNewList");
    if (__proc_glNewList == nullptr) {
        __proc_glNewList = (__pfn_glNewList)GetProcAddress(EnsureRealOpenGL32(), "glNewList");
        if (__proc_glNewList == nullptr) {
            printf("[opengl32_enh_cpp]   glNewList: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNewList: resolved OK\n");
        }
    }
    __proc_glNewList(list, mode);
}

typedef void (__stdcall *__pfn_glNormal3b)(GLbyte, GLbyte, GLbyte);
static __pfn_glNormal3b __proc_glNormal3b = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz) {
    ANAX_TRACE("glNormal3b");
    if (__proc_glNormal3b == nullptr) {
        __proc_glNormal3b = (__pfn_glNormal3b)GetProcAddress(EnsureRealOpenGL32(), "glNormal3b");
        if (__proc_glNormal3b == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3b: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3b: resolved OK\n");
        }
    }
    __proc_glNormal3b(nx, ny, nz);
}

typedef void (__stdcall *__pfn_glNormal3bv)(void*);
static __pfn_glNormal3bv __proc_glNormal3bv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3bv(void* v) {
    ANAX_TRACE("glNormal3bv");
    if (__proc_glNormal3bv == nullptr) {
        __proc_glNormal3bv = (__pfn_glNormal3bv)GetProcAddress(EnsureRealOpenGL32(), "glNormal3bv");
        if (__proc_glNormal3bv == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3bv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3bv: resolved OK\n");
        }
    }
    __proc_glNormal3bv(v);
}

typedef void (__stdcall *__pfn_glNormal3d)(GLdouble, GLdouble, GLdouble);
static __pfn_glNormal3d __proc_glNormal3d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz) {
    ANAX_TRACE("glNormal3d");
    if (__proc_glNormal3d == nullptr) {
        __proc_glNormal3d = (__pfn_glNormal3d)GetProcAddress(EnsureRealOpenGL32(), "glNormal3d");
        if (__proc_glNormal3d == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3d: resolved OK\n");
        }
    }
    __proc_glNormal3d(nx, ny, nz);
}

typedef void (__stdcall *__pfn_glNormal3dv)(void*);
static __pfn_glNormal3dv __proc_glNormal3dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3dv(void* v) {
    ANAX_TRACE("glNormal3dv");
    if (__proc_glNormal3dv == nullptr) {
        __proc_glNormal3dv = (__pfn_glNormal3dv)GetProcAddress(EnsureRealOpenGL32(), "glNormal3dv");
        if (__proc_glNormal3dv == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3dv: resolved OK\n");
        }
    }
    __proc_glNormal3dv(v);
}

typedef void (__stdcall *__pfn_glNormal3f)(GLfloat, GLfloat, GLfloat);
static __pfn_glNormal3f __proc_glNormal3f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    ANAX_TRACE("glNormal3f");
    if (__proc_glNormal3f == nullptr) {
        __proc_glNormal3f = (__pfn_glNormal3f)GetProcAddress(EnsureRealOpenGL32(), "glNormal3f");
        if (__proc_glNormal3f == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3f: resolved OK\n");
        }
    }
    __proc_glNormal3f(nx, ny, nz);
}

typedef void (__stdcall *__pfn_glNormal3fv)(void*);
static __pfn_glNormal3fv __proc_glNormal3fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3fv(void* v) {
    ANAX_TRACE("glNormal3fv");
    if (__proc_glNormal3fv == nullptr) {
        __proc_glNormal3fv = (__pfn_glNormal3fv)GetProcAddress(EnsureRealOpenGL32(), "glNormal3fv");
        if (__proc_glNormal3fv == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3fv: resolved OK\n");
        }
    }
    __proc_glNormal3fv(v);
}

typedef void (__stdcall *__pfn_glNormal3i)(GLint, GLint, GLint);
static __pfn_glNormal3i __proc_glNormal3i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3i(GLint nx, GLint ny, GLint nz) {
    ANAX_TRACE("glNormal3i");
    if (__proc_glNormal3i == nullptr) {
        __proc_glNormal3i = (__pfn_glNormal3i)GetProcAddress(EnsureRealOpenGL32(), "glNormal3i");
        if (__proc_glNormal3i == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3i: resolved OK\n");
        }
    }
    __proc_glNormal3i(nx, ny, nz);
}

typedef void (__stdcall *__pfn_glNormal3iv)(void*);
static __pfn_glNormal3iv __proc_glNormal3iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3iv(void* v) {
    ANAX_TRACE("glNormal3iv");
    if (__proc_glNormal3iv == nullptr) {
        __proc_glNormal3iv = (__pfn_glNormal3iv)GetProcAddress(EnsureRealOpenGL32(), "glNormal3iv");
        if (__proc_glNormal3iv == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3iv: resolved OK\n");
        }
    }
    __proc_glNormal3iv(v);
}

typedef void (__stdcall *__pfn_glNormal3s)(GLshort, GLshort, GLshort);
static __pfn_glNormal3s __proc_glNormal3s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3s(GLshort nx, GLshort ny, GLshort nz) {
    ANAX_TRACE("glNormal3s");
    if (__proc_glNormal3s == nullptr) {
        __proc_glNormal3s = (__pfn_glNormal3s)GetProcAddress(EnsureRealOpenGL32(), "glNormal3s");
        if (__proc_glNormal3s == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3s: resolved OK\n");
        }
    }
    __proc_glNormal3s(nx, ny, nz);
}

typedef void (__stdcall *__pfn_glNormal3sv)(void*);
static __pfn_glNormal3sv __proc_glNormal3sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormal3sv(void* v) {
    ANAX_TRACE("glNormal3sv");
    if (__proc_glNormal3sv == nullptr) {
        __proc_glNormal3sv = (__pfn_glNormal3sv)GetProcAddress(EnsureRealOpenGL32(), "glNormal3sv");
        if (__proc_glNormal3sv == nullptr) {
            printf("[opengl32_enh_cpp]   glNormal3sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormal3sv: resolved OK\n");
        }
    }
    __proc_glNormal3sv(v);
}

typedef void (__stdcall *__pfn_glNormalPointer)(GLenum, GLsizei, void*);
static __pfn_glNormalPointer __proc_glNormalPointer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glNormalPointer(GLenum type, GLsizei stride, void* pointer) {
    ANAX_TRACE("glNormalPointer");
    if (__proc_glNormalPointer == nullptr) {
        __proc_glNormalPointer = (__pfn_glNormalPointer)GetProcAddress(EnsureRealOpenGL32(), "glNormalPointer");
        if (__proc_glNormalPointer == nullptr) {
            printf("[opengl32_enh_cpp]   glNormalPointer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glNormalPointer: resolved OK\n");
        }
    }
    __proc_glNormalPointer(type, stride, pointer);
}

typedef void (__stdcall *__pfn_glOrtho)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glOrtho __proc_glOrtho = nullptr;

extern "C" __declspec(dllexport) void __stdcall glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    ANAX_TRACE("glOrtho");
    if (__proc_glOrtho == nullptr) {
        __proc_glOrtho = (__pfn_glOrtho)GetProcAddress(EnsureRealOpenGL32(), "glOrtho");
        if (__proc_glOrtho == nullptr) {
            printf("[opengl32_enh_cpp]   glOrtho: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glOrtho: resolved OK\n");
        }
    }
    NotifyTwoDProjection();
    NotifyTwoDPassBegan();
    __proc_glOrtho(left, right, bottom, top, zNear, zFar);
}

typedef void (__stdcall *__pfn_glPassThrough)(GLfloat);
static __pfn_glPassThrough __proc_glPassThrough = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPassThrough(GLfloat token) {
    ANAX_TRACE("glPassThrough");
    if (__proc_glPassThrough == nullptr) {
        __proc_glPassThrough = (__pfn_glPassThrough)GetProcAddress(EnsureRealOpenGL32(), "glPassThrough");
        if (__proc_glPassThrough == nullptr) {
            printf("[opengl32_enh_cpp]   glPassThrough: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPassThrough: resolved OK\n");
        }
    }
    __proc_glPassThrough(token);
}

typedef void (__stdcall *__pfn_glPixelMapfv)(GLenum, GLsizei, void*);
static __pfn_glPixelMapfv __proc_glPixelMapfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelMapfv(GLenum map, GLsizei mapsize, void* values) {
    ANAX_TRACE("glPixelMapfv");
    if (__proc_glPixelMapfv == nullptr) {
        __proc_glPixelMapfv = (__pfn_glPixelMapfv)GetProcAddress(EnsureRealOpenGL32(), "glPixelMapfv");
        if (__proc_glPixelMapfv == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelMapfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelMapfv: resolved OK\n");
        }
    }
    __proc_glPixelMapfv(map, mapsize, values);
}

typedef void (__stdcall *__pfn_glPixelMapuiv)(GLenum, GLsizei, void*);
static __pfn_glPixelMapuiv __proc_glPixelMapuiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelMapuiv(GLenum map, GLsizei mapsize, void* values) {
    ANAX_TRACE("glPixelMapuiv");
    if (__proc_glPixelMapuiv == nullptr) {
        __proc_glPixelMapuiv = (__pfn_glPixelMapuiv)GetProcAddress(EnsureRealOpenGL32(), "glPixelMapuiv");
        if (__proc_glPixelMapuiv == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelMapuiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelMapuiv: resolved OK\n");
        }
    }
    __proc_glPixelMapuiv(map, mapsize, values);
}

typedef void (__stdcall *__pfn_glPixelMapusv)(GLenum, GLsizei, void*);
static __pfn_glPixelMapusv __proc_glPixelMapusv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelMapusv(GLenum map, GLsizei mapsize, void* values) {
    ANAX_TRACE("glPixelMapusv");
    if (__proc_glPixelMapusv == nullptr) {
        __proc_glPixelMapusv = (__pfn_glPixelMapusv)GetProcAddress(EnsureRealOpenGL32(), "glPixelMapusv");
        if (__proc_glPixelMapusv == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelMapusv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelMapusv: resolved OK\n");
        }
    }
    __proc_glPixelMapusv(map, mapsize, values);
}

typedef void (__stdcall *__pfn_glPixelStoref)(GLenum, GLfloat);
static __pfn_glPixelStoref __proc_glPixelStoref = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelStoref(GLenum pname, GLfloat param) {
    ANAX_TRACE("glPixelStoref");
    if (__proc_glPixelStoref == nullptr) {
        __proc_glPixelStoref = (__pfn_glPixelStoref)GetProcAddress(EnsureRealOpenGL32(), "glPixelStoref");
        if (__proc_glPixelStoref == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelStoref: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelStoref: resolved OK\n");
        }
    }
    __proc_glPixelStoref(pname, param);
}

typedef void (__stdcall *__pfn_glPixelStorei)(GLenum, GLint);
static __pfn_glPixelStorei __proc_glPixelStorei = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelStorei(GLenum pname, GLint param) {
    ANAX_TRACE("glPixelStorei");
    if (__proc_glPixelStorei == nullptr) {
        __proc_glPixelStorei = (__pfn_glPixelStorei)GetProcAddress(EnsureRealOpenGL32(), "glPixelStorei");
        if (__proc_glPixelStorei == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelStorei: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelStorei: resolved OK\n");
        }
    }
    __proc_glPixelStorei(pname, param);
}

typedef void (__stdcall *__pfn_glPixelTransferf)(GLenum, GLfloat);
static __pfn_glPixelTransferf __proc_glPixelTransferf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelTransferf(GLenum pname, GLfloat param) {
    ANAX_TRACE("glPixelTransferf");
    if (__proc_glPixelTransferf == nullptr) {
        __proc_glPixelTransferf = (__pfn_glPixelTransferf)GetProcAddress(EnsureRealOpenGL32(), "glPixelTransferf");
        if (__proc_glPixelTransferf == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelTransferf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelTransferf: resolved OK\n");
        }
    }
    __proc_glPixelTransferf(pname, param);
}

typedef void (__stdcall *__pfn_glPixelTransferi)(GLenum, GLint);
static __pfn_glPixelTransferi __proc_glPixelTransferi = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelTransferi(GLenum pname, GLint param) {
    ANAX_TRACE("glPixelTransferi");
    if (__proc_glPixelTransferi == nullptr) {
        __proc_glPixelTransferi = (__pfn_glPixelTransferi)GetProcAddress(EnsureRealOpenGL32(), "glPixelTransferi");
        if (__proc_glPixelTransferi == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelTransferi: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelTransferi: resolved OK\n");
        }
    }
    __proc_glPixelTransferi(pname, param);
}

typedef void (__stdcall *__pfn_glPixelZoom)(GLfloat, GLfloat);
static __pfn_glPixelZoom __proc_glPixelZoom = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPixelZoom(GLfloat xfactor, GLfloat yfactor) {
    ANAX_TRACE("glPixelZoom");
    if (__proc_glPixelZoom == nullptr) {
        __proc_glPixelZoom = (__pfn_glPixelZoom)GetProcAddress(EnsureRealOpenGL32(), "glPixelZoom");
        if (__proc_glPixelZoom == nullptr) {
            printf("[opengl32_enh_cpp]   glPixelZoom: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPixelZoom: resolved OK\n");
        }
    }
    __proc_glPixelZoom(xfactor, yfactor);
}

typedef void (__stdcall *__pfn_glPointSize)(GLfloat);
static __pfn_glPointSize __proc_glPointSize = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPointSize(GLfloat size) {
    ANAX_TRACE("glPointSize");
    if (__proc_glPointSize == nullptr) {
        __proc_glPointSize = (__pfn_glPointSize)GetProcAddress(EnsureRealOpenGL32(), "glPointSize");
        if (__proc_glPointSize == nullptr) {
            printf("[opengl32_enh_cpp]   glPointSize: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPointSize: resolved OK\n");
        }
    }
    __proc_glPointSize(size);
}

typedef void (__stdcall *__pfn_glPolygonMode)(GLenum, GLenum);
static __pfn_glPolygonMode __proc_glPolygonMode = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPolygonMode(GLenum face, GLenum mode) {
    ANAX_TRACE("glPolygonMode");
    if (__proc_glPolygonMode == nullptr) {
        __proc_glPolygonMode = (__pfn_glPolygonMode)GetProcAddress(EnsureRealOpenGL32(), "glPolygonMode");
        if (__proc_glPolygonMode == nullptr) {
            printf("[opengl32_enh_cpp]   glPolygonMode: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPolygonMode: resolved OK\n");
        }
    }
    __proc_glPolygonMode(face, mode);
}

typedef void (__stdcall *__pfn_glPolygonOffset)(GLfloat, GLfloat);
static __pfn_glPolygonOffset __proc_glPolygonOffset = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPolygonOffset(GLfloat factor, GLfloat units) {
    ANAX_TRACE("glPolygonOffset");
    if (__proc_glPolygonOffset == nullptr) {
        __proc_glPolygonOffset = (__pfn_glPolygonOffset)GetProcAddress(EnsureRealOpenGL32(), "glPolygonOffset");
        if (__proc_glPolygonOffset == nullptr) {
            printf("[opengl32_enh_cpp]   glPolygonOffset: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPolygonOffset: resolved OK\n");
        }
    }
    __proc_glPolygonOffset(factor, units);
}

typedef void (__stdcall *__pfn_glPolygonStipple)(void*);
static __pfn_glPolygonStipple __proc_glPolygonStipple = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPolygonStipple(void* mask) {
    ANAX_TRACE("glPolygonStipple");
    if (__proc_glPolygonStipple == nullptr) {
        __proc_glPolygonStipple = (__pfn_glPolygonStipple)GetProcAddress(EnsureRealOpenGL32(), "glPolygonStipple");
        if (__proc_glPolygonStipple == nullptr) {
            printf("[opengl32_enh_cpp]   glPolygonStipple: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPolygonStipple: resolved OK\n");
        }
    }
    __proc_glPolygonStipple(mask);
}

typedef void (__stdcall *__pfn_glPopAttrib)(void);
static __pfn_glPopAttrib __proc_glPopAttrib = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPopAttrib(void) {
    ANAX_TRACE("glPopAttrib");
    if (__proc_glPopAttrib == nullptr) {
        __proc_glPopAttrib = (__pfn_glPopAttrib)GetProcAddress(EnsureRealOpenGL32(), "glPopAttrib");
        if (__proc_glPopAttrib == nullptr) {
            printf("[opengl32_enh_cpp]   glPopAttrib: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPopAttrib: resolved OK\n");
        }
    }
    __proc_glPopAttrib();
}

typedef void (__stdcall *__pfn_glPopClientAttrib)(void);
static __pfn_glPopClientAttrib __proc_glPopClientAttrib = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPopClientAttrib(void) {
    ANAX_TRACE("glPopClientAttrib");
    if (__proc_glPopClientAttrib == nullptr) {
        __proc_glPopClientAttrib = (__pfn_glPopClientAttrib)GetProcAddress(EnsureRealOpenGL32(), "glPopClientAttrib");
        if (__proc_glPopClientAttrib == nullptr) {
            printf("[opengl32_enh_cpp]   glPopClientAttrib: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPopClientAttrib: resolved OK\n");
        }
    }
    __proc_glPopClientAttrib();
}

typedef void (__stdcall *__pfn_glPopMatrix)(void);
static __pfn_glPopMatrix __proc_glPopMatrix = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPopMatrix(void) {
    ANAX_TRACE("glPopMatrix");
    if (__proc_glPopMatrix == nullptr) {
        __proc_glPopMatrix = (__pfn_glPopMatrix)GetProcAddress(EnsureRealOpenGL32(), "glPopMatrix");
        if (__proc_glPopMatrix == nullptr) {
            printf("[opengl32_enh_cpp]   glPopMatrix: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPopMatrix: resolved OK\n");
        }
    }
    NotifyMatrixPop();
    __proc_glPopMatrix();
}

typedef void (__stdcall *__pfn_glPopName)(void);
static __pfn_glPopName __proc_glPopName = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPopName(void) {
    ANAX_TRACE("glPopName");
    if (__proc_glPopName == nullptr) {
        __proc_glPopName = (__pfn_glPopName)GetProcAddress(EnsureRealOpenGL32(), "glPopName");
        if (__proc_glPopName == nullptr) {
            printf("[opengl32_enh_cpp]   glPopName: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPopName: resolved OK\n");
        }
    }
    __proc_glPopName();
}

typedef void (__stdcall *__pfn_glPrioritizeTextures)(GLsizei, void*, void*);
static __pfn_glPrioritizeTextures __proc_glPrioritizeTextures = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPrioritizeTextures(GLsizei n, void* textures, void* priorities) {
    ANAX_TRACE("glPrioritizeTextures");
    if (__proc_glPrioritizeTextures == nullptr) {
        __proc_glPrioritizeTextures = (__pfn_glPrioritizeTextures)GetProcAddress(EnsureRealOpenGL32(), "glPrioritizeTextures");
        if (__proc_glPrioritizeTextures == nullptr) {
            printf("[opengl32_enh_cpp]   glPrioritizeTextures: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPrioritizeTextures: resolved OK\n");
        }
    }
    __proc_glPrioritizeTextures(n, textures, priorities);
}

typedef void (__stdcall *__pfn_glPushAttrib)(GLbitfield);
static __pfn_glPushAttrib __proc_glPushAttrib = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPushAttrib(GLbitfield mask) {
    ANAX_TRACE("glPushAttrib");
    if (__proc_glPushAttrib == nullptr) {
        __proc_glPushAttrib = (__pfn_glPushAttrib)GetProcAddress(EnsureRealOpenGL32(), "glPushAttrib");
        if (__proc_glPushAttrib == nullptr) {
            printf("[opengl32_enh_cpp]   glPushAttrib: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPushAttrib: resolved OK\n");
        }
    }
    __proc_glPushAttrib(mask);
}

typedef void (__stdcall *__pfn_glPushClientAttrib)(GLbitfield);
static __pfn_glPushClientAttrib __proc_glPushClientAttrib = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPushClientAttrib(GLbitfield mask) {
    ANAX_TRACE("glPushClientAttrib");
    if (__proc_glPushClientAttrib == nullptr) {
        __proc_glPushClientAttrib = (__pfn_glPushClientAttrib)GetProcAddress(EnsureRealOpenGL32(), "glPushClientAttrib");
        if (__proc_glPushClientAttrib == nullptr) {
            printf("[opengl32_enh_cpp]   glPushClientAttrib: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPushClientAttrib: resolved OK\n");
        }
    }
    __proc_glPushClientAttrib(mask);
}

typedef void (__stdcall *__pfn_glPushMatrix)(void);
static __pfn_glPushMatrix __proc_glPushMatrix = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPushMatrix(void) {
    ANAX_TRACE("glPushMatrix");
    if (__proc_glPushMatrix == nullptr) {
        __proc_glPushMatrix = (__pfn_glPushMatrix)GetProcAddress(EnsureRealOpenGL32(), "glPushMatrix");
        if (__proc_glPushMatrix == nullptr) {
            printf("[opengl32_enh_cpp]   glPushMatrix: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPushMatrix: resolved OK\n");
        }
    }
    NotifyMatrixPush();
    __proc_glPushMatrix();
}

typedef void (__stdcall *__pfn_glPushName)(GLuint);
static __pfn_glPushName __proc_glPushName = nullptr;

extern "C" __declspec(dllexport) void __stdcall glPushName(GLuint name) {
    ANAX_TRACE("glPushName");
    if (__proc_glPushName == nullptr) {
        __proc_glPushName = (__pfn_glPushName)GetProcAddress(EnsureRealOpenGL32(), "glPushName");
        if (__proc_glPushName == nullptr) {
            printf("[opengl32_enh_cpp]   glPushName: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glPushName: resolved OK\n");
        }
    }
    __proc_glPushName(name);
}

typedef void (__stdcall *__pfn_glRasterPos2d)(GLdouble, GLdouble);
static __pfn_glRasterPos2d __proc_glRasterPos2d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2d(GLdouble x, GLdouble y) {
    ANAX_TRACE("glRasterPos2d");
    if (__proc_glRasterPos2d == nullptr) {
        __proc_glRasterPos2d = (__pfn_glRasterPos2d)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2d");
        if (__proc_glRasterPos2d == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2d: resolved OK\n");
        }
    }
    __proc_glRasterPos2d(x, y);
}

typedef void (__stdcall *__pfn_glRasterPos2dv)(void*);
static __pfn_glRasterPos2dv __proc_glRasterPos2dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2dv(void* v) {
    ANAX_TRACE("glRasterPos2dv");
    if (__proc_glRasterPos2dv == nullptr) {
        __proc_glRasterPos2dv = (__pfn_glRasterPos2dv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2dv");
        if (__proc_glRasterPos2dv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2dv: resolved OK\n");
        }
    }
    __proc_glRasterPos2dv(v);
}

typedef void (__stdcall *__pfn_glRasterPos2f)(GLfloat, GLfloat);
static __pfn_glRasterPos2f __proc_glRasterPos2f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2f(GLfloat x, GLfloat y) {
    ANAX_TRACE("glRasterPos2f");
    if (__proc_glRasterPos2f == nullptr) {
        __proc_glRasterPos2f = (__pfn_glRasterPos2f)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2f");
        if (__proc_glRasterPos2f == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2f: resolved OK\n");
        }
    }
    __proc_glRasterPos2f(x, y);
}

typedef void (__stdcall *__pfn_glRasterPos2fv)(void*);
static __pfn_glRasterPos2fv __proc_glRasterPos2fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2fv(void* v) {
    ANAX_TRACE("glRasterPos2fv");
    if (__proc_glRasterPos2fv == nullptr) {
        __proc_glRasterPos2fv = (__pfn_glRasterPos2fv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2fv");
        if (__proc_glRasterPos2fv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2fv: resolved OK\n");
        }
    }
    __proc_glRasterPos2fv(v);
}

typedef void (__stdcall *__pfn_glRasterPos2i)(GLint, GLint);
static __pfn_glRasterPos2i __proc_glRasterPos2i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2i(GLint x, GLint y) {
    ANAX_TRACE("glRasterPos2i");
    if (__proc_glRasterPos2i == nullptr) {
        __proc_glRasterPos2i = (__pfn_glRasterPos2i)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2i");
        if (__proc_glRasterPos2i == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2i: resolved OK\n");
        }
    }
    __proc_glRasterPos2i(x, y);
}

typedef void (__stdcall *__pfn_glRasterPos2iv)(void*);
static __pfn_glRasterPos2iv __proc_glRasterPos2iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2iv(void* v) {
    ANAX_TRACE("glRasterPos2iv");
    if (__proc_glRasterPos2iv == nullptr) {
        __proc_glRasterPos2iv = (__pfn_glRasterPos2iv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2iv");
        if (__proc_glRasterPos2iv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2iv: resolved OK\n");
        }
    }
    __proc_glRasterPos2iv(v);
}

typedef void (__stdcall *__pfn_glRasterPos2s)(GLshort, GLshort);
static __pfn_glRasterPos2s __proc_glRasterPos2s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2s(GLshort x, GLshort y) {
    ANAX_TRACE("glRasterPos2s");
    if (__proc_glRasterPos2s == nullptr) {
        __proc_glRasterPos2s = (__pfn_glRasterPos2s)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2s");
        if (__proc_glRasterPos2s == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2s: resolved OK\n");
        }
    }
    __proc_glRasterPos2s(x, y);
}

typedef void (__stdcall *__pfn_glRasterPos2sv)(void*);
static __pfn_glRasterPos2sv __proc_glRasterPos2sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos2sv(void* v) {
    ANAX_TRACE("glRasterPos2sv");
    if (__proc_glRasterPos2sv == nullptr) {
        __proc_glRasterPos2sv = (__pfn_glRasterPos2sv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos2sv");
        if (__proc_glRasterPos2sv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos2sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos2sv: resolved OK\n");
        }
    }
    __proc_glRasterPos2sv(v);
}

typedef void (__stdcall *__pfn_glRasterPos3d)(GLdouble, GLdouble, GLdouble);
static __pfn_glRasterPos3d __proc_glRasterPos3d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) {
    ANAX_TRACE("glRasterPos3d");
    if (__proc_glRasterPos3d == nullptr) {
        __proc_glRasterPos3d = (__pfn_glRasterPos3d)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3d");
        if (__proc_glRasterPos3d == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3d: resolved OK\n");
        }
    }
    __proc_glRasterPos3d(x, y, z);
}

typedef void (__stdcall *__pfn_glRasterPos3dv)(void*);
static __pfn_glRasterPos3dv __proc_glRasterPos3dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3dv(void* v) {
    ANAX_TRACE("glRasterPos3dv");
    if (__proc_glRasterPos3dv == nullptr) {
        __proc_glRasterPos3dv = (__pfn_glRasterPos3dv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3dv");
        if (__proc_glRasterPos3dv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3dv: resolved OK\n");
        }
    }
    __proc_glRasterPos3dv(v);
}

typedef void (__stdcall *__pfn_glRasterPos3f)(GLfloat, GLfloat, GLfloat);
static __pfn_glRasterPos3f __proc_glRasterPos3f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) {
    ANAX_TRACE("glRasterPos3f");
    if (__proc_glRasterPos3f == nullptr) {
        __proc_glRasterPos3f = (__pfn_glRasterPos3f)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3f");
        if (__proc_glRasterPos3f == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3f: resolved OK\n");
        }
    }
    __proc_glRasterPos3f(x, y, z);
}

typedef void (__stdcall *__pfn_glRasterPos3fv)(void*);
static __pfn_glRasterPos3fv __proc_glRasterPos3fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3fv(void* v) {
    ANAX_TRACE("glRasterPos3fv");
    if (__proc_glRasterPos3fv == nullptr) {
        __proc_glRasterPos3fv = (__pfn_glRasterPos3fv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3fv");
        if (__proc_glRasterPos3fv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3fv: resolved OK\n");
        }
    }
    __proc_glRasterPos3fv(v);
}

typedef void (__stdcall *__pfn_glRasterPos3i)(GLint, GLint, GLint);
static __pfn_glRasterPos3i __proc_glRasterPos3i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3i(GLint x, GLint y, GLint z) {
    ANAX_TRACE("glRasterPos3i");
    if (__proc_glRasterPos3i == nullptr) {
        __proc_glRasterPos3i = (__pfn_glRasterPos3i)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3i");
        if (__proc_glRasterPos3i == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3i: resolved OK\n");
        }
    }
    __proc_glRasterPos3i(x, y, z);
}

typedef void (__stdcall *__pfn_glRasterPos3iv)(void*);
static __pfn_glRasterPos3iv __proc_glRasterPos3iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3iv(void* v) {
    ANAX_TRACE("glRasterPos3iv");
    if (__proc_glRasterPos3iv == nullptr) {
        __proc_glRasterPos3iv = (__pfn_glRasterPos3iv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3iv");
        if (__proc_glRasterPos3iv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3iv: resolved OK\n");
        }
    }
    __proc_glRasterPos3iv(v);
}

typedef void (__stdcall *__pfn_glRasterPos3s)(GLshort, GLshort, GLshort);
static __pfn_glRasterPos3s __proc_glRasterPos3s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3s(GLshort x, GLshort y, GLshort z) {
    ANAX_TRACE("glRasterPos3s");
    if (__proc_glRasterPos3s == nullptr) {
        __proc_glRasterPos3s = (__pfn_glRasterPos3s)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3s");
        if (__proc_glRasterPos3s == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3s: resolved OK\n");
        }
    }
    __proc_glRasterPos3s(x, y, z);
}

typedef void (__stdcall *__pfn_glRasterPos3sv)(void*);
static __pfn_glRasterPos3sv __proc_glRasterPos3sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos3sv(void* v) {
    ANAX_TRACE("glRasterPos3sv");
    if (__proc_glRasterPos3sv == nullptr) {
        __proc_glRasterPos3sv = (__pfn_glRasterPos3sv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos3sv");
        if (__proc_glRasterPos3sv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos3sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos3sv: resolved OK\n");
        }
    }
    __proc_glRasterPos3sv(v);
}

typedef void (__stdcall *__pfn_glRasterPos4d)(GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glRasterPos4d __proc_glRasterPos4d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    ANAX_TRACE("glRasterPos4d");
    if (__proc_glRasterPos4d == nullptr) {
        __proc_glRasterPos4d = (__pfn_glRasterPos4d)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4d");
        if (__proc_glRasterPos4d == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4d: resolved OK\n");
        }
    }
    __proc_glRasterPos4d(x, y, z, w);
}

typedef void (__stdcall *__pfn_glRasterPos4dv)(void*);
static __pfn_glRasterPos4dv __proc_glRasterPos4dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4dv(void* v) {
    ANAX_TRACE("glRasterPos4dv");
    if (__proc_glRasterPos4dv == nullptr) {
        __proc_glRasterPos4dv = (__pfn_glRasterPos4dv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4dv");
        if (__proc_glRasterPos4dv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4dv: resolved OK\n");
        }
    }
    __proc_glRasterPos4dv(v);
}

typedef void (__stdcall *__pfn_glRasterPos4f)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glRasterPos4f __proc_glRasterPos4f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    ANAX_TRACE("glRasterPos4f");
    if (__proc_glRasterPos4f == nullptr) {
        __proc_glRasterPos4f = (__pfn_glRasterPos4f)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4f");
        if (__proc_glRasterPos4f == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4f: resolved OK\n");
        }
    }
    __proc_glRasterPos4f(x, y, z, w);
}

typedef void (__stdcall *__pfn_glRasterPos4fv)(void*);
static __pfn_glRasterPos4fv __proc_glRasterPos4fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4fv(void* v) {
    ANAX_TRACE("glRasterPos4fv");
    if (__proc_glRasterPos4fv == nullptr) {
        __proc_glRasterPos4fv = (__pfn_glRasterPos4fv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4fv");
        if (__proc_glRasterPos4fv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4fv: resolved OK\n");
        }
    }
    __proc_glRasterPos4fv(v);
}

typedef void (__stdcall *__pfn_glRasterPos4i)(GLint, GLint, GLint, GLint);
static __pfn_glRasterPos4i __proc_glRasterPos4i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4i(GLint x, GLint y, GLint z, GLint w) {
    ANAX_TRACE("glRasterPos4i");
    if (__proc_glRasterPos4i == nullptr) {
        __proc_glRasterPos4i = (__pfn_glRasterPos4i)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4i");
        if (__proc_glRasterPos4i == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4i: resolved OK\n");
        }
    }
    __proc_glRasterPos4i(x, y, z, w);
}

typedef void (__stdcall *__pfn_glRasterPos4iv)(void*);
static __pfn_glRasterPos4iv __proc_glRasterPos4iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4iv(void* v) {
    ANAX_TRACE("glRasterPos4iv");
    if (__proc_glRasterPos4iv == nullptr) {
        __proc_glRasterPos4iv = (__pfn_glRasterPos4iv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4iv");
        if (__proc_glRasterPos4iv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4iv: resolved OK\n");
        }
    }
    __proc_glRasterPos4iv(v);
}

typedef void (__stdcall *__pfn_glRasterPos4s)(GLshort, GLshort, GLshort, GLshort);
static __pfn_glRasterPos4s __proc_glRasterPos4s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    ANAX_TRACE("glRasterPos4s");
    if (__proc_glRasterPos4s == nullptr) {
        __proc_glRasterPos4s = (__pfn_glRasterPos4s)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4s");
        if (__proc_glRasterPos4s == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4s: resolved OK\n");
        }
    }
    __proc_glRasterPos4s(x, y, z, w);
}

typedef void (__stdcall *__pfn_glRasterPos4sv)(void*);
static __pfn_glRasterPos4sv __proc_glRasterPos4sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRasterPos4sv(void* v) {
    ANAX_TRACE("glRasterPos4sv");
    if (__proc_glRasterPos4sv == nullptr) {
        __proc_glRasterPos4sv = (__pfn_glRasterPos4sv)GetProcAddress(EnsureRealOpenGL32(), "glRasterPos4sv");
        if (__proc_glRasterPos4sv == nullptr) {
            printf("[opengl32_enh_cpp]   glRasterPos4sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRasterPos4sv: resolved OK\n");
        }
    }
    __proc_glRasterPos4sv(v);
}

typedef void (__stdcall *__pfn_glReadBuffer)(GLenum);
static __pfn_glReadBuffer __proc_glReadBuffer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glReadBuffer(GLenum mode) {
    ANAX_TRACE("glReadBuffer");
    if (__proc_glReadBuffer == nullptr) {
        __proc_glReadBuffer = (__pfn_glReadBuffer)GetProcAddress(EnsureRealOpenGL32(), "glReadBuffer");
        if (__proc_glReadBuffer == nullptr) {
            printf("[opengl32_enh_cpp]   glReadBuffer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glReadBuffer: resolved OK\n");
        }
    }
    __proc_glReadBuffer(mode);
}

typedef void (__stdcall *__pfn_glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
static __pfn_glReadPixels __proc_glReadPixels = nullptr;

extern "C" __declspec(dllexport) void __stdcall glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glReadPixels");
    if (__proc_glReadPixels == nullptr) {
        __proc_glReadPixels = (__pfn_glReadPixels)GetProcAddress(EnsureRealOpenGL32(), "glReadPixels");
        if (__proc_glReadPixels == nullptr) {
            printf("[opengl32_enh_cpp]   glReadPixels: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glReadPixels: resolved OK\n");
        }
    }
    __proc_glReadPixels(x, y, width, height, format, type, pixels);
}

typedef void (__stdcall *__pfn_glRectd)(GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glRectd __proc_glRectd = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {
    ANAX_TRACE("glRectd");
    if (__proc_glRectd == nullptr) {
        __proc_glRectd = (__pfn_glRectd)GetProcAddress(EnsureRealOpenGL32(), "glRectd");
        if (__proc_glRectd == nullptr) {
            printf("[opengl32_enh_cpp]   glRectd: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRectd: resolved OK\n");
        }
    }
    __proc_glRectd(x1, y1, x2, y2);
}

typedef void (__stdcall *__pfn_glRectdv)(void*, void*);
static __pfn_glRectdv __proc_glRectdv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRectdv(void* v1, void* v2) {
    ANAX_TRACE("glRectdv");
    if (__proc_glRectdv == nullptr) {
        __proc_glRectdv = (__pfn_glRectdv)GetProcAddress(EnsureRealOpenGL32(), "glRectdv");
        if (__proc_glRectdv == nullptr) {
            printf("[opengl32_enh_cpp]   glRectdv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRectdv: resolved OK\n");
        }
    }
    __proc_glRectdv(v1, v2);
}

typedef void (__stdcall *__pfn_glRectf)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glRectf __proc_glRectf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    ANAX_TRACE("glRectf");
    if (__proc_glRectf == nullptr) {
        __proc_glRectf = (__pfn_glRectf)GetProcAddress(EnsureRealOpenGL32(), "glRectf");
        if (__proc_glRectf == nullptr) {
            printf("[opengl32_enh_cpp]   glRectf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRectf: resolved OK\n");
        }
    }
    __proc_glRectf(x1, y1, x2, y2);
}

typedef void (__stdcall *__pfn_glRectfv)(void*, void*);
static __pfn_glRectfv __proc_glRectfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRectfv(void* v1, void* v2) {
    ANAX_TRACE("glRectfv");
    if (__proc_glRectfv == nullptr) {
        __proc_glRectfv = (__pfn_glRectfv)GetProcAddress(EnsureRealOpenGL32(), "glRectfv");
        if (__proc_glRectfv == nullptr) {
            printf("[opengl32_enh_cpp]   glRectfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRectfv: resolved OK\n");
        }
    }
    __proc_glRectfv(v1, v2);
}

typedef void (__stdcall *__pfn_glRecti)(GLint, GLint, GLint, GLint);
static __pfn_glRecti __proc_glRecti = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
    ANAX_TRACE("glRecti");
    if (__proc_glRecti == nullptr) {
        __proc_glRecti = (__pfn_glRecti)GetProcAddress(EnsureRealOpenGL32(), "glRecti");
        if (__proc_glRecti == nullptr) {
            printf("[opengl32_enh_cpp]   glRecti: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRecti: resolved OK\n");
        }
    }
    __proc_glRecti(x1, y1, x2, y2);
}

typedef void (__stdcall *__pfn_glRectiv)(void*, void*);
static __pfn_glRectiv __proc_glRectiv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRectiv(void* v1, void* v2) {
    ANAX_TRACE("glRectiv");
    if (__proc_glRectiv == nullptr) {
        __proc_glRectiv = (__pfn_glRectiv)GetProcAddress(EnsureRealOpenGL32(), "glRectiv");
        if (__proc_glRectiv == nullptr) {
            printf("[opengl32_enh_cpp]   glRectiv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRectiv: resolved OK\n");
        }
    }
    __proc_glRectiv(v1, v2);
}

typedef void (__stdcall *__pfn_glRects)(GLshort, GLshort, GLshort, GLshort);
static __pfn_glRects __proc_glRects = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) {
    ANAX_TRACE("glRects");
    if (__proc_glRects == nullptr) {
        __proc_glRects = (__pfn_glRects)GetProcAddress(EnsureRealOpenGL32(), "glRects");
        if (__proc_glRects == nullptr) {
            printf("[opengl32_enh_cpp]   glRects: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRects: resolved OK\n");
        }
    }
    __proc_glRects(x1, y1, x2, y2);
}

typedef void (__stdcall *__pfn_glRectsv)(void*, void*);
static __pfn_glRectsv __proc_glRectsv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRectsv(void* v1, void* v2) {
    ANAX_TRACE("glRectsv");
    if (__proc_glRectsv == nullptr) {
        __proc_glRectsv = (__pfn_glRectsv)GetProcAddress(EnsureRealOpenGL32(), "glRectsv");
        if (__proc_glRectsv == nullptr) {
            printf("[opengl32_enh_cpp]   glRectsv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRectsv: resolved OK\n");
        }
    }
    __proc_glRectsv(v1, v2);
}

typedef GLint (__stdcall *__pfn_glRenderMode)(GLenum);
static __pfn_glRenderMode __proc_glRenderMode = nullptr;

extern "C" __declspec(dllexport) GLint __stdcall glRenderMode(GLenum mode) {
    ANAX_TRACE("glRenderMode");
    if (__proc_glRenderMode == nullptr) {
        __proc_glRenderMode = (__pfn_glRenderMode)GetProcAddress(EnsureRealOpenGL32(), "glRenderMode");
        if (__proc_glRenderMode == nullptr) {
            printf("[opengl32_enh_cpp]   glRenderMode: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRenderMode: resolved OK\n");
        }
    }
    return __proc_glRenderMode(mode);
}

typedef void (__stdcall *__pfn_glRotated)(GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glRotated __proc_glRotated = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    ANAX_TRACE("glRotated");
    if (__proc_glRotated == nullptr) {
        __proc_glRotated = (__pfn_glRotated)GetProcAddress(EnsureRealOpenGL32(), "glRotated");
        if (__proc_glRotated == nullptr) {
            printf("[opengl32_enh_cpp]   glRotated: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRotated: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glRotated(angle, x, y, z);
}

typedef void (__stdcall *__pfn_glRotatef)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glRotatef __proc_glRotatef = nullptr;

extern "C" __declspec(dllexport) void __stdcall glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    ANAX_TRACE("glRotatef");
    if (__proc_glRotatef == nullptr) {
        __proc_glRotatef = (__pfn_glRotatef)GetProcAddress(EnsureRealOpenGL32(), "glRotatef");
        if (__proc_glRotatef == nullptr) {
            printf("[opengl32_enh_cpp]   glRotatef: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glRotatef: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glRotatef(angle, x, y, z);
}

typedef void (__stdcall *__pfn_glScaled)(GLdouble, GLdouble, GLdouble);
static __pfn_glScaled __proc_glScaled = nullptr;

extern "C" __declspec(dllexport) void __stdcall glScaled(GLdouble x, GLdouble y, GLdouble z) {
    ANAX_TRACE("glScaled");
    if (__proc_glScaled == nullptr) {
        __proc_glScaled = (__pfn_glScaled)GetProcAddress(EnsureRealOpenGL32(), "glScaled");
        if (__proc_glScaled == nullptr) {
            printf("[opengl32_enh_cpp]   glScaled: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glScaled: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glScaled(x, y, z);
}

typedef void (__stdcall *__pfn_glScalef)(GLfloat, GLfloat, GLfloat);
static __pfn_glScalef __proc_glScalef = nullptr;

extern "C" __declspec(dllexport) void __stdcall glScalef(GLfloat x, GLfloat y, GLfloat z) {
    ANAX_TRACE("glScalef");
    if (__proc_glScalef == nullptr) {
        __proc_glScalef = (__pfn_glScalef)GetProcAddress(EnsureRealOpenGL32(), "glScalef");
        if (__proc_glScalef == nullptr) {
            printf("[opengl32_enh_cpp]   glScalef: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glScalef: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glScalef(x, y, z);
}

typedef void (__stdcall *__pfn_glScissor)(GLint, GLint, GLsizei, GLsizei);
static __pfn_glScissor __proc_glScissor = nullptr;

extern "C" __declspec(dllexport) void __stdcall glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    ANAX_TRACE("glScissor");
    if (__proc_glScissor == nullptr) {
        __proc_glScissor = (__pfn_glScissor)GetProcAddress(EnsureRealOpenGL32(), "glScissor");
        if (__proc_glScissor == nullptr) {
            printf("[opengl32_enh_cpp]   glScissor: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glScissor: resolved OK\n");
        }
    }
    __proc_glScissor(x, y, width, height);
}

typedef void (__stdcall *__pfn_glSelectBuffer)(GLsizei, void*);
static __pfn_glSelectBuffer __proc_glSelectBuffer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glSelectBuffer(GLsizei size, void* buffer) {
    ANAX_TRACE("glSelectBuffer");
    if (__proc_glSelectBuffer == nullptr) {
        __proc_glSelectBuffer = (__pfn_glSelectBuffer)GetProcAddress(EnsureRealOpenGL32(), "glSelectBuffer");
        if (__proc_glSelectBuffer == nullptr) {
            printf("[opengl32_enh_cpp]   glSelectBuffer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glSelectBuffer: resolved OK\n");
        }
    }
    __proc_glSelectBuffer(size, buffer);
}

typedef void (__stdcall *__pfn_glShadeModel)(GLenum);
static __pfn_glShadeModel __proc_glShadeModel = nullptr;

extern "C" __declspec(dllexport) void __stdcall glShadeModel(GLenum mode) {
    ANAX_TRACE("glShadeModel");
    if (__proc_glShadeModel == nullptr) {
        __proc_glShadeModel = (__pfn_glShadeModel)GetProcAddress(EnsureRealOpenGL32(), "glShadeModel");
        if (__proc_glShadeModel == nullptr) {
            printf("[opengl32_enh_cpp]   glShadeModel: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glShadeModel: resolved OK\n");
        }
    }
    __proc_glShadeModel(mode);
}

typedef void (__stdcall *__pfn_glStencilFunc)(GLenum, GLint, GLuint);
static __pfn_glStencilFunc __proc_glStencilFunc = nullptr;

extern "C" __declspec(dllexport) void __stdcall glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    ANAX_TRACE("glStencilFunc");
    if (__proc_glStencilFunc == nullptr) {
        __proc_glStencilFunc = (__pfn_glStencilFunc)GetProcAddress(EnsureRealOpenGL32(), "glStencilFunc");
        if (__proc_glStencilFunc == nullptr) {
            printf("[opengl32_enh_cpp]   glStencilFunc: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glStencilFunc: resolved OK\n");
        }
    }
    __proc_glStencilFunc(func, ref, mask);
}

typedef void (__stdcall *__pfn_glStencilMask)(GLuint);
static __pfn_glStencilMask __proc_glStencilMask = nullptr;

extern "C" __declspec(dllexport) void __stdcall glStencilMask(GLuint mask) {
    ANAX_TRACE("glStencilMask");
    if (__proc_glStencilMask == nullptr) {
        __proc_glStencilMask = (__pfn_glStencilMask)GetProcAddress(EnsureRealOpenGL32(), "glStencilMask");
        if (__proc_glStencilMask == nullptr) {
            printf("[opengl32_enh_cpp]   glStencilMask: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glStencilMask: resolved OK\n");
        }
    }
    __proc_glStencilMask(mask);
}

typedef void (__stdcall *__pfn_glStencilOp)(GLenum, GLenum, GLenum);
static __pfn_glStencilOp __proc_glStencilOp = nullptr;

extern "C" __declspec(dllexport) void __stdcall glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    ANAX_TRACE("glStencilOp");
    if (__proc_glStencilOp == nullptr) {
        __proc_glStencilOp = (__pfn_glStencilOp)GetProcAddress(EnsureRealOpenGL32(), "glStencilOp");
        if (__proc_glStencilOp == nullptr) {
            printf("[opengl32_enh_cpp]   glStencilOp: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glStencilOp: resolved OK\n");
        }
    }
    __proc_glStencilOp(fail, zfail, zpass);
}

typedef void (__stdcall *__pfn_glTexCoord1d)(GLdouble);
static __pfn_glTexCoord1d __proc_glTexCoord1d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1d(GLdouble s) {
    ANAX_TRACE("glTexCoord1d");
    if (__proc_glTexCoord1d == nullptr) {
        __proc_glTexCoord1d = (__pfn_glTexCoord1d)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1d");
        if (__proc_glTexCoord1d == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1d: resolved OK\n");
        }
    }
    __proc_glTexCoord1d(s);
}

typedef void (__stdcall *__pfn_glTexCoord1dv)(void*);
static __pfn_glTexCoord1dv __proc_glTexCoord1dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1dv(void* v) {
    ANAX_TRACE("glTexCoord1dv");
    if (__proc_glTexCoord1dv == nullptr) {
        __proc_glTexCoord1dv = (__pfn_glTexCoord1dv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1dv");
        if (__proc_glTexCoord1dv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1dv: resolved OK\n");
        }
    }
    __proc_glTexCoord1dv(v);
}

typedef void (__stdcall *__pfn_glTexCoord1f)(GLfloat);
static __pfn_glTexCoord1f __proc_glTexCoord1f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1f(GLfloat s) {
    ANAX_TRACE("glTexCoord1f");
    if (__proc_glTexCoord1f == nullptr) {
        __proc_glTexCoord1f = (__pfn_glTexCoord1f)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1f");
        if (__proc_glTexCoord1f == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1f: resolved OK\n");
        }
    }
    __proc_glTexCoord1f(s);
}

typedef void (__stdcall *__pfn_glTexCoord1fv)(void*);
static __pfn_glTexCoord1fv __proc_glTexCoord1fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1fv(void* v) {
    ANAX_TRACE("glTexCoord1fv");
    if (__proc_glTexCoord1fv == nullptr) {
        __proc_glTexCoord1fv = (__pfn_glTexCoord1fv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1fv");
        if (__proc_glTexCoord1fv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1fv: resolved OK\n");
        }
    }
    __proc_glTexCoord1fv(v);
}

typedef void (__stdcall *__pfn_glTexCoord1i)(GLint);
static __pfn_glTexCoord1i __proc_glTexCoord1i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1i(GLint s) {
    ANAX_TRACE("glTexCoord1i");
    if (__proc_glTexCoord1i == nullptr) {
        __proc_glTexCoord1i = (__pfn_glTexCoord1i)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1i");
        if (__proc_glTexCoord1i == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1i: resolved OK\n");
        }
    }
    __proc_glTexCoord1i(s);
}

typedef void (__stdcall *__pfn_glTexCoord1iv)(void*);
static __pfn_glTexCoord1iv __proc_glTexCoord1iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1iv(void* v) {
    ANAX_TRACE("glTexCoord1iv");
    if (__proc_glTexCoord1iv == nullptr) {
        __proc_glTexCoord1iv = (__pfn_glTexCoord1iv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1iv");
        if (__proc_glTexCoord1iv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1iv: resolved OK\n");
        }
    }
    __proc_glTexCoord1iv(v);
}

typedef void (__stdcall *__pfn_glTexCoord1s)(GLshort);
static __pfn_glTexCoord1s __proc_glTexCoord1s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1s(GLshort s) {
    ANAX_TRACE("glTexCoord1s");
    if (__proc_glTexCoord1s == nullptr) {
        __proc_glTexCoord1s = (__pfn_glTexCoord1s)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1s");
        if (__proc_glTexCoord1s == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1s: resolved OK\n");
        }
    }
    __proc_glTexCoord1s(s);
}

typedef void (__stdcall *__pfn_glTexCoord1sv)(void*);
static __pfn_glTexCoord1sv __proc_glTexCoord1sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord1sv(void* v) {
    ANAX_TRACE("glTexCoord1sv");
    if (__proc_glTexCoord1sv == nullptr) {
        __proc_glTexCoord1sv = (__pfn_glTexCoord1sv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord1sv");
        if (__proc_glTexCoord1sv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord1sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord1sv: resolved OK\n");
        }
    }
    __proc_glTexCoord1sv(v);
}

typedef void (__stdcall *__pfn_glTexCoord2d)(GLdouble, GLdouble);
static __pfn_glTexCoord2d __proc_glTexCoord2d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2d(GLdouble s, GLdouble t) {
    ANAX_TRACE("glTexCoord2d");
    if (__proc_glTexCoord2d == nullptr) {
        __proc_glTexCoord2d = (__pfn_glTexCoord2d)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2d");
        if (__proc_glTexCoord2d == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2d: resolved OK\n");
        }
    }
    __proc_glTexCoord2d(s, t);
}

typedef void (__stdcall *__pfn_glTexCoord2dv)(void*);
static __pfn_glTexCoord2dv __proc_glTexCoord2dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2dv(void* v) {
    ANAX_TRACE("glTexCoord2dv");
    if (__proc_glTexCoord2dv == nullptr) {
        __proc_glTexCoord2dv = (__pfn_glTexCoord2dv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2dv");
        if (__proc_glTexCoord2dv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2dv: resolved OK\n");
        }
    }
    __proc_glTexCoord2dv(v);
}

typedef void (__stdcall *__pfn_glTexCoord2f)(GLfloat, GLfloat);
static __pfn_glTexCoord2f __proc_glTexCoord2f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2f(GLfloat s, GLfloat t) {
    ANAX_TRACE("glTexCoord2f");
    if (__proc_glTexCoord2f == nullptr) {
        __proc_glTexCoord2f = (__pfn_glTexCoord2f)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2f");
        if (__proc_glTexCoord2f == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2f: resolved OK\n");
        }
    }
    __proc_glTexCoord2f(s, t);
}

typedef void (__stdcall *__pfn_glTexCoord2fv)(void*);
static __pfn_glTexCoord2fv __proc_glTexCoord2fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2fv(void* v) {
    ANAX_TRACE("glTexCoord2fv");
    if (__proc_glTexCoord2fv == nullptr) {
        __proc_glTexCoord2fv = (__pfn_glTexCoord2fv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2fv");
        if (__proc_glTexCoord2fv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2fv: resolved OK\n");
        }
    }
    __proc_glTexCoord2fv(v);
}

typedef void (__stdcall *__pfn_glTexCoord2i)(GLint, GLint);
static __pfn_glTexCoord2i __proc_glTexCoord2i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2i(GLint s, GLint t) {
    ANAX_TRACE("glTexCoord2i");
    if (__proc_glTexCoord2i == nullptr) {
        __proc_glTexCoord2i = (__pfn_glTexCoord2i)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2i");
        if (__proc_glTexCoord2i == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2i: resolved OK\n");
        }
    }
    __proc_glTexCoord2i(s, t);
}

typedef void (__stdcall *__pfn_glTexCoord2iv)(void*);
static __pfn_glTexCoord2iv __proc_glTexCoord2iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2iv(void* v) {
    ANAX_TRACE("glTexCoord2iv");
    if (__proc_glTexCoord2iv == nullptr) {
        __proc_glTexCoord2iv = (__pfn_glTexCoord2iv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2iv");
        if (__proc_glTexCoord2iv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2iv: resolved OK\n");
        }
    }
    __proc_glTexCoord2iv(v);
}

typedef void (__stdcall *__pfn_glTexCoord2s)(GLshort, GLshort);
static __pfn_glTexCoord2s __proc_glTexCoord2s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2s(GLshort s, GLshort t) {
    ANAX_TRACE("glTexCoord2s");
    if (__proc_glTexCoord2s == nullptr) {
        __proc_glTexCoord2s = (__pfn_glTexCoord2s)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2s");
        if (__proc_glTexCoord2s == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2s: resolved OK\n");
        }
    }
    __proc_glTexCoord2s(s, t);
}

typedef void (__stdcall *__pfn_glTexCoord2sv)(void*);
static __pfn_glTexCoord2sv __proc_glTexCoord2sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord2sv(void* v) {
    ANAX_TRACE("glTexCoord2sv");
    if (__proc_glTexCoord2sv == nullptr) {
        __proc_glTexCoord2sv = (__pfn_glTexCoord2sv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord2sv");
        if (__proc_glTexCoord2sv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord2sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord2sv: resolved OK\n");
        }
    }
    __proc_glTexCoord2sv(v);
}

typedef void (__stdcall *__pfn_glTexCoord3d)(GLdouble, GLdouble, GLdouble);
static __pfn_glTexCoord3d __proc_glTexCoord3d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) {
    ANAX_TRACE("glTexCoord3d");
    if (__proc_glTexCoord3d == nullptr) {
        __proc_glTexCoord3d = (__pfn_glTexCoord3d)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3d");
        if (__proc_glTexCoord3d == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3d: resolved OK\n");
        }
    }
    __proc_glTexCoord3d(s, t, r);
}

typedef void (__stdcall *__pfn_glTexCoord3dv)(void*);
static __pfn_glTexCoord3dv __proc_glTexCoord3dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3dv(void* v) {
    ANAX_TRACE("glTexCoord3dv");
    if (__proc_glTexCoord3dv == nullptr) {
        __proc_glTexCoord3dv = (__pfn_glTexCoord3dv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3dv");
        if (__proc_glTexCoord3dv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3dv: resolved OK\n");
        }
    }
    __proc_glTexCoord3dv(v);
}

typedef void (__stdcall *__pfn_glTexCoord3f)(GLfloat, GLfloat, GLfloat);
static __pfn_glTexCoord3f __proc_glTexCoord3f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
    ANAX_TRACE("glTexCoord3f");
    if (__proc_glTexCoord3f == nullptr) {
        __proc_glTexCoord3f = (__pfn_glTexCoord3f)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3f");
        if (__proc_glTexCoord3f == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3f: resolved OK\n");
        }
    }
    __proc_glTexCoord3f(s, t, r);
}

typedef void (__stdcall *__pfn_glTexCoord3fv)(void*);
static __pfn_glTexCoord3fv __proc_glTexCoord3fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3fv(void* v) {
    ANAX_TRACE("glTexCoord3fv");
    if (__proc_glTexCoord3fv == nullptr) {
        __proc_glTexCoord3fv = (__pfn_glTexCoord3fv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3fv");
        if (__proc_glTexCoord3fv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3fv: resolved OK\n");
        }
    }
    __proc_glTexCoord3fv(v);
}

typedef void (__stdcall *__pfn_glTexCoord3i)(GLint, GLint, GLint);
static __pfn_glTexCoord3i __proc_glTexCoord3i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3i(GLint s, GLint t, GLint r) {
    ANAX_TRACE("glTexCoord3i");
    if (__proc_glTexCoord3i == nullptr) {
        __proc_glTexCoord3i = (__pfn_glTexCoord3i)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3i");
        if (__proc_glTexCoord3i == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3i: resolved OK\n");
        }
    }
    __proc_glTexCoord3i(s, t, r);
}

typedef void (__stdcall *__pfn_glTexCoord3iv)(void*);
static __pfn_glTexCoord3iv __proc_glTexCoord3iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3iv(void* v) {
    ANAX_TRACE("glTexCoord3iv");
    if (__proc_glTexCoord3iv == nullptr) {
        __proc_glTexCoord3iv = (__pfn_glTexCoord3iv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3iv");
        if (__proc_glTexCoord3iv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3iv: resolved OK\n");
        }
    }
    __proc_glTexCoord3iv(v);
}

typedef void (__stdcall *__pfn_glTexCoord3s)(GLshort, GLshort, GLshort);
static __pfn_glTexCoord3s __proc_glTexCoord3s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3s(GLshort s, GLshort t, GLshort r) {
    ANAX_TRACE("glTexCoord3s");
    if (__proc_glTexCoord3s == nullptr) {
        __proc_glTexCoord3s = (__pfn_glTexCoord3s)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3s");
        if (__proc_glTexCoord3s == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3s: resolved OK\n");
        }
    }
    __proc_glTexCoord3s(s, t, r);
}

typedef void (__stdcall *__pfn_glTexCoord3sv)(void*);
static __pfn_glTexCoord3sv __proc_glTexCoord3sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord3sv(void* v) {
    ANAX_TRACE("glTexCoord3sv");
    if (__proc_glTexCoord3sv == nullptr) {
        __proc_glTexCoord3sv = (__pfn_glTexCoord3sv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord3sv");
        if (__proc_glTexCoord3sv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord3sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord3sv: resolved OK\n");
        }
    }
    __proc_glTexCoord3sv(v);
}

typedef void (__stdcall *__pfn_glTexCoord4d)(GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glTexCoord4d __proc_glTexCoord4d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    ANAX_TRACE("glTexCoord4d");
    if (__proc_glTexCoord4d == nullptr) {
        __proc_glTexCoord4d = (__pfn_glTexCoord4d)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4d");
        if (__proc_glTexCoord4d == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4d: resolved OK\n");
        }
    }
    __proc_glTexCoord4d(s, t, r, q);
}

typedef void (__stdcall *__pfn_glTexCoord4dv)(void*);
static __pfn_glTexCoord4dv __proc_glTexCoord4dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4dv(void* v) {
    ANAX_TRACE("glTexCoord4dv");
    if (__proc_glTexCoord4dv == nullptr) {
        __proc_glTexCoord4dv = (__pfn_glTexCoord4dv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4dv");
        if (__proc_glTexCoord4dv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4dv: resolved OK\n");
        }
    }
    __proc_glTexCoord4dv(v);
}

typedef void (__stdcall *__pfn_glTexCoord4f)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glTexCoord4f __proc_glTexCoord4f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    ANAX_TRACE("glTexCoord4f");
    if (__proc_glTexCoord4f == nullptr) {
        __proc_glTexCoord4f = (__pfn_glTexCoord4f)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4f");
        if (__proc_glTexCoord4f == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4f: resolved OK\n");
        }
    }
    __proc_glTexCoord4f(s, t, r, q);
}

typedef void (__stdcall *__pfn_glTexCoord4fv)(void*);
static __pfn_glTexCoord4fv __proc_glTexCoord4fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4fv(void* v) {
    ANAX_TRACE("glTexCoord4fv");
    if (__proc_glTexCoord4fv == nullptr) {
        __proc_glTexCoord4fv = (__pfn_glTexCoord4fv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4fv");
        if (__proc_glTexCoord4fv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4fv: resolved OK\n");
        }
    }
    __proc_glTexCoord4fv(v);
}

typedef void (__stdcall *__pfn_glTexCoord4i)(GLint, GLint, GLint, GLint);
static __pfn_glTexCoord4i __proc_glTexCoord4i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4i(GLint s, GLint t, GLint r, GLint q) {
    ANAX_TRACE("glTexCoord4i");
    if (__proc_glTexCoord4i == nullptr) {
        __proc_glTexCoord4i = (__pfn_glTexCoord4i)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4i");
        if (__proc_glTexCoord4i == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4i: resolved OK\n");
        }
    }
    __proc_glTexCoord4i(s, t, r, q);
}

typedef void (__stdcall *__pfn_glTexCoord4iv)(void*);
static __pfn_glTexCoord4iv __proc_glTexCoord4iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4iv(void* v) {
    ANAX_TRACE("glTexCoord4iv");
    if (__proc_glTexCoord4iv == nullptr) {
        __proc_glTexCoord4iv = (__pfn_glTexCoord4iv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4iv");
        if (__proc_glTexCoord4iv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4iv: resolved OK\n");
        }
    }
    __proc_glTexCoord4iv(v);
}

typedef void (__stdcall *__pfn_glTexCoord4s)(GLshort, GLshort, GLshort, GLshort);
static __pfn_glTexCoord4s __proc_glTexCoord4s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) {
    ANAX_TRACE("glTexCoord4s");
    if (__proc_glTexCoord4s == nullptr) {
        __proc_glTexCoord4s = (__pfn_glTexCoord4s)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4s");
        if (__proc_glTexCoord4s == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4s: resolved OK\n");
        }
    }
    __proc_glTexCoord4s(s, t, r, q);
}

typedef void (__stdcall *__pfn_glTexCoord4sv)(void*);
static __pfn_glTexCoord4sv __proc_glTexCoord4sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoord4sv(void* v) {
    ANAX_TRACE("glTexCoord4sv");
    if (__proc_glTexCoord4sv == nullptr) {
        __proc_glTexCoord4sv = (__pfn_glTexCoord4sv)GetProcAddress(EnsureRealOpenGL32(), "glTexCoord4sv");
        if (__proc_glTexCoord4sv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoord4sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoord4sv: resolved OK\n");
        }
    }
    __proc_glTexCoord4sv(v);
}

typedef void (__stdcall *__pfn_glTexCoordPointer)(GLint, GLenum, GLsizei, void*);
static __pfn_glTexCoordPointer __proc_glTexCoordPointer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexCoordPointer(GLint size, GLenum type, GLsizei stride, void* pointer) {
    ANAX_TRACE("glTexCoordPointer");
    if (__proc_glTexCoordPointer == nullptr) {
        __proc_glTexCoordPointer = (__pfn_glTexCoordPointer)GetProcAddress(EnsureRealOpenGL32(), "glTexCoordPointer");
        if (__proc_glTexCoordPointer == nullptr) {
            printf("[opengl32_enh_cpp]   glTexCoordPointer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexCoordPointer: resolved OK\n");
        }
    }
    __proc_glTexCoordPointer(size, type, stride, pointer);
}

typedef void (__stdcall *__pfn_glTexEnvf)(GLenum, GLenum, GLfloat);
static __pfn_glTexEnvf __proc_glTexEnvf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    ANAX_TRACE("glTexEnvf");
    if (__proc_glTexEnvf == nullptr) {
        __proc_glTexEnvf = (__pfn_glTexEnvf)GetProcAddress(EnsureRealOpenGL32(), "glTexEnvf");
        if (__proc_glTexEnvf == nullptr) {
            printf("[opengl32_enh_cpp]   glTexEnvf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexEnvf: resolved OK\n");
        }
    }
    __proc_glTexEnvf(target, pname, param);
}

typedef void (__stdcall *__pfn_glTexEnvfv)(GLenum, GLenum, void*);
static __pfn_glTexEnvfv __proc_glTexEnvfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexEnvfv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glTexEnvfv");
    if (__proc_glTexEnvfv == nullptr) {
        __proc_glTexEnvfv = (__pfn_glTexEnvfv)GetProcAddress(EnsureRealOpenGL32(), "glTexEnvfv");
        if (__proc_glTexEnvfv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexEnvfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexEnvfv: resolved OK\n");
        }
    }
    __proc_glTexEnvfv(target, pname, params);
}

typedef void (__stdcall *__pfn_glTexEnvi)(GLenum, GLenum, GLint);
static __pfn_glTexEnvi __proc_glTexEnvi = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexEnvi(GLenum target, GLenum pname, GLint param) {
    ANAX_TRACE("glTexEnvi");
    if (__proc_glTexEnvi == nullptr) {
        __proc_glTexEnvi = (__pfn_glTexEnvi)GetProcAddress(EnsureRealOpenGL32(), "glTexEnvi");
        if (__proc_glTexEnvi == nullptr) {
            printf("[opengl32_enh_cpp]   glTexEnvi: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexEnvi: resolved OK\n");
        }
    }
    __proc_glTexEnvi(target, pname, param);
}

typedef void (__stdcall *__pfn_glTexEnviv)(GLenum, GLenum, void*);
static __pfn_glTexEnviv __proc_glTexEnviv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexEnviv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glTexEnviv");
    if (__proc_glTexEnviv == nullptr) {
        __proc_glTexEnviv = (__pfn_glTexEnviv)GetProcAddress(EnsureRealOpenGL32(), "glTexEnviv");
        if (__proc_glTexEnviv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexEnviv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexEnviv: resolved OK\n");
        }
    }
    __proc_glTexEnviv(target, pname, params);
}

typedef void (__stdcall *__pfn_glTexGend)(GLenum, GLenum, GLdouble);
static __pfn_glTexGend __proc_glTexGend = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexGend(GLenum coord, GLenum pname, GLdouble param) {
    ANAX_TRACE("glTexGend");
    if (__proc_glTexGend == nullptr) {
        __proc_glTexGend = (__pfn_glTexGend)GetProcAddress(EnsureRealOpenGL32(), "glTexGend");
        if (__proc_glTexGend == nullptr) {
            printf("[opengl32_enh_cpp]   glTexGend: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexGend: resolved OK\n");
        }
    }
    __proc_glTexGend(coord, pname, param);
}

typedef void (__stdcall *__pfn_glTexGendv)(GLenum, GLenum, void*);
static __pfn_glTexGendv __proc_glTexGendv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexGendv(GLenum coord, GLenum pname, void* params) {
    ANAX_TRACE("glTexGendv");
    if (__proc_glTexGendv == nullptr) {
        __proc_glTexGendv = (__pfn_glTexGendv)GetProcAddress(EnsureRealOpenGL32(), "glTexGendv");
        if (__proc_glTexGendv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexGendv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexGendv: resolved OK\n");
        }
    }
    __proc_glTexGendv(coord, pname, params);
}

typedef void (__stdcall *__pfn_glTexGenf)(GLenum, GLenum, GLfloat);
static __pfn_glTexGenf __proc_glTexGenf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexGenf(GLenum coord, GLenum pname, GLfloat param) {
    ANAX_TRACE("glTexGenf");
    if (__proc_glTexGenf == nullptr) {
        __proc_glTexGenf = (__pfn_glTexGenf)GetProcAddress(EnsureRealOpenGL32(), "glTexGenf");
        if (__proc_glTexGenf == nullptr) {
            printf("[opengl32_enh_cpp]   glTexGenf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexGenf: resolved OK\n");
        }
    }
    __proc_glTexGenf(coord, pname, param);
}

typedef void (__stdcall *__pfn_glTexGenfv)(GLenum, GLenum, void*);
static __pfn_glTexGenfv __proc_glTexGenfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexGenfv(GLenum coord, GLenum pname, void* params) {
    ANAX_TRACE("glTexGenfv");
    if (__proc_glTexGenfv == nullptr) {
        __proc_glTexGenfv = (__pfn_glTexGenfv)GetProcAddress(EnsureRealOpenGL32(), "glTexGenfv");
        if (__proc_glTexGenfv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexGenfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexGenfv: resolved OK\n");
        }
    }
    __proc_glTexGenfv(coord, pname, params);
}

typedef void (__stdcall *__pfn_glTexGeni)(GLenum, GLenum, GLint);
static __pfn_glTexGeni __proc_glTexGeni = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexGeni(GLenum coord, GLenum pname, GLint param) {
    ANAX_TRACE("glTexGeni");
    if (__proc_glTexGeni == nullptr) {
        __proc_glTexGeni = (__pfn_glTexGeni)GetProcAddress(EnsureRealOpenGL32(), "glTexGeni");
        if (__proc_glTexGeni == nullptr) {
            printf("[opengl32_enh_cpp]   glTexGeni: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexGeni: resolved OK\n");
        }
    }
    __proc_glTexGeni(coord, pname, param);
}

typedef void (__stdcall *__pfn_glTexGeniv)(GLenum, GLenum, void*);
static __pfn_glTexGeniv __proc_glTexGeniv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexGeniv(GLenum coord, GLenum pname, void* params) {
    ANAX_TRACE("glTexGeniv");
    if (__proc_glTexGeniv == nullptr) {
        __proc_glTexGeniv = (__pfn_glTexGeniv)GetProcAddress(EnsureRealOpenGL32(), "glTexGeniv");
        if (__proc_glTexGeniv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexGeniv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexGeniv: resolved OK\n");
        }
    }
    __proc_glTexGeniv(coord, pname, params);
}

typedef void (__stdcall *__pfn_glTexImage1D)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, void*);
static __pfn_glTexImage1D __proc_glTexImage1D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glTexImage1D");
    if (__proc_glTexImage1D == nullptr) {
        __proc_glTexImage1D = (__pfn_glTexImage1D)GetProcAddress(EnsureRealOpenGL32(), "glTexImage1D");
        if (__proc_glTexImage1D == nullptr) {
            printf("[opengl32_enh_cpp]   glTexImage1D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexImage1D: resolved OK\n");
        }
    }
    __proc_glTexImage1D(target, level, internalformat, width, border, format, type, pixels);
}

typedef void (__stdcall *__pfn_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, void*);
static __pfn_glTexImage2D __proc_glTexImage2D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glTexImage2D");
    if (__proc_glTexImage2D == nullptr) {
        __proc_glTexImage2D = (__pfn_glTexImage2D)GetProcAddress(EnsureRealOpenGL32(), "glTexImage2D");
        if (__proc_glTexImage2D == nullptr) {
            printf("[opengl32_enh_cpp]   glTexImage2D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexImage2D: resolved OK\n");
        }
    }
    ApplyTextureEffectUpload(__proc_glTexImage2D, target, level, internalformat, width, height, border, format, type, pixels);
}

typedef void (__stdcall *__pfn_glTexParameterf)(GLenum, GLenum, GLfloat);
static __pfn_glTexParameterf __proc_glTexParameterf = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    ANAX_TRACE("glTexParameterf");
    if (__proc_glTexParameterf == nullptr) {
        __proc_glTexParameterf = (__pfn_glTexParameterf)GetProcAddress(EnsureRealOpenGL32(), "glTexParameterf");
        if (__proc_glTexParameterf == nullptr) {
            printf("[opengl32_enh_cpp]   glTexParameterf: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexParameterf: resolved OK\n");
        }
    }
    ApplyTextureFilterOverrideF(__proc_glTexParameterf, target, pname, param);
}

typedef void (__stdcall *__pfn_glTexParameterfv)(GLenum, GLenum, void*);
static __pfn_glTexParameterfv __proc_glTexParameterfv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexParameterfv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glTexParameterfv");
    if (__proc_glTexParameterfv == nullptr) {
        __proc_glTexParameterfv = (__pfn_glTexParameterfv)GetProcAddress(EnsureRealOpenGL32(), "glTexParameterfv");
        if (__proc_glTexParameterfv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexParameterfv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexParameterfv: resolved OK\n");
        }
    }
    __proc_glTexParameterfv(target, pname, params);
}

typedef void (__stdcall *__pfn_glTexParameteri)(GLenum, GLenum, GLint);
static __pfn_glTexParameteri __proc_glTexParameteri = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexParameteri(GLenum target, GLenum pname, GLint param) {
    ANAX_TRACE("glTexParameteri");
    if (__proc_glTexParameteri == nullptr) {
        __proc_glTexParameteri = (__pfn_glTexParameteri)GetProcAddress(EnsureRealOpenGL32(), "glTexParameteri");
        if (__proc_glTexParameteri == nullptr) {
            printf("[opengl32_enh_cpp]   glTexParameteri: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexParameteri: resolved OK\n");
        }
    }
    ApplyTextureFilterOverride(__proc_glTexParameteri, target, pname, param);
}

typedef void (__stdcall *__pfn_glTexParameteriv)(GLenum, GLenum, void*);
static __pfn_glTexParameteriv __proc_glTexParameteriv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexParameteriv(GLenum target, GLenum pname, void* params) {
    ANAX_TRACE("glTexParameteriv");
    if (__proc_glTexParameteriv == nullptr) {
        __proc_glTexParameteriv = (__pfn_glTexParameteriv)GetProcAddress(EnsureRealOpenGL32(), "glTexParameteriv");
        if (__proc_glTexParameteriv == nullptr) {
            printf("[opengl32_enh_cpp]   glTexParameteriv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexParameteriv: resolved OK\n");
        }
    }
    __proc_glTexParameteriv(target, pname, params);
}

typedef void (__stdcall *__pfn_glTexSubImage1D)(GLenum, GLint, GLint, GLsizei, GLenum, GLenum, void*);
static __pfn_glTexSubImage1D __proc_glTexSubImage1D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glTexSubImage1D");
    if (__proc_glTexSubImage1D == nullptr) {
        __proc_glTexSubImage1D = (__pfn_glTexSubImage1D)GetProcAddress(EnsureRealOpenGL32(), "glTexSubImage1D");
        if (__proc_glTexSubImage1D == nullptr) {
            printf("[opengl32_enh_cpp]   glTexSubImage1D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexSubImage1D: resolved OK\n");
        }
    }
    __proc_glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

typedef void (__stdcall *__pfn_glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
static __pfn_glTexSubImage2D __proc_glTexSubImage2D = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    ANAX_TRACE("glTexSubImage2D");
    if (__proc_glTexSubImage2D == nullptr) {
        __proc_glTexSubImage2D = (__pfn_glTexSubImage2D)GetProcAddress(EnsureRealOpenGL32(), "glTexSubImage2D");
        if (__proc_glTexSubImage2D == nullptr) {
            printf("[opengl32_enh_cpp]   glTexSubImage2D: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTexSubImage2D: resolved OK\n");
        }
    }
    __proc_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

typedef void (__stdcall *__pfn_glTranslated)(GLdouble, GLdouble, GLdouble);
static __pfn_glTranslated __proc_glTranslated = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    ANAX_TRACE("glTranslated");
    if (__proc_glTranslated == nullptr) {
        __proc_glTranslated = (__pfn_glTranslated)GetProcAddress(EnsureRealOpenGL32(), "glTranslated");
        if (__proc_glTranslated == nullptr) {
            printf("[opengl32_enh_cpp]   glTranslated: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTranslated: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glTranslated(x, y, z);
}

typedef void (__stdcall *__pfn_glTranslatef)(GLfloat, GLfloat, GLfloat);
static __pfn_glTranslatef __proc_glTranslatef = nullptr;

extern "C" __declspec(dllexport) void __stdcall glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    ANAX_TRACE("glTranslatef");
    if (__proc_glTranslatef == nullptr) {
        __proc_glTranslatef = (__pfn_glTranslatef)GetProcAddress(EnsureRealOpenGL32(), "glTranslatef");
        if (__proc_glTranslatef == nullptr) {
            printf("[opengl32_enh_cpp]   glTranslatef: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glTranslatef: resolved OK\n");
        }
    }
    NotifyMatrixEdited();
    __proc_glTranslatef(x, y, z);
}

typedef void (__stdcall *__pfn_glVertex2d)(GLdouble, GLdouble);
static __pfn_glVertex2d __proc_glVertex2d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2d(GLdouble x, GLdouble y) {
    ANAX_TRACE("glVertex2d");
    if (__proc_glVertex2d == nullptr) {
        __proc_glVertex2d = (__pfn_glVertex2d)GetProcAddress(EnsureRealOpenGL32(), "glVertex2d");
        if (__proc_glVertex2d == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2d: resolved OK\n");
        }
    }
    __proc_glVertex2d(x, y);
}

typedef void (__stdcall *__pfn_glVertex2dv)(void*);
static __pfn_glVertex2dv __proc_glVertex2dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2dv(void* v) {
    ANAX_TRACE("glVertex2dv");
    if (__proc_glVertex2dv == nullptr) {
        __proc_glVertex2dv = (__pfn_glVertex2dv)GetProcAddress(EnsureRealOpenGL32(), "glVertex2dv");
        if (__proc_glVertex2dv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2dv: resolved OK\n");
        }
    }
    __proc_glVertex2dv(v);
}

typedef void (__stdcall *__pfn_glVertex2f)(GLfloat, GLfloat);
static __pfn_glVertex2f __proc_glVertex2f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2f(GLfloat x, GLfloat y) {
    ANAX_TRACE("glVertex2f");
    if (__proc_glVertex2f == nullptr) {
        __proc_glVertex2f = (__pfn_glVertex2f)GetProcAddress(EnsureRealOpenGL32(), "glVertex2f");
        if (__proc_glVertex2f == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2f: resolved OK\n");
        }
    }
    __proc_glVertex2f(x, y);
}

typedef void (__stdcall *__pfn_glVertex2fv)(void*);
static __pfn_glVertex2fv __proc_glVertex2fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2fv(void* v) {
    ANAX_TRACE("glVertex2fv");
    if (__proc_glVertex2fv == nullptr) {
        __proc_glVertex2fv = (__pfn_glVertex2fv)GetProcAddress(EnsureRealOpenGL32(), "glVertex2fv");
        if (__proc_glVertex2fv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2fv: resolved OK\n");
        }
    }
    __proc_glVertex2fv(v);
}

typedef void (__stdcall *__pfn_glVertex2i)(GLint, GLint);
static __pfn_glVertex2i __proc_glVertex2i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2i(GLint x, GLint y) {
    ANAX_TRACE("glVertex2i");
    if (__proc_glVertex2i == nullptr) {
        __proc_glVertex2i = (__pfn_glVertex2i)GetProcAddress(EnsureRealOpenGL32(), "glVertex2i");
        if (__proc_glVertex2i == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2i: resolved OK\n");
        }
    }
    __proc_glVertex2i(x, y);
}

typedef void (__stdcall *__pfn_glVertex2iv)(void*);
static __pfn_glVertex2iv __proc_glVertex2iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2iv(void* v) {
    ANAX_TRACE("glVertex2iv");
    if (__proc_glVertex2iv == nullptr) {
        __proc_glVertex2iv = (__pfn_glVertex2iv)GetProcAddress(EnsureRealOpenGL32(), "glVertex2iv");
        if (__proc_glVertex2iv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2iv: resolved OK\n");
        }
    }
    __proc_glVertex2iv(v);
}

typedef void (__stdcall *__pfn_glVertex2s)(GLshort, GLshort);
static __pfn_glVertex2s __proc_glVertex2s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2s(GLshort x, GLshort y) {
    ANAX_TRACE("glVertex2s");
    if (__proc_glVertex2s == nullptr) {
        __proc_glVertex2s = (__pfn_glVertex2s)GetProcAddress(EnsureRealOpenGL32(), "glVertex2s");
        if (__proc_glVertex2s == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2s: resolved OK\n");
        }
    }
    __proc_glVertex2s(x, y);
}

typedef void (__stdcall *__pfn_glVertex2sv)(void*);
static __pfn_glVertex2sv __proc_glVertex2sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex2sv(void* v) {
    ANAX_TRACE("glVertex2sv");
    if (__proc_glVertex2sv == nullptr) {
        __proc_glVertex2sv = (__pfn_glVertex2sv)GetProcAddress(EnsureRealOpenGL32(), "glVertex2sv");
        if (__proc_glVertex2sv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex2sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex2sv: resolved OK\n");
        }
    }
    __proc_glVertex2sv(v);
}

typedef void (__stdcall *__pfn_glVertex3d)(GLdouble, GLdouble, GLdouble);
static __pfn_glVertex3d __proc_glVertex3d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3d(GLdouble x, GLdouble y, GLdouble z) {
    ANAX_TRACE("glVertex3d");
    if (__proc_glVertex3d == nullptr) {
        __proc_glVertex3d = (__pfn_glVertex3d)GetProcAddress(EnsureRealOpenGL32(), "glVertex3d");
        if (__proc_glVertex3d == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3d: resolved OK\n");
        }
    }
    __proc_glVertex3d(x, y, z);
}

typedef void (__stdcall *__pfn_glVertex3dv)(void*);
static __pfn_glVertex3dv __proc_glVertex3dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3dv(void* v) {
    ANAX_TRACE("glVertex3dv");
    if (__proc_glVertex3dv == nullptr) {
        __proc_glVertex3dv = (__pfn_glVertex3dv)GetProcAddress(EnsureRealOpenGL32(), "glVertex3dv");
        if (__proc_glVertex3dv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3dv: resolved OK\n");
        }
    }
    __proc_glVertex3dv(v);
}

typedef void (__stdcall *__pfn_glVertex3f)(GLfloat, GLfloat, GLfloat);
static __pfn_glVertex3f __proc_glVertex3f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    ANAX_TRACE("glVertex3f");
    if (__proc_glVertex3f == nullptr) {
        __proc_glVertex3f = (__pfn_glVertex3f)GetProcAddress(EnsureRealOpenGL32(), "glVertex3f");
        if (__proc_glVertex3f == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3f: resolved OK\n");
        }
    }
    __proc_glVertex3f(x, y, z);
}

typedef void (__stdcall *__pfn_glVertex3fv)(void*);
static __pfn_glVertex3fv __proc_glVertex3fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3fv(void* v) {
    ANAX_TRACE("glVertex3fv");
    if (__proc_glVertex3fv == nullptr) {
        __proc_glVertex3fv = (__pfn_glVertex3fv)GetProcAddress(EnsureRealOpenGL32(), "glVertex3fv");
        if (__proc_glVertex3fv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3fv: resolved OK\n");
        }
    }
    __proc_glVertex3fv(v);
}

typedef void (__stdcall *__pfn_glVertex3i)(GLint, GLint, GLint);
static __pfn_glVertex3i __proc_glVertex3i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3i(GLint x, GLint y, GLint z) {
    ANAX_TRACE("glVertex3i");
    if (__proc_glVertex3i == nullptr) {
        __proc_glVertex3i = (__pfn_glVertex3i)GetProcAddress(EnsureRealOpenGL32(), "glVertex3i");
        if (__proc_glVertex3i == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3i: resolved OK\n");
        }
    }
    __proc_glVertex3i(x, y, z);
}

typedef void (__stdcall *__pfn_glVertex3iv)(void*);
static __pfn_glVertex3iv __proc_glVertex3iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3iv(void* v) {
    ANAX_TRACE("glVertex3iv");
    if (__proc_glVertex3iv == nullptr) {
        __proc_glVertex3iv = (__pfn_glVertex3iv)GetProcAddress(EnsureRealOpenGL32(), "glVertex3iv");
        if (__proc_glVertex3iv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3iv: resolved OK\n");
        }
    }
    __proc_glVertex3iv(v);
}

typedef void (__stdcall *__pfn_glVertex3s)(GLshort, GLshort, GLshort);
static __pfn_glVertex3s __proc_glVertex3s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3s(GLshort x, GLshort y, GLshort z) {
    ANAX_TRACE("glVertex3s");
    if (__proc_glVertex3s == nullptr) {
        __proc_glVertex3s = (__pfn_glVertex3s)GetProcAddress(EnsureRealOpenGL32(), "glVertex3s");
        if (__proc_glVertex3s == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3s: resolved OK\n");
        }
    }
    __proc_glVertex3s(x, y, z);
}

typedef void (__stdcall *__pfn_glVertex3sv)(void*);
static __pfn_glVertex3sv __proc_glVertex3sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex3sv(void* v) {
    ANAX_TRACE("glVertex3sv");
    if (__proc_glVertex3sv == nullptr) {
        __proc_glVertex3sv = (__pfn_glVertex3sv)GetProcAddress(EnsureRealOpenGL32(), "glVertex3sv");
        if (__proc_glVertex3sv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex3sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex3sv: resolved OK\n");
        }
    }
    __proc_glVertex3sv(v);
}

typedef void (__stdcall *__pfn_glVertex4d)(GLdouble, GLdouble, GLdouble, GLdouble);
static __pfn_glVertex4d __proc_glVertex4d = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    ANAX_TRACE("glVertex4d");
    if (__proc_glVertex4d == nullptr) {
        __proc_glVertex4d = (__pfn_glVertex4d)GetProcAddress(EnsureRealOpenGL32(), "glVertex4d");
        if (__proc_glVertex4d == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4d: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4d: resolved OK\n");
        }
    }
    __proc_glVertex4d(x, y, z, w);
}

typedef void (__stdcall *__pfn_glVertex4dv)(void*);
static __pfn_glVertex4dv __proc_glVertex4dv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4dv(void* v) {
    ANAX_TRACE("glVertex4dv");
    if (__proc_glVertex4dv == nullptr) {
        __proc_glVertex4dv = (__pfn_glVertex4dv)GetProcAddress(EnsureRealOpenGL32(), "glVertex4dv");
        if (__proc_glVertex4dv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4dv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4dv: resolved OK\n");
        }
    }
    __proc_glVertex4dv(v);
}

typedef void (__stdcall *__pfn_glVertex4f)(GLfloat, GLfloat, GLfloat, GLfloat);
static __pfn_glVertex4f __proc_glVertex4f = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    ANAX_TRACE("glVertex4f");
    if (__proc_glVertex4f == nullptr) {
        __proc_glVertex4f = (__pfn_glVertex4f)GetProcAddress(EnsureRealOpenGL32(), "glVertex4f");
        if (__proc_glVertex4f == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4f: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4f: resolved OK\n");
        }
    }
    __proc_glVertex4f(x, y, z, w);
}

typedef void (__stdcall *__pfn_glVertex4fv)(void*);
static __pfn_glVertex4fv __proc_glVertex4fv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4fv(void* v) {
    ANAX_TRACE("glVertex4fv");
    if (__proc_glVertex4fv == nullptr) {
        __proc_glVertex4fv = (__pfn_glVertex4fv)GetProcAddress(EnsureRealOpenGL32(), "glVertex4fv");
        if (__proc_glVertex4fv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4fv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4fv: resolved OK\n");
        }
    }
    __proc_glVertex4fv(v);
}

typedef void (__stdcall *__pfn_glVertex4i)(GLint, GLint, GLint, GLint);
static __pfn_glVertex4i __proc_glVertex4i = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4i(GLint x, GLint y, GLint z, GLint w) {
    ANAX_TRACE("glVertex4i");
    if (__proc_glVertex4i == nullptr) {
        __proc_glVertex4i = (__pfn_glVertex4i)GetProcAddress(EnsureRealOpenGL32(), "glVertex4i");
        if (__proc_glVertex4i == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4i: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4i: resolved OK\n");
        }
    }
    __proc_glVertex4i(x, y, z, w);
}

typedef void (__stdcall *__pfn_glVertex4iv)(void*);
static __pfn_glVertex4iv __proc_glVertex4iv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4iv(void* v) {
    ANAX_TRACE("glVertex4iv");
    if (__proc_glVertex4iv == nullptr) {
        __proc_glVertex4iv = (__pfn_glVertex4iv)GetProcAddress(EnsureRealOpenGL32(), "glVertex4iv");
        if (__proc_glVertex4iv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4iv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4iv: resolved OK\n");
        }
    }
    __proc_glVertex4iv(v);
}

typedef void (__stdcall *__pfn_glVertex4s)(GLshort, GLshort, GLshort, GLshort);
static __pfn_glVertex4s __proc_glVertex4s = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    ANAX_TRACE("glVertex4s");
    if (__proc_glVertex4s == nullptr) {
        __proc_glVertex4s = (__pfn_glVertex4s)GetProcAddress(EnsureRealOpenGL32(), "glVertex4s");
        if (__proc_glVertex4s == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4s: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4s: resolved OK\n");
        }
    }
    __proc_glVertex4s(x, y, z, w);
}

typedef void (__stdcall *__pfn_glVertex4sv)(void*);
static __pfn_glVertex4sv __proc_glVertex4sv = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertex4sv(void* v) {
    ANAX_TRACE("glVertex4sv");
    if (__proc_glVertex4sv == nullptr) {
        __proc_glVertex4sv = (__pfn_glVertex4sv)GetProcAddress(EnsureRealOpenGL32(), "glVertex4sv");
        if (__proc_glVertex4sv == nullptr) {
            printf("[opengl32_enh_cpp]   glVertex4sv: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertex4sv: resolved OK\n");
        }
    }
    __proc_glVertex4sv(v);
}

typedef void (__stdcall *__pfn_glVertexPointer)(GLint, GLenum, GLsizei, void*);
static __pfn_glVertexPointer __proc_glVertexPointer = nullptr;

extern "C" __declspec(dllexport) void __stdcall glVertexPointer(GLint size, GLenum type, GLsizei stride, void* pointer) {
    ANAX_TRACE("glVertexPointer");
    if (__proc_glVertexPointer == nullptr) {
        __proc_glVertexPointer = (__pfn_glVertexPointer)GetProcAddress(EnsureRealOpenGL32(), "glVertexPointer");
        if (__proc_glVertexPointer == nullptr) {
            printf("[opengl32_enh_cpp]   glVertexPointer: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glVertexPointer: resolved OK\n");
        }
    }
    __proc_glVertexPointer(size, type, stride, pointer);
}

typedef void (__stdcall *__pfn_glViewport)(GLint, GLint, GLsizei, GLsizei);
static __pfn_glViewport __proc_glViewport = nullptr;

extern "C" __declspec(dllexport) void __stdcall glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    ANAX_TRACE("glViewport");
    if (__proc_glViewport == nullptr) {
        __proc_glViewport = (__pfn_glViewport)GetProcAddress(EnsureRealOpenGL32(), "glViewport");
        if (__proc_glViewport == nullptr) {
            printf("[opengl32_enh_cpp]   glViewport: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   glViewport: resolved OK\n");
        }
    }
    __proc_glViewport(x, y, width, height);
}

typedef BOOL (__stdcall *__pfn_wglCopyContext)(void*, void*, UINT);
static __pfn_wglCopyContext __proc_wglCopyContext = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglCopyContext(void* p0, void* p1, UINT p2) {
    ANAX_TRACE("wglCopyContext");
    if (__proc_wglCopyContext == nullptr) {
        __proc_wglCopyContext = (__pfn_wglCopyContext)GetProcAddress(EnsureRealOpenGL32(), "wglCopyContext");
        if (__proc_wglCopyContext == nullptr) {
            printf("[opengl32_enh_cpp]   wglCopyContext: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglCopyContext: resolved OK\n");
        }
    }
    return __proc_wglCopyContext(p0, p1, p2);
}

typedef void* (__stdcall *__pfn_wglCreateContext)(void*);
static __pfn_wglCreateContext __proc_wglCreateContext = nullptr;

extern "C" __declspec(dllexport) void* __stdcall wglCreateContext(void* p0) {
    ANAX_TRACE("wglCreateContext");
    if (__proc_wglCreateContext == nullptr) {
        __proc_wglCreateContext = (__pfn_wglCreateContext)GetProcAddress(EnsureRealOpenGL32(), "wglCreateContext");
        if (__proc_wglCreateContext == nullptr) {
            printf("[opengl32_enh_cpp]   wglCreateContext: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglCreateContext: resolved OK\n");
        }
    }
    ApplyWindowSizeOverride(p0);
    return __proc_wglCreateContext(p0);
}

typedef void* (__stdcall *__pfn_wglCreateLayerContext)(void*, int);
static __pfn_wglCreateLayerContext __proc_wglCreateLayerContext = nullptr;

extern "C" __declspec(dllexport) void* __stdcall wglCreateLayerContext(void* p0, int p1) {
    ANAX_TRACE("wglCreateLayerContext");
    if (__proc_wglCreateLayerContext == nullptr) {
        __proc_wglCreateLayerContext = (__pfn_wglCreateLayerContext)GetProcAddress(EnsureRealOpenGL32(), "wglCreateLayerContext");
        if (__proc_wglCreateLayerContext == nullptr) {
            printf("[opengl32_enh_cpp]   wglCreateLayerContext: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglCreateLayerContext: resolved OK\n");
        }
    }
    return __proc_wglCreateLayerContext(p0, p1);
}

typedef BOOL (__stdcall *__pfn_wglDeleteContext)(void*);
static __pfn_wglDeleteContext __proc_wglDeleteContext = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglDeleteContext(void* p0) {
    ANAX_TRACE("wglDeleteContext");
    if (__proc_wglDeleteContext == nullptr) {
        __proc_wglDeleteContext = (__pfn_wglDeleteContext)GetProcAddress(EnsureRealOpenGL32(), "wglDeleteContext");
        if (__proc_wglDeleteContext == nullptr) {
            printf("[opengl32_enh_cpp]   wglDeleteContext: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglDeleteContext: resolved OK\n");
        }
    }
    return __proc_wglDeleteContext(p0);
}

typedef void* (__stdcall *__pfn_wglGetCurrentContext)(void);
static __pfn_wglGetCurrentContext __proc_wglGetCurrentContext = nullptr;

extern "C" __declspec(dllexport) void* __stdcall wglGetCurrentContext(void) {
    ANAX_TRACE("wglGetCurrentContext");
    if (__proc_wglGetCurrentContext == nullptr) {
        __proc_wglGetCurrentContext = (__pfn_wglGetCurrentContext)GetProcAddress(EnsureRealOpenGL32(), "wglGetCurrentContext");
        if (__proc_wglGetCurrentContext == nullptr) {
            printf("[opengl32_enh_cpp]   wglGetCurrentContext: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglGetCurrentContext: resolved OK\n");
        }
    }
    return __proc_wglGetCurrentContext();
}

typedef void* (__stdcall *__pfn_wglGetCurrentDC)(void);
static __pfn_wglGetCurrentDC __proc_wglGetCurrentDC = nullptr;

extern "C" __declspec(dllexport) void* __stdcall wglGetCurrentDC(void) {
    ANAX_TRACE("wglGetCurrentDC");
    if (__proc_wglGetCurrentDC == nullptr) {
        __proc_wglGetCurrentDC = (__pfn_wglGetCurrentDC)GetProcAddress(EnsureRealOpenGL32(), "wglGetCurrentDC");
        if (__proc_wglGetCurrentDC == nullptr) {
            printf("[opengl32_enh_cpp]   wglGetCurrentDC: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglGetCurrentDC: resolved OK\n");
        }
    }
    return __proc_wglGetCurrentDC();
}

typedef void* (__stdcall *__pfn_wglGetProcAddress)(void*);
static __pfn_wglGetProcAddress __proc_wglGetProcAddress = nullptr;

extern "C" __declspec(dllexport) void* __stdcall wglGetProcAddress(void* p0) {
    ANAX_TRACE("wglGetProcAddress");
    if (__proc_wglGetProcAddress == nullptr) {
        __proc_wglGetProcAddress = (__pfn_wglGetProcAddress)GetProcAddress(EnsureRealOpenGL32(), "wglGetProcAddress");
        if (__proc_wglGetProcAddress == nullptr) {
            printf("[opengl32_enh_cpp]   wglGetProcAddress: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglGetProcAddress: resolved OK\n");
        }
    }
    return __proc_wglGetProcAddress(p0);
}

typedef BOOL (__stdcall *__pfn_wglMakeCurrent)(void*, void*);
static __pfn_wglMakeCurrent __proc_wglMakeCurrent = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglMakeCurrent(void* p0, void* p1) {
    ANAX_TRACE("wglMakeCurrent");
    if (__proc_wglMakeCurrent == nullptr) {
        __proc_wglMakeCurrent = (__pfn_wglMakeCurrent)GetProcAddress(EnsureRealOpenGL32(), "wglMakeCurrent");
        if (__proc_wglMakeCurrent == nullptr) {
            printf("[opengl32_enh_cpp]   wglMakeCurrent: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglMakeCurrent: resolved OK\n");
        }
    }
    return __proc_wglMakeCurrent(p0, p1);
}

typedef BOOL (__stdcall *__pfn_wglShareLists)(void*, void*);
static __pfn_wglShareLists __proc_wglShareLists = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglShareLists(void* p0, void* p1) {
    ANAX_TRACE("wglShareLists");
    if (__proc_wglShareLists == nullptr) {
        __proc_wglShareLists = (__pfn_wglShareLists)GetProcAddress(EnsureRealOpenGL32(), "wglShareLists");
        if (__proc_wglShareLists == nullptr) {
            printf("[opengl32_enh_cpp]   wglShareLists: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglShareLists: resolved OK\n");
        }
    }
    return __proc_wglShareLists(p0, p1);
}

typedef BOOL (__stdcall *__pfn_wglUseFontBitmapsA)(void*, DWORD, DWORD, DWORD);
static __pfn_wglUseFontBitmapsA __proc_wglUseFontBitmapsA = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglUseFontBitmapsA(void* p0, DWORD p1, DWORD p2, DWORD p3) {
    ANAX_TRACE("wglUseFontBitmapsA");
    if (__proc_wglUseFontBitmapsA == nullptr) {
        __proc_wglUseFontBitmapsA = (__pfn_wglUseFontBitmapsA)GetProcAddress(EnsureRealOpenGL32(), "wglUseFontBitmapsA");
        if (__proc_wglUseFontBitmapsA == nullptr) {
            printf("[opengl32_enh_cpp]   wglUseFontBitmapsA: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglUseFontBitmapsA: resolved OK\n");
        }
    }
    return __proc_wglUseFontBitmapsA(p0, p1, p2, p3);
}

typedef BOOL (__stdcall *__pfn_wglUseFontBitmapsW)(void*, DWORD, DWORD, DWORD);
static __pfn_wglUseFontBitmapsW __proc_wglUseFontBitmapsW = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglUseFontBitmapsW(void* p0, DWORD p1, DWORD p2, DWORD p3) {
    ANAX_TRACE("wglUseFontBitmapsW");
    if (__proc_wglUseFontBitmapsW == nullptr) {
        __proc_wglUseFontBitmapsW = (__pfn_wglUseFontBitmapsW)GetProcAddress(EnsureRealOpenGL32(), "wglUseFontBitmapsW");
        if (__proc_wglUseFontBitmapsW == nullptr) {
            printf("[opengl32_enh_cpp]   wglUseFontBitmapsW: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglUseFontBitmapsW: resolved OK\n");
        }
    }
    return __proc_wglUseFontBitmapsW(p0, p1, p2, p3);
}

typedef BOOL (__stdcall *__pfn_wglUseFontOutlinesA)(void*, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, void*);
static __pfn_wglUseFontOutlinesA __proc_wglUseFontOutlinesA = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglUseFontOutlinesA(void* p0, DWORD p1, DWORD p2, DWORD p3, FLOAT p4, FLOAT p5, int p6, void* p7) {
    ANAX_TRACE("wglUseFontOutlinesA");
    if (__proc_wglUseFontOutlinesA == nullptr) {
        __proc_wglUseFontOutlinesA = (__pfn_wglUseFontOutlinesA)GetProcAddress(EnsureRealOpenGL32(), "wglUseFontOutlinesA");
        if (__proc_wglUseFontOutlinesA == nullptr) {
            printf("[opengl32_enh_cpp]   wglUseFontOutlinesA: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglUseFontOutlinesA: resolved OK\n");
        }
    }
    return __proc_wglUseFontOutlinesA(p0, p1, p2, p3, p4, p5, p6, p7);
}

typedef BOOL (__stdcall *__pfn_wglUseFontOutlinesW)(void*, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, void*);
static __pfn_wglUseFontOutlinesW __proc_wglUseFontOutlinesW = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglUseFontOutlinesW(void* p0, DWORD p1, DWORD p2, DWORD p3, FLOAT p4, FLOAT p5, int p6, void* p7) {
    ANAX_TRACE("wglUseFontOutlinesW");
    if (__proc_wglUseFontOutlinesW == nullptr) {
        __proc_wglUseFontOutlinesW = (__pfn_wglUseFontOutlinesW)GetProcAddress(EnsureRealOpenGL32(), "wglUseFontOutlinesW");
        if (__proc_wglUseFontOutlinesW == nullptr) {
            printf("[opengl32_enh_cpp]   wglUseFontOutlinesW: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglUseFontOutlinesW: resolved OK\n");
        }
    }
    return __proc_wglUseFontOutlinesW(p0, p1, p2, p3, p4, p5, p6, p7);
}

typedef BOOL (__stdcall *__pfn_wglDescribeLayerPlane)(void*, int, int, UINT, void*);
static __pfn_wglDescribeLayerPlane __proc_wglDescribeLayerPlane = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglDescribeLayerPlane(void* p0, int p1, int p2, UINT p3, void* p4) {
    ANAX_TRACE("wglDescribeLayerPlane");
    if (__proc_wglDescribeLayerPlane == nullptr) {
        __proc_wglDescribeLayerPlane = (__pfn_wglDescribeLayerPlane)GetProcAddress(EnsureRealOpenGL32(), "wglDescribeLayerPlane");
        if (__proc_wglDescribeLayerPlane == nullptr) {
            printf("[opengl32_enh_cpp]   wglDescribeLayerPlane: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglDescribeLayerPlane: resolved OK\n");
        }
    }
    return __proc_wglDescribeLayerPlane(p0, p1, p2, p3, p4);
}

typedef int (__stdcall *__pfn_wglSetLayerPaletteEntries)(void*, int, int, int, void*);
static __pfn_wglSetLayerPaletteEntries __proc_wglSetLayerPaletteEntries = nullptr;

extern "C" __declspec(dllexport) int __stdcall wglSetLayerPaletteEntries(void* p0, int p1, int p2, int p3, void* p4) {
    ANAX_TRACE("wglSetLayerPaletteEntries");
    if (__proc_wglSetLayerPaletteEntries == nullptr) {
        __proc_wglSetLayerPaletteEntries = (__pfn_wglSetLayerPaletteEntries)GetProcAddress(EnsureRealOpenGL32(), "wglSetLayerPaletteEntries");
        if (__proc_wglSetLayerPaletteEntries == nullptr) {
            printf("[opengl32_enh_cpp]   wglSetLayerPaletteEntries: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglSetLayerPaletteEntries: resolved OK\n");
        }
    }
    return __proc_wglSetLayerPaletteEntries(p0, p1, p2, p3, p4);
}

typedef int (__stdcall *__pfn_wglGetLayerPaletteEntries)(void*, int, int, int, void*);
static __pfn_wglGetLayerPaletteEntries __proc_wglGetLayerPaletteEntries = nullptr;

extern "C" __declspec(dllexport) int __stdcall wglGetLayerPaletteEntries(void* p0, int p1, int p2, int p3, void* p4) {
    ANAX_TRACE("wglGetLayerPaletteEntries");
    if (__proc_wglGetLayerPaletteEntries == nullptr) {
        __proc_wglGetLayerPaletteEntries = (__pfn_wglGetLayerPaletteEntries)GetProcAddress(EnsureRealOpenGL32(), "wglGetLayerPaletteEntries");
        if (__proc_wglGetLayerPaletteEntries == nullptr) {
            printf("[opengl32_enh_cpp]   wglGetLayerPaletteEntries: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglGetLayerPaletteEntries: resolved OK\n");
        }
    }
    return __proc_wglGetLayerPaletteEntries(p0, p1, p2, p3, p4);
}

typedef BOOL (__stdcall *__pfn_wglRealizeLayerPalette)(void*, int, BOOL);
static __pfn_wglRealizeLayerPalette __proc_wglRealizeLayerPalette = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglRealizeLayerPalette(void* p0, int p1, BOOL p2) {
    ANAX_TRACE("wglRealizeLayerPalette");
    if (__proc_wglRealizeLayerPalette == nullptr) {
        __proc_wglRealizeLayerPalette = (__pfn_wglRealizeLayerPalette)GetProcAddress(EnsureRealOpenGL32(), "wglRealizeLayerPalette");
        if (__proc_wglRealizeLayerPalette == nullptr) {
            printf("[opengl32_enh_cpp]   wglRealizeLayerPalette: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglRealizeLayerPalette: resolved OK\n");
        }
    }
    return __proc_wglRealizeLayerPalette(p0, p1, p2);
}

typedef BOOL (__stdcall *__pfn_wglSwapLayerBuffers)(void*, UINT);
static __pfn_wglSwapLayerBuffers __proc_wglSwapLayerBuffers = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglSwapLayerBuffers(void* p0, UINT p1) {
    ANAX_TRACE("wglSwapLayerBuffers");
    if (__proc_wglSwapLayerBuffers == nullptr) {
        __proc_wglSwapLayerBuffers = (__pfn_wglSwapLayerBuffers)GetProcAddress(EnsureRealOpenGL32(), "wglSwapLayerBuffers");
        if (__proc_wglSwapLayerBuffers == nullptr) {
            printf("[opengl32_enh_cpp]   wglSwapLayerBuffers: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglSwapLayerBuffers: resolved OK\n");
        }
    }
    return __proc_wglSwapLayerBuffers(p0, p1);
}

typedef DWORD (__stdcall *__pfn_wglSwapMultipleBuffers)(UINT, void*);
static __pfn_wglSwapMultipleBuffers __proc_wglSwapMultipleBuffers = nullptr;

extern "C" __declspec(dllexport) DWORD __stdcall wglSwapMultipleBuffers(UINT p0, void* p1) {
    ANAX_TRACE("wglSwapMultipleBuffers");
    if (__proc_wglSwapMultipleBuffers == nullptr) {
        __proc_wglSwapMultipleBuffers = (__pfn_wglSwapMultipleBuffers)GetProcAddress(EnsureRealOpenGL32(), "wglSwapMultipleBuffers");
        if (__proc_wglSwapMultipleBuffers == nullptr) {
            printf("[opengl32_enh_cpp]   wglSwapMultipleBuffers: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglSwapMultipleBuffers: resolved OK\n");
        }
    }
    return __proc_wglSwapMultipleBuffers(p0, p1);
}

typedef int (__stdcall *__pfn_wglChoosePixelFormat)(void*, void*);
static __pfn_wglChoosePixelFormat __proc_wglChoosePixelFormat = nullptr;

extern "C" __declspec(dllexport) int __stdcall wglChoosePixelFormat(void* p0, void* p1) {
    ANAX_TRACE("wglChoosePixelFormat");
    if (__proc_wglChoosePixelFormat == nullptr) {
        __proc_wglChoosePixelFormat = (__pfn_wglChoosePixelFormat)GetProcAddress(EnsureRealOpenGL32(), "wglChoosePixelFormat");
        if (__proc_wglChoosePixelFormat == nullptr) {
            printf("[opengl32_enh_cpp]   wglChoosePixelFormat: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglChoosePixelFormat: resolved OK\n");
        }
    }
    return __proc_wglChoosePixelFormat(p0, p1);
}

typedef int (__stdcall *__pfn_wglDescribePixelFormat)(void*, int, UINT, void*);
static __pfn_wglDescribePixelFormat __proc_wglDescribePixelFormat = nullptr;

extern "C" __declspec(dllexport) int __stdcall wglDescribePixelFormat(void* p0, int p1, UINT p2, void* p3) {
    ANAX_TRACE("wglDescribePixelFormat");
    if (__proc_wglDescribePixelFormat == nullptr) {
        __proc_wglDescribePixelFormat = (__pfn_wglDescribePixelFormat)GetProcAddress(EnsureRealOpenGL32(), "wglDescribePixelFormat");
        if (__proc_wglDescribePixelFormat == nullptr) {
            printf("[opengl32_enh_cpp]   wglDescribePixelFormat: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglDescribePixelFormat: resolved OK\n");
        }
    }
    return __proc_wglDescribePixelFormat(p0, p1, p2, p3);
}

typedef int (__stdcall *__pfn_wglGetPixelFormat)(void*);
static __pfn_wglGetPixelFormat __proc_wglGetPixelFormat = nullptr;

extern "C" __declspec(dllexport) int __stdcall wglGetPixelFormat(void* p0) {
    ANAX_TRACE("wglGetPixelFormat");
    if (__proc_wglGetPixelFormat == nullptr) {
        __proc_wglGetPixelFormat = (__pfn_wglGetPixelFormat)GetProcAddress(EnsureRealOpenGL32(), "wglGetPixelFormat");
        if (__proc_wglGetPixelFormat == nullptr) {
            printf("[opengl32_enh_cpp]   wglGetPixelFormat: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglGetPixelFormat: resolved OK\n");
        }
    }
    return __proc_wglGetPixelFormat(p0);
}

typedef BOOL (__stdcall *__pfn_wglSetPixelFormat)(void*, int, void*);
static __pfn_wglSetPixelFormat __proc_wglSetPixelFormat = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglSetPixelFormat(void* p0, int p1, void* p2) {
    ANAX_TRACE("wglSetPixelFormat");
    if (__proc_wglSetPixelFormat == nullptr) {
        __proc_wglSetPixelFormat = (__pfn_wglSetPixelFormat)GetProcAddress(EnsureRealOpenGL32(), "wglSetPixelFormat");
        if (__proc_wglSetPixelFormat == nullptr) {
            printf("[opengl32_enh_cpp]   wglSetPixelFormat: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglSetPixelFormat: resolved OK\n");
        }
    }
    return __proc_wglSetPixelFormat(p0, p1, p2);
}

typedef BOOL (__stdcall *__pfn_wglSwapBuffers)(void*);
static __pfn_wglSwapBuffers __proc_wglSwapBuffers = nullptr;

extern "C" __declspec(dllexport) BOOL __stdcall wglSwapBuffers(void* p0) {
    ANAX_TRACE("wglSwapBuffers");
    if (__proc_wglSwapBuffers == nullptr) {
        __proc_wglSwapBuffers = (__pfn_wglSwapBuffers)GetProcAddress(EnsureRealOpenGL32(), "wglSwapBuffers");
        if (__proc_wglSwapBuffers == nullptr) {
            printf("[opengl32_enh_cpp]   wglSwapBuffers: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglSwapBuffers: resolved OK\n");
        }
    }
    FinalizeCameraForFrame();
    ApplySelectedEffect(p0);
    AdvanceCameraHistory();
    AdvanceProjectionHistory();
    AdvanceTaaJitter();
    InvalidateWorldFrame();
    return __proc_wglSwapBuffers(p0);
}

typedef void* (__stdcall *__pfn_wglGetDefaultProcAddress)(void*);
static __pfn_wglGetDefaultProcAddress __proc_wglGetDefaultProcAddress = nullptr;

extern "C" __declspec(dllexport) void* __stdcall wglGetDefaultProcAddress(void* p0) {
    ANAX_TRACE("wglGetDefaultProcAddress");
    if (__proc_wglGetDefaultProcAddress == nullptr) {
        __proc_wglGetDefaultProcAddress = (__pfn_wglGetDefaultProcAddress)GetProcAddress(EnsureRealOpenGL32(), "wglGetDefaultProcAddress");
        if (__proc_wglGetDefaultProcAddress == nullptr) {
            printf("[opengl32_enh_cpp]   wglGetDefaultProcAddress: FAILED to resolve, GetLastError=%lu\n", GetLastError());
        } else {
            printf("[opengl32_enh_cpp]   wglGetDefaultProcAddress: resolved OK\n");
        }
    }
    return __proc_wglGetDefaultProcAddress(p0);
}
