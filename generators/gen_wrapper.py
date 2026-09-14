import re, sys

GL_FILE = r"gl_full.txt"
OUT_FILE = r"wrapper_generated.ts"

# C type -> tslang native type
TYPE_MAP = {
    "GLenum": "u32", "GLbitfield": "u32", "GLuint": "u32", "DWORD": "u32",
    "UINT": "u32", "COLORREF": "u32",
    "GLint": "i32", "GLsizei": "i32", "int": "i32", "BOOL": "i32",
    "GLboolean": "u8", "GLubyte": "u8", "BYTE": "u8",
    "GLbyte": "i8",
    "GLshort": "i16",
    "GLushort": "u16", "WORD": "u16",
    "GLfloat": "f32", "GLclampf": "f32", "FLOAT": "f32",
    "GLdouble": "f64", "GLclampd": "f64",
    "void": "void", "VOID": "void",
}

def map_type(c_type, is_return):
    t = c_type.strip()
    t = re.sub(r"^const\s+", "", t)
    t = re.sub(r"\s+", " ", t).strip()
    if "*" in t or t in ("HDC", "HGLRC", "LPCSTR", "LPVOID", "GLvoid", "HANDLE"):
        return "Opaque"
    base = t
    if base in TYPE_MAP:
        mapped = TYPE_MAP[base]
        if is_return and mapped == "void":
            return "void"
        return mapped
    # Unknown named struct/typedef (e.g. WGLSWAP by value - none here) -> Opaque as fallback
    return "Opaque"

def parse_args(arg_str, fname):
    arg_str = arg_str.strip()
    if arg_str == "" or arg_str == "VOID" or arg_str == "void":
        return []
    parts = []
    depth = 0
    cur = ""
    for ch in arg_str:
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            if ch in "(<":
                depth += 1
            elif ch in ")>":
                depth -= 1
            cur += ch
    parts.append(cur)

    args = []
    for i, p in enumerate(parts):
        p = p.strip()
        if p in ("", "VOID", "void"):
            continue
        # split into type + optional identifier name (last word if it's a plain identifier)
        m = re.match(r"^(.*?[\s\*])?(\**)\s*([A-Za-z_][A-Za-z0-9_]*)$", p)
        pname = f"p{i}"
        ctype = p
        if m:
            head = (m.group(1) or "") + (m.group(2) or "")
            ident = m.group(3)
            # if the whole thing is just a type keyword (no separate identifier), keep as type
            if head.strip() != "":
                ctype = head.strip()
                pname = ident
            else:
                ctype = p
        args.append((pname, map_type(ctype, False)))
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
        ret = map_type(ret_c, True)
        args = parse_args(args_c, name)
        funcs.append((name, args, ret))
    return funcs

# --- WGL functions from wingdi.h (manually transcribed, params unnamed in the header) ---
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
    # Undocumented in current SDK headers but stable, well-known signatures
    # (GDI pixel-format entry points also exported by opengl32.dll under these names).
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
        ret = map_type(ret_c, True)
        args = [(f"p{i}", map_type(t, False)) for i, t in enumerate(arg_types)]
        out.append((name, args, ret))
    return out

# Exports with genuinely unknown/undocumented signatures - not safe to guess at.
UNKNOWN_EXPORTS = [
    "GlmfBeginGlsBlock", "GlmfCloseMetaFile", "GlmfEndGlsBlock",
    "GlmfEndPlayback", "GlmfInitPlayback", "GlmfPlayGlsRecord",
    "glDebugEntry",
]

def emit(funcs):
    lines = []
    lines.append("// AUTO-GENERATED opengl32.dll interception/passthrough wrapper.")
    lines.append("// Every exported function below re-exports under the SAME name as the real")
    lines.append("// opengl32.dll, resolves the real implementation from the genuine system DLL")
    lines.append("// (loaded once, by absolute path so this proxy never loads itself), and")
    lines.append("// forwards the call unchanged. Add per-function logging/interception inside")
    lines.append("// the wrapper bodies below as needed.")
    lines.append("//")
    lines.append("// Every wrapper prints a debug trace (tagged [opengl32_enh]) on:")
    lines.append("//   - module load (path used, success/failure, GetLastError() on failure)")
    lines.append("//   - first-time symbol resolution per function (success/failure + address)")
    lines.append("//   - every call (function name), so a loading/interception problem shows up")
    lines.append("//     as a gap in the trace rather than a silent wrong result.")
    lines.append("// This is debug-build noise; strip it (or gate it behind a flag) before")
    lines.append("// using this wrapper in a real render loop.")
    lines.append("//")
    lines.append("// Signatures come from the Windows SDK headers (GL/GL.h, wingdi.h) for the")
    lines.append("// documented functions, and from well-known stable signatures for a few")
    lines.append("// undocumented-but-stable wgl entry points (wglChoosePixelFormat and friends).")
    lines.append("//")
    lines.append(f"// NOT wrapped (signature genuinely unknown/undocumented): {', '.join(UNKNOWN_EXPORTS)}.")
    lines.append("// Add these manually if you need them, after confirming their real signature.")
    lines.append("")
    lines.append('declare function LoadLibraryA(libraryName: Opaque): Opaque;')
    lines.append('declare function GetProcAddress(library: Opaque, functionName: Opaque): Opaque;')
    lines.append("declare function GetLastError(): u32;")
    lines.append("")
    lines.append("// Absolute path so this proxy DLL (also named opengl32.dll when deployed")
    lines.append("// next to the target application) never resolves back to itself.")
    lines.append('const __real_opengl32_path: Opaque = "C:\\\\Windows\\\\System32\\\\opengl32.dll";')
    lines.append("let __real_opengl32: Opaque = 0;")
    lines.append("")
    lines.append("function __ensureRealOpenGL32(): Opaque {")
    lines.append("    if (__real_opengl32 === 0) {")
    lines.append('        console.log("[opengl32_enh] loading real opengl32.dll from C:\\\\Windows\\\\System32\\\\opengl32.dll ...");')
    lines.append("        __real_opengl32 = LoadLibraryA(__real_opengl32_path);")
    lines.append("        if (__real_opengl32 === null) {")
    lines.append('            console.error("[opengl32_enh] FAILED to load real opengl32.dll, GetLastError=" + (<number>GetLastError()).toString());')
    lines.append("        } else {")
    lines.append('            console.log("[opengl32_enh] real opengl32.dll loaded OK");')
    lines.append("        }")
    lines.append("    }")
    lines.append("    return __real_opengl32;")
    lines.append("}")
    lines.append("")

    for name, args, ret in funcs:
        params_decl = ", ".join(f"{pname}: {ptype}" for pname, ptype in args)
        params_call = ", ".join(pname for pname, _ in args)
        cache_var = f"__proc_{name}"
        lines.append(f"let {cache_var}: Opaque = 0;")
        lines.append("")
        lines.append(f'@dllname("{name}")')
        lines.append(f"export function {name}({params_decl}): {ret} {{")
        lines.append(f'    console.log("[opengl32_enh] call {name}");')
        lines.append(f"    if ({cache_var} === 0) {{")
        lines.append(f'        {cache_var} = GetProcAddress(__ensureRealOpenGL32(), "{name}");')
        lines.append(f"        if ({cache_var} === null) {{")
        lines.append(f'            console.error("[opengl32_enh]   {name}: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());')
        lines.append("        } else {")
        lines.append(f'            console.log("[opengl32_enh]   {name}: resolved OK");')
        lines.append("        }")
        lines.append("    }")
        cast_params = ", ".join(ptype for _, ptype in args)
        if ret == "void":
            lines.append(f"    (<({cast_params}) => void>{cache_var})({params_call});")
        else:
            lines.append(f"    return (<({cast_params}) => {ret}>{cache_var})({params_call});")
        lines.append("}")
        lines.append("")

    return "\n".join(lines)

def main():
    gl_funcs = parse_gl_file(GL_FILE)
    wgl = wgl_funcs()
    all_funcs = gl_funcs + wgl
    names = set()
    dups = []
    for n, _, _ in all_funcs:
        if n in names:
            dups.append(n)
        names.add(n)
    print(f"GL functions parsed: {len(gl_funcs)}")
    print(f"WGL functions:       {len(wgl)}")
    print(f"Total:               {len(all_funcs)}")
    print(f"Duplicates:          {dups}")
    src = emit(all_funcs)
    with open(OUT_FILE, "w", encoding="utf-8", newline="\n") as f:
        f.write(src)
    print(f"Written: {OUT_FILE}")

if __name__ == "__main__":
    main()
