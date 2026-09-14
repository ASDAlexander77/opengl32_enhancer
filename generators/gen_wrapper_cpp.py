"""
Generates a plain C++ proxy DLL (wrapper32.cpp + wrapper32.def) with the exact same
forwarding logic and debug tracing as gen_wrapper.py's tslang wrapper.ts, but compilable
with the plain MSVC x86 toolchain - no tslang/LLVM involved. This exists to prove out a
32-bit build quickly: the tslang toolchain only has x64 runtime libraries installed right
now (see docs note in wrapper.ts), so a genuinely 32-bit wrapper.ts build needs an x86
LLVM/MLIR rebuild first. This script sidesteps that for a fast proof-of-concept.

Deliberately avoids #include <windows.h> (and therefore <wingdi.h>): that header declares
wgl*/Gl* prototypes unconditionally (NOGDI does not gate them), which would collide with
our own dllexport definitions of the same names. Instead it forward-declares only the
handful of kernel32 entry points actually used, exactly as wrapper.ts's `declare function`
lines do for the tslang build.

Usage: python gen_wrapper_cpp.py [x86|x64]   (default: x86, the point of this script)
"""
import re
import sys

GL_FILE = r"gl_full.txt"
CPP_OUT = r"..\wrapper32.cpp"
DEF_OUT = r"..\wrapper32.def"

# Raw C type -> C++ type used in the generated file. Any pointer/handle type collapses to
# void* (ABI only cares about pointer size/slot, never the pointee), matching Opaque in the
# tslang wrapper. Everything else gets a same-size typedef defined at the top of the file.
POINTER_TYPES = {"HDC", "HGLRC", "HANDLE", "LPVOID", "PROC", "LPCSTR", "GLvoid"}

def is_pointer(c_type):
    t = c_type.strip()
    return "*" in t or t in POINTER_TYPES or t.startswith("LP")

def map_type(c_type):
    t = re.sub(r"^const\s+", "", c_type.strip())
    t = re.sub(r"\bCONST\b", "", t).strip()
    t = re.sub(r"\s+", " ", t).strip()
    if is_pointer(t):
        return "void*"
    return t  # GLenum, GLfloat, DWORD, UINT, BOOL, int, FLOAT, ... - typedef'd below

def parse_args(arg_str):
    arg_str = arg_str.strip()
    if arg_str in ("", "VOID", "void"):
        return []
    parts, depth, cur = [], 0, ""
    for ch in arg_str:
        if ch == "," and depth == 0:
            parts.append(cur); cur = ""
        else:
            if ch in "(<": depth += 1
            elif ch in ")>": depth -= 1
            cur += ch
    parts.append(cur)

    args = []
    for i, p in enumerate(parts):
        p = p.strip()
        if p in ("", "VOID", "void"):
            continue
        m = re.match(r"^(.*?[\s\*])?(\**)\s*([A-Za-z_][A-Za-z0-9_]*)$", p)
        pname = f"p{i}"
        ctype = p
        if m:
            head = (m.group(1) or "") + (m.group(2) or "")
            ident = m.group(3)
            if head.strip() != "":
                ctype = head.strip()
                pname = ident
            else:
                ctype = p
        args.append((pname, map_type(ctype)))
    return args

def parse_gl_file(path):
    text = open(path, encoding="utf-8").read()
    pattern = re.compile(
        r"WINGDIAPI\s+([A-Za-z_][A-Za-z0-9_ \*]*?)\s+APIENTRY\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*;",
        re.MULTILINE
    )
    funcs = []
    for m in pattern.finditer(text):
        ret_c, name, args_c = m.group(1).strip(), m.group(2), m.group(3)
        funcs.append((name, parse_args(args_c), map_type(ret_c)))
    return funcs

# Same table as gen_wrapper.py (kept in sync by hand - small, rarely changes).
WGL_RAW = [
    ("wglCopyContext", "BOOL", ["HGLRC", "HGLRC", "UINT"]),
    ("wglCreateContext", "HGLRC", ["HDC"]),
    ("wglCreateLayerContext", "HGLRC", ["HDC", "int"]),
    ("wglDeleteContext", "BOOL", ["HGLRC"]),
    ("wglGetCurrentContext", "HGLRC", []),
    ("wglGetCurrentDC", "HDC", []),
    ("wglGetProcAddress", "PROC", ["LPCSTR"]),
    ("wglMakeCurrent", "BOOL", ["HDC", "HGLRC"]),
    ("wglShareLists", "BOOL", ["HGLRC", "HGLRC"]),
    ("wglUseFontBitmapsA", "BOOL", ["HDC", "DWORD", "DWORD", "DWORD"]),
    ("wglUseFontBitmapsW", "BOOL", ["HDC", "DWORD", "DWORD", "DWORD"]),
    ("wglUseFontOutlinesA", "BOOL", ["HDC", "DWORD", "DWORD", "DWORD", "FLOAT", "FLOAT", "int", "LPGLYPHMETRICSFLOAT"]),
    ("wglUseFontOutlinesW", "BOOL", ["HDC", "DWORD", "DWORD", "DWORD", "FLOAT", "FLOAT", "int", "LPGLYPHMETRICSFLOAT"]),
    ("wglDescribeLayerPlane", "BOOL", ["HDC", "int", "int", "UINT", "LPLAYERPLANEDESCRIPTOR"]),
    ("wglSetLayerPaletteEntries", "int", ["HDC", "int", "int", "int", "CONST COLORREF *"]),
    ("wglGetLayerPaletteEntries", "int", ["HDC", "int", "int", "int", "COLORREF *"]),
    ("wglRealizeLayerPalette", "BOOL", ["HDC", "int", "BOOL"]),
    ("wglSwapLayerBuffers", "BOOL", ["HDC", "UINT"]),
    ("wglSwapMultipleBuffers", "DWORD", ["UINT", "CONST WGLSWAP *"]),
    ("wglChoosePixelFormat", "int", ["HDC", "CONST PIXELFORMATDESCRIPTOR *"]),
    ("wglDescribePixelFormat", "int", ["HDC", "int", "UINT", "LPPIXELFORMATDESCRIPTOR"]),
    ("wglGetPixelFormat", "int", ["HDC"]),
    ("wglSetPixelFormat", "BOOL", ["HDC", "int", "CONST PIXELFORMATDESCRIPTOR *"]),
    ("wglSwapBuffers", "BOOL", ["HDC"]),
    ("wglGetDefaultProcAddress", "PROC", ["LPCSTR"]),
]

def wgl_funcs():
    out = []
    for name, ret_c, arg_types in WGL_RAW:
        ret = map_type(ret_c)
        args = [(f"p{i}", map_type(t)) for i, t in enumerate(arg_types)]
        out.append((name, args, ret))
    return out

UNKNOWN_EXPORTS = [
    "GlmfBeginGlsBlock", "GlmfCloseMetaFile", "GlmfEndGlsBlock",
    "GlmfEndPlayback", "GlmfInitPlayback", "GlmfPlayGlsRecord",
    "glDebugEntry",
]

TYPEDEFS = """\
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

extern "C" {
    __declspec(dllimport) void* __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
    __declspec(dllimport) DWORD __stdcall GetLastError(void);
}
"""

def emit_cpp(funcs):
    lines = []
    lines.append("// AUTO-GENERATED opengl32 interception/passthrough wrapper - C++ x86 proof of concept.")
    lines.append("//")
    lines.append("// Same forwarding logic and debug tracing as wrapper.ts (see generators/gen_wrapper.py),")
    lines.append("// but plain C++ so it can be built with the ordinary MSVC x86 toolchain: the tslang")
    lines.append("// toolchain only has x64 runtime libraries (TypeScriptAsyncRuntime.lib, LLVMSupport.lib)")
    lines.append("// installed right now, so a real wrapper.ts x86 build needs an x86 LLVM/MLIR rebuild")
    lines.append("// first. This file exists to prove the 32-bit build is needed/works before investing in")
    lines.append("// that rebuild - see generators/gen_wrapper_cpp.py.")
    lines.append("//")
    lines.append(f"// NOT wrapped (signature genuinely unknown/undocumented): {', '.join(UNKNOWN_EXPORTS)}.")
    lines.append("")
    lines.append("#include <cstdio>")
    lines.append("")
    lines.append(TYPEDEFS)
    lines.append("static void* g_real = nullptr;")
    lines.append("")
    lines.append("static void* EnsureRealOpenGL32() {")
    lines.append("    if (g_real == nullptr) {")
    lines.append('        printf("[opengl32_enh_cpp] loading real opengl32.dll from C:\\\\Windows\\\\System32\\\\opengl32.dll ...\\n");')
    lines.append('        g_real = LoadLibraryA("C:\\\\Windows\\\\System32\\\\opengl32.dll");')
    lines.append("        if (g_real == nullptr) {")
    lines.append('            printf("[opengl32_enh_cpp] FAILED to load real opengl32.dll, GetLastError=%lu\\n", GetLastError());')
    lines.append("        } else {")
    lines.append('            printf("[opengl32_enh_cpp] real opengl32.dll loaded OK\\n");')
    lines.append("        }")
    lines.append("    }")
    lines.append("    return g_real;")
    lines.append("}")
    lines.append("")

    for name, args, ret in funcs:
        params_decl = ", ".join(f"{ptype} {pname}" for pname, ptype in args) or "void"
        params_call = ", ".join(pname for pname, _ in args)
        fn_ptr_t = f"__pfn_{name}"
        cache_var = f"__proc_{name}"
        param_types = ", ".join(ptype for _, ptype in args) or "void"
        lines.append(f"typedef {ret} (__stdcall *{fn_ptr_t})({param_types});")
        lines.append(f"static {fn_ptr_t} {cache_var} = nullptr;")
        lines.append("")
        lines.append(f'extern "C" __declspec(dllexport) {ret} __stdcall {name}({params_decl}) {{')
        lines.append(f'    printf("[opengl32_enh_cpp] call {name}\\n");')
        lines.append(f"    if ({cache_var} == nullptr) {{")
        lines.append(f'        {cache_var} = ({fn_ptr_t})GetProcAddress(EnsureRealOpenGL32(), "{name}");')
        lines.append(f"        if ({cache_var} == nullptr) {{")
        lines.append(f'            printf("[opengl32_enh_cpp]   {name}: FAILED to resolve, GetLastError=%lu\\n", GetLastError());')
        lines.append("        } else {")
        lines.append(f'            printf("[opengl32_enh_cpp]   {name}: resolved OK\\n");')
        lines.append("        }")
        lines.append("    }")
        if ret == "void":
            lines.append(f"    {cache_var}({params_call});")
        else:
            lines.append(f"    return {cache_var}({params_call});")
        lines.append("}")
        lines.append("")

    return "\n".join(lines)

def emit_def(funcs, dll_name):
    lines = [f"LIBRARY {dll_name}", "EXPORTS"]
    for name, _, _ in funcs:
        lines.append(f"    {name}")
    return "\n".join(lines) + "\n"

def main():
    gl_funcs = parse_gl_file(GL_FILE)
    wgl = wgl_funcs()
    all_funcs = gl_funcs + wgl
    names = set()
    dups = [n for n, _, _ in all_funcs if n in names or names.add(n)]
    print(f"GL functions parsed: {len(gl_funcs)}")
    print(f"WGL functions:       {len(wgl)}")
    print(f"Total:               {len(all_funcs)}")
    print(f"Duplicates:          {dups}")

    with open(CPP_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(emit_cpp(all_funcs))
    print(f"Written: {CPP_OUT}")

    with open(DEF_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(emit_def(all_funcs, "opengl32_enh32"))
    print(f"Written: {DEF_OUT}")

if __name__ == "__main__":
    main()
