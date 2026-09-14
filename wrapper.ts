// AUTO-GENERATED opengl32.dll interception/passthrough wrapper.
// Every exported function below re-exports under the SAME name as the real
// opengl32.dll, resolves the real implementation from the genuine system DLL
// (loaded once, by absolute path so this proxy never loads itself), and
// forwards the call unchanged. Add per-function logging/interception inside
// the wrapper bodies below as needed.
//
// Every wrapper prints a debug trace (tagged [opengl32_enh]) on:
//   - module load (path used, success/failure, GetLastError() on failure)
//   - first-time symbol resolution per function (success/failure + address)
//   - every call (function name), so a loading/interception problem shows up
//     as a gap in the trace rather than a silent wrong result.
// This is debug-build noise; strip it (or gate it behind a flag) before
// using this wrapper in a real render loop.
//
// Signatures come from the Windows SDK headers (GL/GL.h, wingdi.h) for the
// documented functions, and from well-known stable signatures for a few
// undocumented-but-stable wgl entry points (wglChoosePixelFormat and friends).
//
// NOT wrapped (signature genuinely unknown/undocumented): GlmfBeginGlsBlock, GlmfCloseMetaFile, GlmfEndGlsBlock, GlmfEndPlayback, GlmfInitPlayback, GlmfPlayGlsRecord, glDebugEntry.
// Add these manually if you need them, after confirming their real signature.

declare function LoadLibraryA(libraryName: Opaque): Opaque;
declare function GetProcAddress(library: Opaque, functionName: Opaque): Opaque;
declare function GetLastError(): u32;

// Absolute path so this proxy DLL (also named opengl32.dll when deployed
// next to the target application) never resolves back to itself.
const __real_opengl32_path: Opaque = "C:\\Windows\\System32\\opengl32.dll";
let __real_opengl32: Opaque = 0;

function __ensureRealOpenGL32(): Opaque {
    if (__real_opengl32 === 0) {
        console.log("[opengl32_enh] loading real opengl32.dll from C:\\Windows\\System32\\opengl32.dll ...");
        __real_opengl32 = LoadLibraryA(__real_opengl32_path);
        if (__real_opengl32 === null) {
            console.error("[opengl32_enh] FAILED to load real opengl32.dll, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh] real opengl32.dll loaded OK");
        }
    }
    return __real_opengl32;
}

let __proc_glAccum: Opaque = 0;

@dllname("glAccum")
export function glAccum(op: u32, value: f32): void {
    console.log("[opengl32_enh] call glAccum");
    if (__proc_glAccum === 0) {
        __proc_glAccum = GetProcAddress(__ensureRealOpenGL32(), "glAccum");
        if (__proc_glAccum === null) {
            console.error("[opengl32_enh]   glAccum: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glAccum: resolved OK");
        }
    }
    (<(u32, f32) => void>__proc_glAccum)(op, value);
}

let __proc_glAlphaFunc: Opaque = 0;

@dllname("glAlphaFunc")
export function glAlphaFunc(func: u32, ref: f32): void {
    console.log("[opengl32_enh] call glAlphaFunc");
    if (__proc_glAlphaFunc === 0) {
        __proc_glAlphaFunc = GetProcAddress(__ensureRealOpenGL32(), "glAlphaFunc");
        if (__proc_glAlphaFunc === null) {
            console.error("[opengl32_enh]   glAlphaFunc: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glAlphaFunc: resolved OK");
        }
    }
    (<(u32, f32) => void>__proc_glAlphaFunc)(func, ref);
}

let __proc_glAreTexturesResident: Opaque = 0;

@dllname("glAreTexturesResident")
export function glAreTexturesResident(n: i32, textures: Opaque, residences: Opaque): u8 {
    console.log("[opengl32_enh] call glAreTexturesResident");
    if (__proc_glAreTexturesResident === 0) {
        __proc_glAreTexturesResident = GetProcAddress(__ensureRealOpenGL32(), "glAreTexturesResident");
        if (__proc_glAreTexturesResident === null) {
            console.error("[opengl32_enh]   glAreTexturesResident: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glAreTexturesResident: resolved OK");
        }
    }
    return (<(i32, Opaque, Opaque) => u8>__proc_glAreTexturesResident)(n, textures, residences);
}

let __proc_glArrayElement: Opaque = 0;

@dllname("glArrayElement")
export function glArrayElement(i: i32): void {
    console.log("[opengl32_enh] call glArrayElement");
    if (__proc_glArrayElement === 0) {
        __proc_glArrayElement = GetProcAddress(__ensureRealOpenGL32(), "glArrayElement");
        if (__proc_glArrayElement === null) {
            console.error("[opengl32_enh]   glArrayElement: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glArrayElement: resolved OK");
        }
    }
    (<(i32) => void>__proc_glArrayElement)(i);
}

let __proc_glBegin: Opaque = 0;

@dllname("glBegin")
export function glBegin(mode: u32): void {
    console.log("[opengl32_enh] call glBegin");
    if (__proc_glBegin === 0) {
        __proc_glBegin = GetProcAddress(__ensureRealOpenGL32(), "glBegin");
        if (__proc_glBegin === null) {
            console.error("[opengl32_enh]   glBegin: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glBegin: resolved OK");
        }
    }
    (<(u32) => void>__proc_glBegin)(mode);
}

let __proc_glBindTexture: Opaque = 0;

@dllname("glBindTexture")
export function glBindTexture(target: u32, texture: u32): void {
    console.log("[opengl32_enh] call glBindTexture");
    if (__proc_glBindTexture === 0) {
        __proc_glBindTexture = GetProcAddress(__ensureRealOpenGL32(), "glBindTexture");
        if (__proc_glBindTexture === null) {
            console.error("[opengl32_enh]   glBindTexture: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glBindTexture: resolved OK");
        }
    }
    (<(u32, u32) => void>__proc_glBindTexture)(target, texture);
}

let __proc_glBitmap: Opaque = 0;

@dllname("glBitmap")
export function glBitmap(width: i32, height: i32, xorig: f32, yorig: f32, xmove: f32, ymove: f32, bitmap: Opaque): void {
    console.log("[opengl32_enh] call glBitmap");
    if (__proc_glBitmap === 0) {
        __proc_glBitmap = GetProcAddress(__ensureRealOpenGL32(), "glBitmap");
        if (__proc_glBitmap === null) {
            console.error("[opengl32_enh]   glBitmap: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glBitmap: resolved OK");
        }
    }
    (<(i32, i32, f32, f32, f32, f32, Opaque) => void>__proc_glBitmap)(width, height, xorig, yorig, xmove, ymove, bitmap);
}

let __proc_glBlendFunc: Opaque = 0;

@dllname("glBlendFunc")
export function glBlendFunc(sfactor: u32, dfactor: u32): void {
    console.log("[opengl32_enh] call glBlendFunc");
    if (__proc_glBlendFunc === 0) {
        __proc_glBlendFunc = GetProcAddress(__ensureRealOpenGL32(), "glBlendFunc");
        if (__proc_glBlendFunc === null) {
            console.error("[opengl32_enh]   glBlendFunc: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glBlendFunc: resolved OK");
        }
    }
    (<(u32, u32) => void>__proc_glBlendFunc)(sfactor, dfactor);
}

let __proc_glCallList: Opaque = 0;

@dllname("glCallList")
export function glCallList(list: u32): void {
    console.log("[opengl32_enh] call glCallList");
    if (__proc_glCallList === 0) {
        __proc_glCallList = GetProcAddress(__ensureRealOpenGL32(), "glCallList");
        if (__proc_glCallList === null) {
            console.error("[opengl32_enh]   glCallList: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCallList: resolved OK");
        }
    }
    (<(u32) => void>__proc_glCallList)(list);
}

let __proc_glCallLists: Opaque = 0;

@dllname("glCallLists")
export function glCallLists(n: i32, type: u32, lists: Opaque): void {
    console.log("[opengl32_enh] call glCallLists");
    if (__proc_glCallLists === 0) {
        __proc_glCallLists = GetProcAddress(__ensureRealOpenGL32(), "glCallLists");
        if (__proc_glCallLists === null) {
            console.error("[opengl32_enh]   glCallLists: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCallLists: resolved OK");
        }
    }
    (<(i32, u32, Opaque) => void>__proc_glCallLists)(n, type, lists);
}

let __proc_glClear: Opaque = 0;

@dllname("glClear")
export function glClear(mask: u32): void {
    console.log("[opengl32_enh] call glClear");
    if (__proc_glClear === 0) {
        __proc_glClear = GetProcAddress(__ensureRealOpenGL32(), "glClear");
        if (__proc_glClear === null) {
            console.error("[opengl32_enh]   glClear: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClear: resolved OK");
        }
    }
    (<(u32) => void>__proc_glClear)(mask);
}

let __proc_glClearAccum: Opaque = 0;

@dllname("glClearAccum")
export function glClearAccum(red: f32, green: f32, blue: f32, alpha: f32): void {
    console.log("[opengl32_enh] call glClearAccum");
    if (__proc_glClearAccum === 0) {
        __proc_glClearAccum = GetProcAddress(__ensureRealOpenGL32(), "glClearAccum");
        if (__proc_glClearAccum === null) {
            console.error("[opengl32_enh]   glClearAccum: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClearAccum: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glClearAccum)(red, green, blue, alpha);
}

let __proc_glClearColor: Opaque = 0;

@dllname("glClearColor")
export function glClearColor(red: f32, green: f32, blue: f32, alpha: f32): void {
    console.log("[opengl32_enh] call glClearColor");
    if (__proc_glClearColor === 0) {
        __proc_glClearColor = GetProcAddress(__ensureRealOpenGL32(), "glClearColor");
        if (__proc_glClearColor === null) {
            console.error("[opengl32_enh]   glClearColor: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClearColor: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glClearColor)(red, green, blue, alpha);
}

let __proc_glClearDepth: Opaque = 0;

@dllname("glClearDepth")
export function glClearDepth(depth: f64): void {
    console.log("[opengl32_enh] call glClearDepth");
    if (__proc_glClearDepth === 0) {
        __proc_glClearDepth = GetProcAddress(__ensureRealOpenGL32(), "glClearDepth");
        if (__proc_glClearDepth === null) {
            console.error("[opengl32_enh]   glClearDepth: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClearDepth: resolved OK");
        }
    }
    (<(f64) => void>__proc_glClearDepth)(depth);
}

let __proc_glClearIndex: Opaque = 0;

@dllname("glClearIndex")
export function glClearIndex(c: f32): void {
    console.log("[opengl32_enh] call glClearIndex");
    if (__proc_glClearIndex === 0) {
        __proc_glClearIndex = GetProcAddress(__ensureRealOpenGL32(), "glClearIndex");
        if (__proc_glClearIndex === null) {
            console.error("[opengl32_enh]   glClearIndex: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClearIndex: resolved OK");
        }
    }
    (<(f32) => void>__proc_glClearIndex)(c);
}

let __proc_glClearStencil: Opaque = 0;

@dllname("glClearStencil")
export function glClearStencil(s: i32): void {
    console.log("[opengl32_enh] call glClearStencil");
    if (__proc_glClearStencil === 0) {
        __proc_glClearStencil = GetProcAddress(__ensureRealOpenGL32(), "glClearStencil");
        if (__proc_glClearStencil === null) {
            console.error("[opengl32_enh]   glClearStencil: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClearStencil: resolved OK");
        }
    }
    (<(i32) => void>__proc_glClearStencil)(s);
}

let __proc_glClipPlane: Opaque = 0;

@dllname("glClipPlane")
export function glClipPlane(plane: u32, equation: Opaque): void {
    console.log("[opengl32_enh] call glClipPlane");
    if (__proc_glClipPlane === 0) {
        __proc_glClipPlane = GetProcAddress(__ensureRealOpenGL32(), "glClipPlane");
        if (__proc_glClipPlane === null) {
            console.error("[opengl32_enh]   glClipPlane: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glClipPlane: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glClipPlane)(plane, equation);
}

let __proc_glColor3b: Opaque = 0;

@dllname("glColor3b")
export function glColor3b(red: i8, green: i8, blue: i8): void {
    console.log("[opengl32_enh] call glColor3b");
    if (__proc_glColor3b === 0) {
        __proc_glColor3b = GetProcAddress(__ensureRealOpenGL32(), "glColor3b");
        if (__proc_glColor3b === null) {
            console.error("[opengl32_enh]   glColor3b: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3b: resolved OK");
        }
    }
    (<(i8, i8, i8) => void>__proc_glColor3b)(red, green, blue);
}

let __proc_glColor3bv: Opaque = 0;

@dllname("glColor3bv")
export function glColor3bv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3bv");
    if (__proc_glColor3bv === 0) {
        __proc_glColor3bv = GetProcAddress(__ensureRealOpenGL32(), "glColor3bv");
        if (__proc_glColor3bv === null) {
            console.error("[opengl32_enh]   glColor3bv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3bv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3bv)(v);
}

let __proc_glColor3d: Opaque = 0;

@dllname("glColor3d")
export function glColor3d(red: f64, green: f64, blue: f64): void {
    console.log("[opengl32_enh] call glColor3d");
    if (__proc_glColor3d === 0) {
        __proc_glColor3d = GetProcAddress(__ensureRealOpenGL32(), "glColor3d");
        if (__proc_glColor3d === null) {
            console.error("[opengl32_enh]   glColor3d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3d: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glColor3d)(red, green, blue);
}

let __proc_glColor3dv: Opaque = 0;

@dllname("glColor3dv")
export function glColor3dv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3dv");
    if (__proc_glColor3dv === 0) {
        __proc_glColor3dv = GetProcAddress(__ensureRealOpenGL32(), "glColor3dv");
        if (__proc_glColor3dv === null) {
            console.error("[opengl32_enh]   glColor3dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3dv)(v);
}

let __proc_glColor3f: Opaque = 0;

@dllname("glColor3f")
export function glColor3f(red: f32, green: f32, blue: f32): void {
    console.log("[opengl32_enh] call glColor3f");
    if (__proc_glColor3f === 0) {
        __proc_glColor3f = GetProcAddress(__ensureRealOpenGL32(), "glColor3f");
        if (__proc_glColor3f === null) {
            console.error("[opengl32_enh]   glColor3f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3f: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glColor3f)(red, green, blue);
}

let __proc_glColor3fv: Opaque = 0;

@dllname("glColor3fv")
export function glColor3fv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3fv");
    if (__proc_glColor3fv === 0) {
        __proc_glColor3fv = GetProcAddress(__ensureRealOpenGL32(), "glColor3fv");
        if (__proc_glColor3fv === null) {
            console.error("[opengl32_enh]   glColor3fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3fv)(v);
}

let __proc_glColor3i: Opaque = 0;

@dllname("glColor3i")
export function glColor3i(red: i32, green: i32, blue: i32): void {
    console.log("[opengl32_enh] call glColor3i");
    if (__proc_glColor3i === 0) {
        __proc_glColor3i = GetProcAddress(__ensureRealOpenGL32(), "glColor3i");
        if (__proc_glColor3i === null) {
            console.error("[opengl32_enh]   glColor3i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3i: resolved OK");
        }
    }
    (<(i32, i32, i32) => void>__proc_glColor3i)(red, green, blue);
}

let __proc_glColor3iv: Opaque = 0;

@dllname("glColor3iv")
export function glColor3iv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3iv");
    if (__proc_glColor3iv === 0) {
        __proc_glColor3iv = GetProcAddress(__ensureRealOpenGL32(), "glColor3iv");
        if (__proc_glColor3iv === null) {
            console.error("[opengl32_enh]   glColor3iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3iv)(v);
}

let __proc_glColor3s: Opaque = 0;

@dllname("glColor3s")
export function glColor3s(red: i16, green: i16, blue: i16): void {
    console.log("[opengl32_enh] call glColor3s");
    if (__proc_glColor3s === 0) {
        __proc_glColor3s = GetProcAddress(__ensureRealOpenGL32(), "glColor3s");
        if (__proc_glColor3s === null) {
            console.error("[opengl32_enh]   glColor3s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3s: resolved OK");
        }
    }
    (<(i16, i16, i16) => void>__proc_glColor3s)(red, green, blue);
}

let __proc_glColor3sv: Opaque = 0;

@dllname("glColor3sv")
export function glColor3sv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3sv");
    if (__proc_glColor3sv === 0) {
        __proc_glColor3sv = GetProcAddress(__ensureRealOpenGL32(), "glColor3sv");
        if (__proc_glColor3sv === null) {
            console.error("[opengl32_enh]   glColor3sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3sv)(v);
}

let __proc_glColor3ub: Opaque = 0;

@dllname("glColor3ub")
export function glColor3ub(red: u8, green: u8, blue: u8): void {
    console.log("[opengl32_enh] call glColor3ub");
    if (__proc_glColor3ub === 0) {
        __proc_glColor3ub = GetProcAddress(__ensureRealOpenGL32(), "glColor3ub");
        if (__proc_glColor3ub === null) {
            console.error("[opengl32_enh]   glColor3ub: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3ub: resolved OK");
        }
    }
    (<(u8, u8, u8) => void>__proc_glColor3ub)(red, green, blue);
}

let __proc_glColor3ubv: Opaque = 0;

@dllname("glColor3ubv")
export function glColor3ubv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3ubv");
    if (__proc_glColor3ubv === 0) {
        __proc_glColor3ubv = GetProcAddress(__ensureRealOpenGL32(), "glColor3ubv");
        if (__proc_glColor3ubv === null) {
            console.error("[opengl32_enh]   glColor3ubv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3ubv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3ubv)(v);
}

let __proc_glColor3ui: Opaque = 0;

@dllname("glColor3ui")
export function glColor3ui(red: u32, green: u32, blue: u32): void {
    console.log("[opengl32_enh] call glColor3ui");
    if (__proc_glColor3ui === 0) {
        __proc_glColor3ui = GetProcAddress(__ensureRealOpenGL32(), "glColor3ui");
        if (__proc_glColor3ui === null) {
            console.error("[opengl32_enh]   glColor3ui: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3ui: resolved OK");
        }
    }
    (<(u32, u32, u32) => void>__proc_glColor3ui)(red, green, blue);
}

let __proc_glColor3uiv: Opaque = 0;

@dllname("glColor3uiv")
export function glColor3uiv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3uiv");
    if (__proc_glColor3uiv === 0) {
        __proc_glColor3uiv = GetProcAddress(__ensureRealOpenGL32(), "glColor3uiv");
        if (__proc_glColor3uiv === null) {
            console.error("[opengl32_enh]   glColor3uiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3uiv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3uiv)(v);
}

let __proc_glColor3us: Opaque = 0;

@dllname("glColor3us")
export function glColor3us(red: u16, green: u16, blue: u16): void {
    console.log("[opengl32_enh] call glColor3us");
    if (__proc_glColor3us === 0) {
        __proc_glColor3us = GetProcAddress(__ensureRealOpenGL32(), "glColor3us");
        if (__proc_glColor3us === null) {
            console.error("[opengl32_enh]   glColor3us: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3us: resolved OK");
        }
    }
    (<(u16, u16, u16) => void>__proc_glColor3us)(red, green, blue);
}

let __proc_glColor3usv: Opaque = 0;

@dllname("glColor3usv")
export function glColor3usv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor3usv");
    if (__proc_glColor3usv === 0) {
        __proc_glColor3usv = GetProcAddress(__ensureRealOpenGL32(), "glColor3usv");
        if (__proc_glColor3usv === null) {
            console.error("[opengl32_enh]   glColor3usv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor3usv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor3usv)(v);
}

let __proc_glColor4b: Opaque = 0;

@dllname("glColor4b")
export function glColor4b(red: i8, green: i8, blue: i8, alpha: i8): void {
    console.log("[opengl32_enh] call glColor4b");
    if (__proc_glColor4b === 0) {
        __proc_glColor4b = GetProcAddress(__ensureRealOpenGL32(), "glColor4b");
        if (__proc_glColor4b === null) {
            console.error("[opengl32_enh]   glColor4b: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4b: resolved OK");
        }
    }
    (<(i8, i8, i8, i8) => void>__proc_glColor4b)(red, green, blue, alpha);
}

let __proc_glColor4bv: Opaque = 0;

@dllname("glColor4bv")
export function glColor4bv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4bv");
    if (__proc_glColor4bv === 0) {
        __proc_glColor4bv = GetProcAddress(__ensureRealOpenGL32(), "glColor4bv");
        if (__proc_glColor4bv === null) {
            console.error("[opengl32_enh]   glColor4bv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4bv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4bv)(v);
}

let __proc_glColor4d: Opaque = 0;

@dllname("glColor4d")
export function glColor4d(red: f64, green: f64, blue: f64, alpha: f64): void {
    console.log("[opengl32_enh] call glColor4d");
    if (__proc_glColor4d === 0) {
        __proc_glColor4d = GetProcAddress(__ensureRealOpenGL32(), "glColor4d");
        if (__proc_glColor4d === null) {
            console.error("[opengl32_enh]   glColor4d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4d: resolved OK");
        }
    }
    (<(f64, f64, f64, f64) => void>__proc_glColor4d)(red, green, blue, alpha);
}

let __proc_glColor4dv: Opaque = 0;

@dllname("glColor4dv")
export function glColor4dv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4dv");
    if (__proc_glColor4dv === 0) {
        __proc_glColor4dv = GetProcAddress(__ensureRealOpenGL32(), "glColor4dv");
        if (__proc_glColor4dv === null) {
            console.error("[opengl32_enh]   glColor4dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4dv)(v);
}

let __proc_glColor4f: Opaque = 0;

@dllname("glColor4f")
export function glColor4f(red: f32, green: f32, blue: f32, alpha: f32): void {
    console.log("[opengl32_enh] call glColor4f");
    if (__proc_glColor4f === 0) {
        __proc_glColor4f = GetProcAddress(__ensureRealOpenGL32(), "glColor4f");
        if (__proc_glColor4f === null) {
            console.error("[opengl32_enh]   glColor4f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4f: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glColor4f)(red, green, blue, alpha);
}

let __proc_glColor4fv: Opaque = 0;

@dllname("glColor4fv")
export function glColor4fv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4fv");
    if (__proc_glColor4fv === 0) {
        __proc_glColor4fv = GetProcAddress(__ensureRealOpenGL32(), "glColor4fv");
        if (__proc_glColor4fv === null) {
            console.error("[opengl32_enh]   glColor4fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4fv)(v);
}

let __proc_glColor4i: Opaque = 0;

@dllname("glColor4i")
export function glColor4i(red: i32, green: i32, blue: i32, alpha: i32): void {
    console.log("[opengl32_enh] call glColor4i");
    if (__proc_glColor4i === 0) {
        __proc_glColor4i = GetProcAddress(__ensureRealOpenGL32(), "glColor4i");
        if (__proc_glColor4i === null) {
            console.error("[opengl32_enh]   glColor4i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4i: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glColor4i)(red, green, blue, alpha);
}

let __proc_glColor4iv: Opaque = 0;

@dllname("glColor4iv")
export function glColor4iv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4iv");
    if (__proc_glColor4iv === 0) {
        __proc_glColor4iv = GetProcAddress(__ensureRealOpenGL32(), "glColor4iv");
        if (__proc_glColor4iv === null) {
            console.error("[opengl32_enh]   glColor4iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4iv)(v);
}

let __proc_glColor4s: Opaque = 0;

@dllname("glColor4s")
export function glColor4s(red: i16, green: i16, blue: i16, alpha: i16): void {
    console.log("[opengl32_enh] call glColor4s");
    if (__proc_glColor4s === 0) {
        __proc_glColor4s = GetProcAddress(__ensureRealOpenGL32(), "glColor4s");
        if (__proc_glColor4s === null) {
            console.error("[opengl32_enh]   glColor4s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4s: resolved OK");
        }
    }
    (<(i16, i16, i16, i16) => void>__proc_glColor4s)(red, green, blue, alpha);
}

let __proc_glColor4sv: Opaque = 0;

@dllname("glColor4sv")
export function glColor4sv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4sv");
    if (__proc_glColor4sv === 0) {
        __proc_glColor4sv = GetProcAddress(__ensureRealOpenGL32(), "glColor4sv");
        if (__proc_glColor4sv === null) {
            console.error("[opengl32_enh]   glColor4sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4sv)(v);
}

let __proc_glColor4ub: Opaque = 0;

@dllname("glColor4ub")
export function glColor4ub(red: u8, green: u8, blue: u8, alpha: u8): void {
    console.log("[opengl32_enh] call glColor4ub");
    if (__proc_glColor4ub === 0) {
        __proc_glColor4ub = GetProcAddress(__ensureRealOpenGL32(), "glColor4ub");
        if (__proc_glColor4ub === null) {
            console.error("[opengl32_enh]   glColor4ub: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4ub: resolved OK");
        }
    }
    (<(u8, u8, u8, u8) => void>__proc_glColor4ub)(red, green, blue, alpha);
}

let __proc_glColor4ubv: Opaque = 0;

@dllname("glColor4ubv")
export function glColor4ubv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4ubv");
    if (__proc_glColor4ubv === 0) {
        __proc_glColor4ubv = GetProcAddress(__ensureRealOpenGL32(), "glColor4ubv");
        if (__proc_glColor4ubv === null) {
            console.error("[opengl32_enh]   glColor4ubv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4ubv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4ubv)(v);
}

let __proc_glColor4ui: Opaque = 0;

@dllname("glColor4ui")
export function glColor4ui(red: u32, green: u32, blue: u32, alpha: u32): void {
    console.log("[opengl32_enh] call glColor4ui");
    if (__proc_glColor4ui === 0) {
        __proc_glColor4ui = GetProcAddress(__ensureRealOpenGL32(), "glColor4ui");
        if (__proc_glColor4ui === null) {
            console.error("[opengl32_enh]   glColor4ui: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4ui: resolved OK");
        }
    }
    (<(u32, u32, u32, u32) => void>__proc_glColor4ui)(red, green, blue, alpha);
}

let __proc_glColor4uiv: Opaque = 0;

@dllname("glColor4uiv")
export function glColor4uiv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4uiv");
    if (__proc_glColor4uiv === 0) {
        __proc_glColor4uiv = GetProcAddress(__ensureRealOpenGL32(), "glColor4uiv");
        if (__proc_glColor4uiv === null) {
            console.error("[opengl32_enh]   glColor4uiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4uiv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4uiv)(v);
}

let __proc_glColor4us: Opaque = 0;

@dllname("glColor4us")
export function glColor4us(red: u16, green: u16, blue: u16, alpha: u16): void {
    console.log("[opengl32_enh] call glColor4us");
    if (__proc_glColor4us === 0) {
        __proc_glColor4us = GetProcAddress(__ensureRealOpenGL32(), "glColor4us");
        if (__proc_glColor4us === null) {
            console.error("[opengl32_enh]   glColor4us: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4us: resolved OK");
        }
    }
    (<(u16, u16, u16, u16) => void>__proc_glColor4us)(red, green, blue, alpha);
}

let __proc_glColor4usv: Opaque = 0;

@dllname("glColor4usv")
export function glColor4usv(v: Opaque): void {
    console.log("[opengl32_enh] call glColor4usv");
    if (__proc_glColor4usv === 0) {
        __proc_glColor4usv = GetProcAddress(__ensureRealOpenGL32(), "glColor4usv");
        if (__proc_glColor4usv === null) {
            console.error("[opengl32_enh]   glColor4usv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColor4usv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glColor4usv)(v);
}

let __proc_glColorMask: Opaque = 0;

@dllname("glColorMask")
export function glColorMask(red: u8, green: u8, blue: u8, alpha: u8): void {
    console.log("[opengl32_enh] call glColorMask");
    if (__proc_glColorMask === 0) {
        __proc_glColorMask = GetProcAddress(__ensureRealOpenGL32(), "glColorMask");
        if (__proc_glColorMask === null) {
            console.error("[opengl32_enh]   glColorMask: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColorMask: resolved OK");
        }
    }
    (<(u8, u8, u8, u8) => void>__proc_glColorMask)(red, green, blue, alpha);
}

let __proc_glColorMaterial: Opaque = 0;

@dllname("glColorMaterial")
export function glColorMaterial(face: u32, mode: u32): void {
    console.log("[opengl32_enh] call glColorMaterial");
    if (__proc_glColorMaterial === 0) {
        __proc_glColorMaterial = GetProcAddress(__ensureRealOpenGL32(), "glColorMaterial");
        if (__proc_glColorMaterial === null) {
            console.error("[opengl32_enh]   glColorMaterial: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColorMaterial: resolved OK");
        }
    }
    (<(u32, u32) => void>__proc_glColorMaterial)(face, mode);
}

let __proc_glColorPointer: Opaque = 0;

@dllname("glColorPointer")
export function glColorPointer(size: i32, type: u32, stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glColorPointer");
    if (__proc_glColorPointer === 0) {
        __proc_glColorPointer = GetProcAddress(__ensureRealOpenGL32(), "glColorPointer");
        if (__proc_glColorPointer === null) {
            console.error("[opengl32_enh]   glColorPointer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glColorPointer: resolved OK");
        }
    }
    (<(i32, u32, i32, Opaque) => void>__proc_glColorPointer)(size, type, stride, pointer);
}

let __proc_glCopyPixels: Opaque = 0;

@dllname("glCopyPixels")
export function glCopyPixels(x: i32, y: i32, width: i32, height: i32, type: u32): void {
    console.log("[opengl32_enh] call glCopyPixels");
    if (__proc_glCopyPixels === 0) {
        __proc_glCopyPixels = GetProcAddress(__ensureRealOpenGL32(), "glCopyPixels");
        if (__proc_glCopyPixels === null) {
            console.error("[opengl32_enh]   glCopyPixels: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCopyPixels: resolved OK");
        }
    }
    (<(i32, i32, i32, i32, u32) => void>__proc_glCopyPixels)(x, y, width, height, type);
}

let __proc_glCopyTexImage1D: Opaque = 0;

@dllname("glCopyTexImage1D")
export function glCopyTexImage1D(target: u32, level: i32, internalFormat: u32, x: i32, y: i32, width: i32, border: i32): void {
    console.log("[opengl32_enh] call glCopyTexImage1D");
    if (__proc_glCopyTexImage1D === 0) {
        __proc_glCopyTexImage1D = GetProcAddress(__ensureRealOpenGL32(), "glCopyTexImage1D");
        if (__proc_glCopyTexImage1D === null) {
            console.error("[opengl32_enh]   glCopyTexImage1D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCopyTexImage1D: resolved OK");
        }
    }
    (<(u32, i32, u32, i32, i32, i32, i32) => void>__proc_glCopyTexImage1D)(target, level, internalFormat, x, y, width, border);
}

let __proc_glCopyTexImage2D: Opaque = 0;

@dllname("glCopyTexImage2D")
export function glCopyTexImage2D(target: u32, level: i32, internalFormat: u32, x: i32, y: i32, width: i32, height: i32, border: i32): void {
    console.log("[opengl32_enh] call glCopyTexImage2D");
    if (__proc_glCopyTexImage2D === 0) {
        __proc_glCopyTexImage2D = GetProcAddress(__ensureRealOpenGL32(), "glCopyTexImage2D");
        if (__proc_glCopyTexImage2D === null) {
            console.error("[opengl32_enh]   glCopyTexImage2D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCopyTexImage2D: resolved OK");
        }
    }
    (<(u32, i32, u32, i32, i32, i32, i32, i32) => void>__proc_glCopyTexImage2D)(target, level, internalFormat, x, y, width, height, border);
}

let __proc_glCopyTexSubImage1D: Opaque = 0;

@dllname("glCopyTexSubImage1D")
export function glCopyTexSubImage1D(target: u32, level: i32, xoffset: i32, x: i32, y: i32, width: i32): void {
    console.log("[opengl32_enh] call glCopyTexSubImage1D");
    if (__proc_glCopyTexSubImage1D === 0) {
        __proc_glCopyTexSubImage1D = GetProcAddress(__ensureRealOpenGL32(), "glCopyTexSubImage1D");
        if (__proc_glCopyTexSubImage1D === null) {
            console.error("[opengl32_enh]   glCopyTexSubImage1D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCopyTexSubImage1D: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, i32, i32) => void>__proc_glCopyTexSubImage1D)(target, level, xoffset, x, y, width);
}

let __proc_glCopyTexSubImage2D: Opaque = 0;

@dllname("glCopyTexSubImage2D")
export function glCopyTexSubImage2D(target: u32, level: i32, xoffset: i32, yoffset: i32, x: i32, y: i32, width: i32, height: i32): void {
    console.log("[opengl32_enh] call glCopyTexSubImage2D");
    if (__proc_glCopyTexSubImage2D === 0) {
        __proc_glCopyTexSubImage2D = GetProcAddress(__ensureRealOpenGL32(), "glCopyTexSubImage2D");
        if (__proc_glCopyTexSubImage2D === null) {
            console.error("[opengl32_enh]   glCopyTexSubImage2D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCopyTexSubImage2D: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, i32, i32, i32, i32) => void>__proc_glCopyTexSubImage2D)(target, level, xoffset, yoffset, x, y, width, height);
}

let __proc_glCullFace: Opaque = 0;

@dllname("glCullFace")
export function glCullFace(mode: u32): void {
    console.log("[opengl32_enh] call glCullFace");
    if (__proc_glCullFace === 0) {
        __proc_glCullFace = GetProcAddress(__ensureRealOpenGL32(), "glCullFace");
        if (__proc_glCullFace === null) {
            console.error("[opengl32_enh]   glCullFace: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glCullFace: resolved OK");
        }
    }
    (<(u32) => void>__proc_glCullFace)(mode);
}

let __proc_glDeleteLists: Opaque = 0;

@dllname("glDeleteLists")
export function glDeleteLists(list: u32, range: i32): void {
    console.log("[opengl32_enh] call glDeleteLists");
    if (__proc_glDeleteLists === 0) {
        __proc_glDeleteLists = GetProcAddress(__ensureRealOpenGL32(), "glDeleteLists");
        if (__proc_glDeleteLists === null) {
            console.error("[opengl32_enh]   glDeleteLists: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDeleteLists: resolved OK");
        }
    }
    (<(u32, i32) => void>__proc_glDeleteLists)(list, range);
}

let __proc_glDeleteTextures: Opaque = 0;

@dllname("glDeleteTextures")
export function glDeleteTextures(n: i32, textures: Opaque): void {
    console.log("[opengl32_enh] call glDeleteTextures");
    if (__proc_glDeleteTextures === 0) {
        __proc_glDeleteTextures = GetProcAddress(__ensureRealOpenGL32(), "glDeleteTextures");
        if (__proc_glDeleteTextures === null) {
            console.error("[opengl32_enh]   glDeleteTextures: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDeleteTextures: resolved OK");
        }
    }
    (<(i32, Opaque) => void>__proc_glDeleteTextures)(n, textures);
}

let __proc_glDepthFunc: Opaque = 0;

@dllname("glDepthFunc")
export function glDepthFunc(func: u32): void {
    console.log("[opengl32_enh] call glDepthFunc");
    if (__proc_glDepthFunc === 0) {
        __proc_glDepthFunc = GetProcAddress(__ensureRealOpenGL32(), "glDepthFunc");
        if (__proc_glDepthFunc === null) {
            console.error("[opengl32_enh]   glDepthFunc: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDepthFunc: resolved OK");
        }
    }
    (<(u32) => void>__proc_glDepthFunc)(func);
}

let __proc_glDepthMask: Opaque = 0;

@dllname("glDepthMask")
export function glDepthMask(flag: u8): void {
    console.log("[opengl32_enh] call glDepthMask");
    if (__proc_glDepthMask === 0) {
        __proc_glDepthMask = GetProcAddress(__ensureRealOpenGL32(), "glDepthMask");
        if (__proc_glDepthMask === null) {
            console.error("[opengl32_enh]   glDepthMask: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDepthMask: resolved OK");
        }
    }
    (<(u8) => void>__proc_glDepthMask)(flag);
}

let __proc_glDepthRange: Opaque = 0;

@dllname("glDepthRange")
export function glDepthRange(zNear: f64, zFar: f64): void {
    console.log("[opengl32_enh] call glDepthRange");
    if (__proc_glDepthRange === 0) {
        __proc_glDepthRange = GetProcAddress(__ensureRealOpenGL32(), "glDepthRange");
        if (__proc_glDepthRange === null) {
            console.error("[opengl32_enh]   glDepthRange: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDepthRange: resolved OK");
        }
    }
    (<(f64, f64) => void>__proc_glDepthRange)(zNear, zFar);
}

let __proc_glDisable: Opaque = 0;

@dllname("glDisable")
export function glDisable(cap: u32): void {
    console.log("[opengl32_enh] call glDisable");
    if (__proc_glDisable === 0) {
        __proc_glDisable = GetProcAddress(__ensureRealOpenGL32(), "glDisable");
        if (__proc_glDisable === null) {
            console.error("[opengl32_enh]   glDisable: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDisable: resolved OK");
        }
    }
    (<(u32) => void>__proc_glDisable)(cap);
}

let __proc_glDisableClientState: Opaque = 0;

@dllname("glDisableClientState")
export function glDisableClientState(array: u32): void {
    console.log("[opengl32_enh] call glDisableClientState");
    if (__proc_glDisableClientState === 0) {
        __proc_glDisableClientState = GetProcAddress(__ensureRealOpenGL32(), "glDisableClientState");
        if (__proc_glDisableClientState === null) {
            console.error("[opengl32_enh]   glDisableClientState: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDisableClientState: resolved OK");
        }
    }
    (<(u32) => void>__proc_glDisableClientState)(array);
}

let __proc_glDrawArrays: Opaque = 0;

@dllname("glDrawArrays")
export function glDrawArrays(mode: u32, first: i32, count: i32): void {
    console.log("[opengl32_enh] call glDrawArrays");
    if (__proc_glDrawArrays === 0) {
        __proc_glDrawArrays = GetProcAddress(__ensureRealOpenGL32(), "glDrawArrays");
        if (__proc_glDrawArrays === null) {
            console.error("[opengl32_enh]   glDrawArrays: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDrawArrays: resolved OK");
        }
    }
    (<(u32, i32, i32) => void>__proc_glDrawArrays)(mode, first, count);
}

let __proc_glDrawBuffer: Opaque = 0;

@dllname("glDrawBuffer")
export function glDrawBuffer(mode: u32): void {
    console.log("[opengl32_enh] call glDrawBuffer");
    if (__proc_glDrawBuffer === 0) {
        __proc_glDrawBuffer = GetProcAddress(__ensureRealOpenGL32(), "glDrawBuffer");
        if (__proc_glDrawBuffer === null) {
            console.error("[opengl32_enh]   glDrawBuffer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDrawBuffer: resolved OK");
        }
    }
    (<(u32) => void>__proc_glDrawBuffer)(mode);
}

let __proc_glDrawElements: Opaque = 0;

@dllname("glDrawElements")
export function glDrawElements(mode: u32, count: i32, type: u32, indices: Opaque): void {
    console.log("[opengl32_enh] call glDrawElements");
    if (__proc_glDrawElements === 0) {
        __proc_glDrawElements = GetProcAddress(__ensureRealOpenGL32(), "glDrawElements");
        if (__proc_glDrawElements === null) {
            console.error("[opengl32_enh]   glDrawElements: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDrawElements: resolved OK");
        }
    }
    (<(u32, i32, u32, Opaque) => void>__proc_glDrawElements)(mode, count, type, indices);
}

let __proc_glDrawPixels: Opaque = 0;

@dllname("glDrawPixels")
export function glDrawPixels(width: i32, height: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glDrawPixels");
    if (__proc_glDrawPixels === 0) {
        __proc_glDrawPixels = GetProcAddress(__ensureRealOpenGL32(), "glDrawPixels");
        if (__proc_glDrawPixels === null) {
            console.error("[opengl32_enh]   glDrawPixels: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glDrawPixels: resolved OK");
        }
    }
    (<(i32, i32, u32, u32, Opaque) => void>__proc_glDrawPixels)(width, height, format, type, pixels);
}

let __proc_glEdgeFlag: Opaque = 0;

@dllname("glEdgeFlag")
export function glEdgeFlag(flag: u8): void {
    console.log("[opengl32_enh] call glEdgeFlag");
    if (__proc_glEdgeFlag === 0) {
        __proc_glEdgeFlag = GetProcAddress(__ensureRealOpenGL32(), "glEdgeFlag");
        if (__proc_glEdgeFlag === null) {
            console.error("[opengl32_enh]   glEdgeFlag: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEdgeFlag: resolved OK");
        }
    }
    (<(u8) => void>__proc_glEdgeFlag)(flag);
}

let __proc_glEdgeFlagPointer: Opaque = 0;

@dllname("glEdgeFlagPointer")
export function glEdgeFlagPointer(stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glEdgeFlagPointer");
    if (__proc_glEdgeFlagPointer === 0) {
        __proc_glEdgeFlagPointer = GetProcAddress(__ensureRealOpenGL32(), "glEdgeFlagPointer");
        if (__proc_glEdgeFlagPointer === null) {
            console.error("[opengl32_enh]   glEdgeFlagPointer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEdgeFlagPointer: resolved OK");
        }
    }
    (<(i32, Opaque) => void>__proc_glEdgeFlagPointer)(stride, pointer);
}

let __proc_glEdgeFlagv: Opaque = 0;

@dllname("glEdgeFlagv")
export function glEdgeFlagv(flag: Opaque): void {
    console.log("[opengl32_enh] call glEdgeFlagv");
    if (__proc_glEdgeFlagv === 0) {
        __proc_glEdgeFlagv = GetProcAddress(__ensureRealOpenGL32(), "glEdgeFlagv");
        if (__proc_glEdgeFlagv === null) {
            console.error("[opengl32_enh]   glEdgeFlagv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEdgeFlagv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glEdgeFlagv)(flag);
}

let __proc_glEnable: Opaque = 0;

@dllname("glEnable")
export function glEnable(cap: u32): void {
    console.log("[opengl32_enh] call glEnable");
    if (__proc_glEnable === 0) {
        __proc_glEnable = GetProcAddress(__ensureRealOpenGL32(), "glEnable");
        if (__proc_glEnable === null) {
            console.error("[opengl32_enh]   glEnable: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEnable: resolved OK");
        }
    }
    (<(u32) => void>__proc_glEnable)(cap);
}

let __proc_glEnableClientState: Opaque = 0;

@dllname("glEnableClientState")
export function glEnableClientState(array: u32): void {
    console.log("[opengl32_enh] call glEnableClientState");
    if (__proc_glEnableClientState === 0) {
        __proc_glEnableClientState = GetProcAddress(__ensureRealOpenGL32(), "glEnableClientState");
        if (__proc_glEnableClientState === null) {
            console.error("[opengl32_enh]   glEnableClientState: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEnableClientState: resolved OK");
        }
    }
    (<(u32) => void>__proc_glEnableClientState)(array);
}

let __proc_glEnd: Opaque = 0;

@dllname("glEnd")
export function glEnd(): void {
    console.log("[opengl32_enh] call glEnd");
    if (__proc_glEnd === 0) {
        __proc_glEnd = GetProcAddress(__ensureRealOpenGL32(), "glEnd");
        if (__proc_glEnd === null) {
            console.error("[opengl32_enh]   glEnd: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEnd: resolved OK");
        }
    }
    (<() => void>__proc_glEnd)();
}

let __proc_glEndList: Opaque = 0;

@dllname("glEndList")
export function glEndList(): void {
    console.log("[opengl32_enh] call glEndList");
    if (__proc_glEndList === 0) {
        __proc_glEndList = GetProcAddress(__ensureRealOpenGL32(), "glEndList");
        if (__proc_glEndList === null) {
            console.error("[opengl32_enh]   glEndList: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEndList: resolved OK");
        }
    }
    (<() => void>__proc_glEndList)();
}

let __proc_glEvalCoord1d: Opaque = 0;

@dllname("glEvalCoord1d")
export function glEvalCoord1d(u: f64): void {
    console.log("[opengl32_enh] call glEvalCoord1d");
    if (__proc_glEvalCoord1d === 0) {
        __proc_glEvalCoord1d = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord1d");
        if (__proc_glEvalCoord1d === null) {
            console.error("[opengl32_enh]   glEvalCoord1d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord1d: resolved OK");
        }
    }
    (<(f64) => void>__proc_glEvalCoord1d)(u);
}

let __proc_glEvalCoord1dv: Opaque = 0;

@dllname("glEvalCoord1dv")
export function glEvalCoord1dv(u: Opaque): void {
    console.log("[opengl32_enh] call glEvalCoord1dv");
    if (__proc_glEvalCoord1dv === 0) {
        __proc_glEvalCoord1dv = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord1dv");
        if (__proc_glEvalCoord1dv === null) {
            console.error("[opengl32_enh]   glEvalCoord1dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord1dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glEvalCoord1dv)(u);
}

let __proc_glEvalCoord1f: Opaque = 0;

@dllname("glEvalCoord1f")
export function glEvalCoord1f(u: f32): void {
    console.log("[opengl32_enh] call glEvalCoord1f");
    if (__proc_glEvalCoord1f === 0) {
        __proc_glEvalCoord1f = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord1f");
        if (__proc_glEvalCoord1f === null) {
            console.error("[opengl32_enh]   glEvalCoord1f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord1f: resolved OK");
        }
    }
    (<(f32) => void>__proc_glEvalCoord1f)(u);
}

let __proc_glEvalCoord1fv: Opaque = 0;

@dllname("glEvalCoord1fv")
export function glEvalCoord1fv(u: Opaque): void {
    console.log("[opengl32_enh] call glEvalCoord1fv");
    if (__proc_glEvalCoord1fv === 0) {
        __proc_glEvalCoord1fv = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord1fv");
        if (__proc_glEvalCoord1fv === null) {
            console.error("[opengl32_enh]   glEvalCoord1fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord1fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glEvalCoord1fv)(u);
}

let __proc_glEvalCoord2d: Opaque = 0;

@dllname("glEvalCoord2d")
export function glEvalCoord2d(u: f64, v: f64): void {
    console.log("[opengl32_enh] call glEvalCoord2d");
    if (__proc_glEvalCoord2d === 0) {
        __proc_glEvalCoord2d = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord2d");
        if (__proc_glEvalCoord2d === null) {
            console.error("[opengl32_enh]   glEvalCoord2d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord2d: resolved OK");
        }
    }
    (<(f64, f64) => void>__proc_glEvalCoord2d)(u, v);
}

let __proc_glEvalCoord2dv: Opaque = 0;

@dllname("glEvalCoord2dv")
export function glEvalCoord2dv(u: Opaque): void {
    console.log("[opengl32_enh] call glEvalCoord2dv");
    if (__proc_glEvalCoord2dv === 0) {
        __proc_glEvalCoord2dv = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord2dv");
        if (__proc_glEvalCoord2dv === null) {
            console.error("[opengl32_enh]   glEvalCoord2dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord2dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glEvalCoord2dv)(u);
}

let __proc_glEvalCoord2f: Opaque = 0;

@dllname("glEvalCoord2f")
export function glEvalCoord2f(u: f32, v: f32): void {
    console.log("[opengl32_enh] call glEvalCoord2f");
    if (__proc_glEvalCoord2f === 0) {
        __proc_glEvalCoord2f = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord2f");
        if (__proc_glEvalCoord2f === null) {
            console.error("[opengl32_enh]   glEvalCoord2f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord2f: resolved OK");
        }
    }
    (<(f32, f32) => void>__proc_glEvalCoord2f)(u, v);
}

let __proc_glEvalCoord2fv: Opaque = 0;

@dllname("glEvalCoord2fv")
export function glEvalCoord2fv(u: Opaque): void {
    console.log("[opengl32_enh] call glEvalCoord2fv");
    if (__proc_glEvalCoord2fv === 0) {
        __proc_glEvalCoord2fv = GetProcAddress(__ensureRealOpenGL32(), "glEvalCoord2fv");
        if (__proc_glEvalCoord2fv === null) {
            console.error("[opengl32_enh]   glEvalCoord2fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalCoord2fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glEvalCoord2fv)(u);
}

let __proc_glEvalMesh1: Opaque = 0;

@dllname("glEvalMesh1")
export function glEvalMesh1(mode: u32, i1: i32, i2: i32): void {
    console.log("[opengl32_enh] call glEvalMesh1");
    if (__proc_glEvalMesh1 === 0) {
        __proc_glEvalMesh1 = GetProcAddress(__ensureRealOpenGL32(), "glEvalMesh1");
        if (__proc_glEvalMesh1 === null) {
            console.error("[opengl32_enh]   glEvalMesh1: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalMesh1: resolved OK");
        }
    }
    (<(u32, i32, i32) => void>__proc_glEvalMesh1)(mode, i1, i2);
}

let __proc_glEvalMesh2: Opaque = 0;

@dllname("glEvalMesh2")
export function glEvalMesh2(mode: u32, i1: i32, i2: i32, j1: i32, j2: i32): void {
    console.log("[opengl32_enh] call glEvalMesh2");
    if (__proc_glEvalMesh2 === 0) {
        __proc_glEvalMesh2 = GetProcAddress(__ensureRealOpenGL32(), "glEvalMesh2");
        if (__proc_glEvalMesh2 === null) {
            console.error("[opengl32_enh]   glEvalMesh2: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalMesh2: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, i32) => void>__proc_glEvalMesh2)(mode, i1, i2, j1, j2);
}

let __proc_glEvalPoint1: Opaque = 0;

@dllname("glEvalPoint1")
export function glEvalPoint1(i: i32): void {
    console.log("[opengl32_enh] call glEvalPoint1");
    if (__proc_glEvalPoint1 === 0) {
        __proc_glEvalPoint1 = GetProcAddress(__ensureRealOpenGL32(), "glEvalPoint1");
        if (__proc_glEvalPoint1 === null) {
            console.error("[opengl32_enh]   glEvalPoint1: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalPoint1: resolved OK");
        }
    }
    (<(i32) => void>__proc_glEvalPoint1)(i);
}

let __proc_glEvalPoint2: Opaque = 0;

@dllname("glEvalPoint2")
export function glEvalPoint2(i: i32, j: i32): void {
    console.log("[opengl32_enh] call glEvalPoint2");
    if (__proc_glEvalPoint2 === 0) {
        __proc_glEvalPoint2 = GetProcAddress(__ensureRealOpenGL32(), "glEvalPoint2");
        if (__proc_glEvalPoint2 === null) {
            console.error("[opengl32_enh]   glEvalPoint2: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glEvalPoint2: resolved OK");
        }
    }
    (<(i32, i32) => void>__proc_glEvalPoint2)(i, j);
}

let __proc_glFeedbackBuffer: Opaque = 0;

@dllname("glFeedbackBuffer")
export function glFeedbackBuffer(size: i32, type: u32, buffer: Opaque): void {
    console.log("[opengl32_enh] call glFeedbackBuffer");
    if (__proc_glFeedbackBuffer === 0) {
        __proc_glFeedbackBuffer = GetProcAddress(__ensureRealOpenGL32(), "glFeedbackBuffer");
        if (__proc_glFeedbackBuffer === null) {
            console.error("[opengl32_enh]   glFeedbackBuffer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFeedbackBuffer: resolved OK");
        }
    }
    (<(i32, u32, Opaque) => void>__proc_glFeedbackBuffer)(size, type, buffer);
}

let __proc_glFinish: Opaque = 0;

@dllname("glFinish")
export function glFinish(): void {
    console.log("[opengl32_enh] call glFinish");
    if (__proc_glFinish === 0) {
        __proc_glFinish = GetProcAddress(__ensureRealOpenGL32(), "glFinish");
        if (__proc_glFinish === null) {
            console.error("[opengl32_enh]   glFinish: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFinish: resolved OK");
        }
    }
    (<() => void>__proc_glFinish)();
}

let __proc_glFlush: Opaque = 0;

@dllname("glFlush")
export function glFlush(): void {
    console.log("[opengl32_enh] call glFlush");
    if (__proc_glFlush === 0) {
        __proc_glFlush = GetProcAddress(__ensureRealOpenGL32(), "glFlush");
        if (__proc_glFlush === null) {
            console.error("[opengl32_enh]   glFlush: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFlush: resolved OK");
        }
    }
    (<() => void>__proc_glFlush)();
}

let __proc_glFogf: Opaque = 0;

@dllname("glFogf")
export function glFogf(pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glFogf");
    if (__proc_glFogf === 0) {
        __proc_glFogf = GetProcAddress(__ensureRealOpenGL32(), "glFogf");
        if (__proc_glFogf === null) {
            console.error("[opengl32_enh]   glFogf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFogf: resolved OK");
        }
    }
    (<(u32, f32) => void>__proc_glFogf)(pname, param);
}

let __proc_glFogfv: Opaque = 0;

@dllname("glFogfv")
export function glFogfv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glFogfv");
    if (__proc_glFogfv === 0) {
        __proc_glFogfv = GetProcAddress(__ensureRealOpenGL32(), "glFogfv");
        if (__proc_glFogfv === null) {
            console.error("[opengl32_enh]   glFogfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFogfv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glFogfv)(pname, params);
}

let __proc_glFogi: Opaque = 0;

@dllname("glFogi")
export function glFogi(pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glFogi");
    if (__proc_glFogi === 0) {
        __proc_glFogi = GetProcAddress(__ensureRealOpenGL32(), "glFogi");
        if (__proc_glFogi === null) {
            console.error("[opengl32_enh]   glFogi: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFogi: resolved OK");
        }
    }
    (<(u32, i32) => void>__proc_glFogi)(pname, param);
}

let __proc_glFogiv: Opaque = 0;

@dllname("glFogiv")
export function glFogiv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glFogiv");
    if (__proc_glFogiv === 0) {
        __proc_glFogiv = GetProcAddress(__ensureRealOpenGL32(), "glFogiv");
        if (__proc_glFogiv === null) {
            console.error("[opengl32_enh]   glFogiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFogiv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glFogiv)(pname, params);
}

let __proc_glFrontFace: Opaque = 0;

@dllname("glFrontFace")
export function glFrontFace(mode: u32): void {
    console.log("[opengl32_enh] call glFrontFace");
    if (__proc_glFrontFace === 0) {
        __proc_glFrontFace = GetProcAddress(__ensureRealOpenGL32(), "glFrontFace");
        if (__proc_glFrontFace === null) {
            console.error("[opengl32_enh]   glFrontFace: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFrontFace: resolved OK");
        }
    }
    (<(u32) => void>__proc_glFrontFace)(mode);
}

let __proc_glFrustum: Opaque = 0;

@dllname("glFrustum")
export function glFrustum(left: f64, right: f64, bottom: f64, top: f64, zNear: f64, zFar: f64): void {
    console.log("[opengl32_enh] call glFrustum");
    if (__proc_glFrustum === 0) {
        __proc_glFrustum = GetProcAddress(__ensureRealOpenGL32(), "glFrustum");
        if (__proc_glFrustum === null) {
            console.error("[opengl32_enh]   glFrustum: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glFrustum: resolved OK");
        }
    }
    (<(f64, f64, f64, f64, f64, f64) => void>__proc_glFrustum)(left, right, bottom, top, zNear, zFar);
}

let __proc_glGenLists: Opaque = 0;

@dllname("glGenLists")
export function glGenLists(range: i32): u32 {
    console.log("[opengl32_enh] call glGenLists");
    if (__proc_glGenLists === 0) {
        __proc_glGenLists = GetProcAddress(__ensureRealOpenGL32(), "glGenLists");
        if (__proc_glGenLists === null) {
            console.error("[opengl32_enh]   glGenLists: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGenLists: resolved OK");
        }
    }
    return (<(i32) => u32>__proc_glGenLists)(range);
}

let __proc_glGenTextures: Opaque = 0;

@dllname("glGenTextures")
export function glGenTextures(n: i32, textures: Opaque): void {
    console.log("[opengl32_enh] call glGenTextures");
    if (__proc_glGenTextures === 0) {
        __proc_glGenTextures = GetProcAddress(__ensureRealOpenGL32(), "glGenTextures");
        if (__proc_glGenTextures === null) {
            console.error("[opengl32_enh]   glGenTextures: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGenTextures: resolved OK");
        }
    }
    (<(i32, Opaque) => void>__proc_glGenTextures)(n, textures);
}

let __proc_glGetBooleanv: Opaque = 0;

@dllname("glGetBooleanv")
export function glGetBooleanv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetBooleanv");
    if (__proc_glGetBooleanv === 0) {
        __proc_glGetBooleanv = GetProcAddress(__ensureRealOpenGL32(), "glGetBooleanv");
        if (__proc_glGetBooleanv === null) {
            console.error("[opengl32_enh]   glGetBooleanv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetBooleanv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetBooleanv)(pname, params);
}

let __proc_glGetClipPlane: Opaque = 0;

@dllname("glGetClipPlane")
export function glGetClipPlane(plane: u32, equation: Opaque): void {
    console.log("[opengl32_enh] call glGetClipPlane");
    if (__proc_glGetClipPlane === 0) {
        __proc_glGetClipPlane = GetProcAddress(__ensureRealOpenGL32(), "glGetClipPlane");
        if (__proc_glGetClipPlane === null) {
            console.error("[opengl32_enh]   glGetClipPlane: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetClipPlane: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetClipPlane)(plane, equation);
}

let __proc_glGetDoublev: Opaque = 0;

@dllname("glGetDoublev")
export function glGetDoublev(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetDoublev");
    if (__proc_glGetDoublev === 0) {
        __proc_glGetDoublev = GetProcAddress(__ensureRealOpenGL32(), "glGetDoublev");
        if (__proc_glGetDoublev === null) {
            console.error("[opengl32_enh]   glGetDoublev: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetDoublev: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetDoublev)(pname, params);
}

let __proc_glGetError: Opaque = 0;

@dllname("glGetError")
export function glGetError(): u32 {
    console.log("[opengl32_enh] call glGetError");
    if (__proc_glGetError === 0) {
        __proc_glGetError = GetProcAddress(__ensureRealOpenGL32(), "glGetError");
        if (__proc_glGetError === null) {
            console.error("[opengl32_enh]   glGetError: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetError: resolved OK");
        }
    }
    return (<() => u32>__proc_glGetError)();
}

let __proc_glGetFloatv: Opaque = 0;

@dllname("glGetFloatv")
export function glGetFloatv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetFloatv");
    if (__proc_glGetFloatv === 0) {
        __proc_glGetFloatv = GetProcAddress(__ensureRealOpenGL32(), "glGetFloatv");
        if (__proc_glGetFloatv === null) {
            console.error("[opengl32_enh]   glGetFloatv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetFloatv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetFloatv)(pname, params);
}

let __proc_glGetIntegerv: Opaque = 0;

@dllname("glGetIntegerv")
export function glGetIntegerv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetIntegerv");
    if (__proc_glGetIntegerv === 0) {
        __proc_glGetIntegerv = GetProcAddress(__ensureRealOpenGL32(), "glGetIntegerv");
        if (__proc_glGetIntegerv === null) {
            console.error("[opengl32_enh]   glGetIntegerv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetIntegerv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetIntegerv)(pname, params);
}

let __proc_glGetLightfv: Opaque = 0;

@dllname("glGetLightfv")
export function glGetLightfv(light: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetLightfv");
    if (__proc_glGetLightfv === 0) {
        __proc_glGetLightfv = GetProcAddress(__ensureRealOpenGL32(), "glGetLightfv");
        if (__proc_glGetLightfv === null) {
            console.error("[opengl32_enh]   glGetLightfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetLightfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetLightfv)(light, pname, params);
}

let __proc_glGetLightiv: Opaque = 0;

@dllname("glGetLightiv")
export function glGetLightiv(light: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetLightiv");
    if (__proc_glGetLightiv === 0) {
        __proc_glGetLightiv = GetProcAddress(__ensureRealOpenGL32(), "glGetLightiv");
        if (__proc_glGetLightiv === null) {
            console.error("[opengl32_enh]   glGetLightiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetLightiv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetLightiv)(light, pname, params);
}

let __proc_glGetMapdv: Opaque = 0;

@dllname("glGetMapdv")
export function glGetMapdv(target: u32, query: u32, v: Opaque): void {
    console.log("[opengl32_enh] call glGetMapdv");
    if (__proc_glGetMapdv === 0) {
        __proc_glGetMapdv = GetProcAddress(__ensureRealOpenGL32(), "glGetMapdv");
        if (__proc_glGetMapdv === null) {
            console.error("[opengl32_enh]   glGetMapdv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetMapdv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetMapdv)(target, query, v);
}

let __proc_glGetMapfv: Opaque = 0;

@dllname("glGetMapfv")
export function glGetMapfv(target: u32, query: u32, v: Opaque): void {
    console.log("[opengl32_enh] call glGetMapfv");
    if (__proc_glGetMapfv === 0) {
        __proc_glGetMapfv = GetProcAddress(__ensureRealOpenGL32(), "glGetMapfv");
        if (__proc_glGetMapfv === null) {
            console.error("[opengl32_enh]   glGetMapfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetMapfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetMapfv)(target, query, v);
}

let __proc_glGetMapiv: Opaque = 0;

@dllname("glGetMapiv")
export function glGetMapiv(target: u32, query: u32, v: Opaque): void {
    console.log("[opengl32_enh] call glGetMapiv");
    if (__proc_glGetMapiv === 0) {
        __proc_glGetMapiv = GetProcAddress(__ensureRealOpenGL32(), "glGetMapiv");
        if (__proc_glGetMapiv === null) {
            console.error("[opengl32_enh]   glGetMapiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetMapiv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetMapiv)(target, query, v);
}

let __proc_glGetMaterialfv: Opaque = 0;

@dllname("glGetMaterialfv")
export function glGetMaterialfv(face: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetMaterialfv");
    if (__proc_glGetMaterialfv === 0) {
        __proc_glGetMaterialfv = GetProcAddress(__ensureRealOpenGL32(), "glGetMaterialfv");
        if (__proc_glGetMaterialfv === null) {
            console.error("[opengl32_enh]   glGetMaterialfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetMaterialfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetMaterialfv)(face, pname, params);
}

let __proc_glGetMaterialiv: Opaque = 0;

@dllname("glGetMaterialiv")
export function glGetMaterialiv(face: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetMaterialiv");
    if (__proc_glGetMaterialiv === 0) {
        __proc_glGetMaterialiv = GetProcAddress(__ensureRealOpenGL32(), "glGetMaterialiv");
        if (__proc_glGetMaterialiv === null) {
            console.error("[opengl32_enh]   glGetMaterialiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetMaterialiv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetMaterialiv)(face, pname, params);
}

let __proc_glGetPixelMapfv: Opaque = 0;

@dllname("glGetPixelMapfv")
export function glGetPixelMapfv(map: u32, values: Opaque): void {
    console.log("[opengl32_enh] call glGetPixelMapfv");
    if (__proc_glGetPixelMapfv === 0) {
        __proc_glGetPixelMapfv = GetProcAddress(__ensureRealOpenGL32(), "glGetPixelMapfv");
        if (__proc_glGetPixelMapfv === null) {
            console.error("[opengl32_enh]   glGetPixelMapfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetPixelMapfv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetPixelMapfv)(map, values);
}

let __proc_glGetPixelMapuiv: Opaque = 0;

@dllname("glGetPixelMapuiv")
export function glGetPixelMapuiv(map: u32, values: Opaque): void {
    console.log("[opengl32_enh] call glGetPixelMapuiv");
    if (__proc_glGetPixelMapuiv === 0) {
        __proc_glGetPixelMapuiv = GetProcAddress(__ensureRealOpenGL32(), "glGetPixelMapuiv");
        if (__proc_glGetPixelMapuiv === null) {
            console.error("[opengl32_enh]   glGetPixelMapuiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetPixelMapuiv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetPixelMapuiv)(map, values);
}

let __proc_glGetPixelMapusv: Opaque = 0;

@dllname("glGetPixelMapusv")
export function glGetPixelMapusv(map: u32, values: Opaque): void {
    console.log("[opengl32_enh] call glGetPixelMapusv");
    if (__proc_glGetPixelMapusv === 0) {
        __proc_glGetPixelMapusv = GetProcAddress(__ensureRealOpenGL32(), "glGetPixelMapusv");
        if (__proc_glGetPixelMapusv === null) {
            console.error("[opengl32_enh]   glGetPixelMapusv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetPixelMapusv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetPixelMapusv)(map, values);
}

let __proc_glGetPointerv: Opaque = 0;

@dllname("glGetPointerv")
export function glGetPointerv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetPointerv");
    if (__proc_glGetPointerv === 0) {
        __proc_glGetPointerv = GetProcAddress(__ensureRealOpenGL32(), "glGetPointerv");
        if (__proc_glGetPointerv === null) {
            console.error("[opengl32_enh]   glGetPointerv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetPointerv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glGetPointerv)(pname, params);
}

let __proc_glGetPolygonStipple: Opaque = 0;

@dllname("glGetPolygonStipple")
export function glGetPolygonStipple(mask: Opaque): void {
    console.log("[opengl32_enh] call glGetPolygonStipple");
    if (__proc_glGetPolygonStipple === 0) {
        __proc_glGetPolygonStipple = GetProcAddress(__ensureRealOpenGL32(), "glGetPolygonStipple");
        if (__proc_glGetPolygonStipple === null) {
            console.error("[opengl32_enh]   glGetPolygonStipple: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetPolygonStipple: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glGetPolygonStipple)(mask);
}

let __proc_glGetString: Opaque = 0;

@dllname("glGetString")
export function glGetString(name: u32): Opaque {
    console.log("[opengl32_enh] call glGetString");
    if (__proc_glGetString === 0) {
        __proc_glGetString = GetProcAddress(__ensureRealOpenGL32(), "glGetString");
        if (__proc_glGetString === null) {
            console.error("[opengl32_enh]   glGetString: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetString: resolved OK");
        }
    }
    return (<(u32) => Opaque>__proc_glGetString)(name);
}

let __proc_glGetTexEnvfv: Opaque = 0;

@dllname("glGetTexEnvfv")
export function glGetTexEnvfv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexEnvfv");
    if (__proc_glGetTexEnvfv === 0) {
        __proc_glGetTexEnvfv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexEnvfv");
        if (__proc_glGetTexEnvfv === null) {
            console.error("[opengl32_enh]   glGetTexEnvfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexEnvfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexEnvfv)(target, pname, params);
}

let __proc_glGetTexEnviv: Opaque = 0;

@dllname("glGetTexEnviv")
export function glGetTexEnviv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexEnviv");
    if (__proc_glGetTexEnviv === 0) {
        __proc_glGetTexEnviv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexEnviv");
        if (__proc_glGetTexEnviv === null) {
            console.error("[opengl32_enh]   glGetTexEnviv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexEnviv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexEnviv)(target, pname, params);
}

let __proc_glGetTexGendv: Opaque = 0;

@dllname("glGetTexGendv")
export function glGetTexGendv(coord: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexGendv");
    if (__proc_glGetTexGendv === 0) {
        __proc_glGetTexGendv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexGendv");
        if (__proc_glGetTexGendv === null) {
            console.error("[opengl32_enh]   glGetTexGendv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexGendv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexGendv)(coord, pname, params);
}

let __proc_glGetTexGenfv: Opaque = 0;

@dllname("glGetTexGenfv")
export function glGetTexGenfv(coord: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexGenfv");
    if (__proc_glGetTexGenfv === 0) {
        __proc_glGetTexGenfv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexGenfv");
        if (__proc_glGetTexGenfv === null) {
            console.error("[opengl32_enh]   glGetTexGenfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexGenfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexGenfv)(coord, pname, params);
}

let __proc_glGetTexGeniv: Opaque = 0;

@dllname("glGetTexGeniv")
export function glGetTexGeniv(coord: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexGeniv");
    if (__proc_glGetTexGeniv === 0) {
        __proc_glGetTexGeniv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexGeniv");
        if (__proc_glGetTexGeniv === null) {
            console.error("[opengl32_enh]   glGetTexGeniv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexGeniv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexGeniv)(coord, pname, params);
}

let __proc_glGetTexImage: Opaque = 0;

@dllname("glGetTexImage")
export function glGetTexImage(target: u32, level: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glGetTexImage");
    if (__proc_glGetTexImage === 0) {
        __proc_glGetTexImage = GetProcAddress(__ensureRealOpenGL32(), "glGetTexImage");
        if (__proc_glGetTexImage === null) {
            console.error("[opengl32_enh]   glGetTexImage: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexImage: resolved OK");
        }
    }
    (<(u32, i32, u32, u32, Opaque) => void>__proc_glGetTexImage)(target, level, format, type, pixels);
}

let __proc_glGetTexLevelParameterfv: Opaque = 0;

@dllname("glGetTexLevelParameterfv")
export function glGetTexLevelParameterfv(target: u32, level: i32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexLevelParameterfv");
    if (__proc_glGetTexLevelParameterfv === 0) {
        __proc_glGetTexLevelParameterfv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexLevelParameterfv");
        if (__proc_glGetTexLevelParameterfv === null) {
            console.error("[opengl32_enh]   glGetTexLevelParameterfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexLevelParameterfv: resolved OK");
        }
    }
    (<(u32, i32, u32, Opaque) => void>__proc_glGetTexLevelParameterfv)(target, level, pname, params);
}

let __proc_glGetTexLevelParameteriv: Opaque = 0;

@dllname("glGetTexLevelParameteriv")
export function glGetTexLevelParameteriv(target: u32, level: i32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexLevelParameteriv");
    if (__proc_glGetTexLevelParameteriv === 0) {
        __proc_glGetTexLevelParameteriv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexLevelParameteriv");
        if (__proc_glGetTexLevelParameteriv === null) {
            console.error("[opengl32_enh]   glGetTexLevelParameteriv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexLevelParameteriv: resolved OK");
        }
    }
    (<(u32, i32, u32, Opaque) => void>__proc_glGetTexLevelParameteriv)(target, level, pname, params);
}

let __proc_glGetTexParameterfv: Opaque = 0;

@dllname("glGetTexParameterfv")
export function glGetTexParameterfv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexParameterfv");
    if (__proc_glGetTexParameterfv === 0) {
        __proc_glGetTexParameterfv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexParameterfv");
        if (__proc_glGetTexParameterfv === null) {
            console.error("[opengl32_enh]   glGetTexParameterfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexParameterfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexParameterfv)(target, pname, params);
}

let __proc_glGetTexParameteriv: Opaque = 0;

@dllname("glGetTexParameteriv")
export function glGetTexParameteriv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glGetTexParameteriv");
    if (__proc_glGetTexParameteriv === 0) {
        __proc_glGetTexParameteriv = GetProcAddress(__ensureRealOpenGL32(), "glGetTexParameteriv");
        if (__proc_glGetTexParameteriv === null) {
            console.error("[opengl32_enh]   glGetTexParameteriv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glGetTexParameteriv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glGetTexParameteriv)(target, pname, params);
}

let __proc_glHint: Opaque = 0;

@dllname("glHint")
export function glHint(target: u32, mode: u32): void {
    console.log("[opengl32_enh] call glHint");
    if (__proc_glHint === 0) {
        __proc_glHint = GetProcAddress(__ensureRealOpenGL32(), "glHint");
        if (__proc_glHint === null) {
            console.error("[opengl32_enh]   glHint: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glHint: resolved OK");
        }
    }
    (<(u32, u32) => void>__proc_glHint)(target, mode);
}

let __proc_glIndexMask: Opaque = 0;

@dllname("glIndexMask")
export function glIndexMask(mask: u32): void {
    console.log("[opengl32_enh] call glIndexMask");
    if (__proc_glIndexMask === 0) {
        __proc_glIndexMask = GetProcAddress(__ensureRealOpenGL32(), "glIndexMask");
        if (__proc_glIndexMask === null) {
            console.error("[opengl32_enh]   glIndexMask: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexMask: resolved OK");
        }
    }
    (<(u32) => void>__proc_glIndexMask)(mask);
}

let __proc_glIndexPointer: Opaque = 0;

@dllname("glIndexPointer")
export function glIndexPointer(type: u32, stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glIndexPointer");
    if (__proc_glIndexPointer === 0) {
        __proc_glIndexPointer = GetProcAddress(__ensureRealOpenGL32(), "glIndexPointer");
        if (__proc_glIndexPointer === null) {
            console.error("[opengl32_enh]   glIndexPointer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexPointer: resolved OK");
        }
    }
    (<(u32, i32, Opaque) => void>__proc_glIndexPointer)(type, stride, pointer);
}

let __proc_glIndexd: Opaque = 0;

@dllname("glIndexd")
export function glIndexd(c: f64): void {
    console.log("[opengl32_enh] call glIndexd");
    if (__proc_glIndexd === 0) {
        __proc_glIndexd = GetProcAddress(__ensureRealOpenGL32(), "glIndexd");
        if (__proc_glIndexd === null) {
            console.error("[opengl32_enh]   glIndexd: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexd: resolved OK");
        }
    }
    (<(f64) => void>__proc_glIndexd)(c);
}

let __proc_glIndexdv: Opaque = 0;

@dllname("glIndexdv")
export function glIndexdv(c: Opaque): void {
    console.log("[opengl32_enh] call glIndexdv");
    if (__proc_glIndexdv === 0) {
        __proc_glIndexdv = GetProcAddress(__ensureRealOpenGL32(), "glIndexdv");
        if (__proc_glIndexdv === null) {
            console.error("[opengl32_enh]   glIndexdv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexdv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glIndexdv)(c);
}

let __proc_glIndexf: Opaque = 0;

@dllname("glIndexf")
export function glIndexf(c: f32): void {
    console.log("[opengl32_enh] call glIndexf");
    if (__proc_glIndexf === 0) {
        __proc_glIndexf = GetProcAddress(__ensureRealOpenGL32(), "glIndexf");
        if (__proc_glIndexf === null) {
            console.error("[opengl32_enh]   glIndexf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexf: resolved OK");
        }
    }
    (<(f32) => void>__proc_glIndexf)(c);
}

let __proc_glIndexfv: Opaque = 0;

@dllname("glIndexfv")
export function glIndexfv(c: Opaque): void {
    console.log("[opengl32_enh] call glIndexfv");
    if (__proc_glIndexfv === 0) {
        __proc_glIndexfv = GetProcAddress(__ensureRealOpenGL32(), "glIndexfv");
        if (__proc_glIndexfv === null) {
            console.error("[opengl32_enh]   glIndexfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexfv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glIndexfv)(c);
}

let __proc_glIndexi: Opaque = 0;

@dllname("glIndexi")
export function glIndexi(c: i32): void {
    console.log("[opengl32_enh] call glIndexi");
    if (__proc_glIndexi === 0) {
        __proc_glIndexi = GetProcAddress(__ensureRealOpenGL32(), "glIndexi");
        if (__proc_glIndexi === null) {
            console.error("[opengl32_enh]   glIndexi: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexi: resolved OK");
        }
    }
    (<(i32) => void>__proc_glIndexi)(c);
}

let __proc_glIndexiv: Opaque = 0;

@dllname("glIndexiv")
export function glIndexiv(c: Opaque): void {
    console.log("[opengl32_enh] call glIndexiv");
    if (__proc_glIndexiv === 0) {
        __proc_glIndexiv = GetProcAddress(__ensureRealOpenGL32(), "glIndexiv");
        if (__proc_glIndexiv === null) {
            console.error("[opengl32_enh]   glIndexiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexiv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glIndexiv)(c);
}

let __proc_glIndexs: Opaque = 0;

@dllname("glIndexs")
export function glIndexs(c: i16): void {
    console.log("[opengl32_enh] call glIndexs");
    if (__proc_glIndexs === 0) {
        __proc_glIndexs = GetProcAddress(__ensureRealOpenGL32(), "glIndexs");
        if (__proc_glIndexs === null) {
            console.error("[opengl32_enh]   glIndexs: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexs: resolved OK");
        }
    }
    (<(i16) => void>__proc_glIndexs)(c);
}

let __proc_glIndexsv: Opaque = 0;

@dllname("glIndexsv")
export function glIndexsv(c: Opaque): void {
    console.log("[opengl32_enh] call glIndexsv");
    if (__proc_glIndexsv === 0) {
        __proc_glIndexsv = GetProcAddress(__ensureRealOpenGL32(), "glIndexsv");
        if (__proc_glIndexsv === null) {
            console.error("[opengl32_enh]   glIndexsv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexsv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glIndexsv)(c);
}

let __proc_glIndexub: Opaque = 0;

@dllname("glIndexub")
export function glIndexub(c: u8): void {
    console.log("[opengl32_enh] call glIndexub");
    if (__proc_glIndexub === 0) {
        __proc_glIndexub = GetProcAddress(__ensureRealOpenGL32(), "glIndexub");
        if (__proc_glIndexub === null) {
            console.error("[opengl32_enh]   glIndexub: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexub: resolved OK");
        }
    }
    (<(u8) => void>__proc_glIndexub)(c);
}

let __proc_glIndexubv: Opaque = 0;

@dllname("glIndexubv")
export function glIndexubv(c: Opaque): void {
    console.log("[opengl32_enh] call glIndexubv");
    if (__proc_glIndexubv === 0) {
        __proc_glIndexubv = GetProcAddress(__ensureRealOpenGL32(), "glIndexubv");
        if (__proc_glIndexubv === null) {
            console.error("[opengl32_enh]   glIndexubv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIndexubv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glIndexubv)(c);
}

let __proc_glInitNames: Opaque = 0;

@dllname("glInitNames")
export function glInitNames(): void {
    console.log("[opengl32_enh] call glInitNames");
    if (__proc_glInitNames === 0) {
        __proc_glInitNames = GetProcAddress(__ensureRealOpenGL32(), "glInitNames");
        if (__proc_glInitNames === null) {
            console.error("[opengl32_enh]   glInitNames: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glInitNames: resolved OK");
        }
    }
    (<() => void>__proc_glInitNames)();
}

let __proc_glInterleavedArrays: Opaque = 0;

@dllname("glInterleavedArrays")
export function glInterleavedArrays(format: u32, stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glInterleavedArrays");
    if (__proc_glInterleavedArrays === 0) {
        __proc_glInterleavedArrays = GetProcAddress(__ensureRealOpenGL32(), "glInterleavedArrays");
        if (__proc_glInterleavedArrays === null) {
            console.error("[opengl32_enh]   glInterleavedArrays: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glInterleavedArrays: resolved OK");
        }
    }
    (<(u32, i32, Opaque) => void>__proc_glInterleavedArrays)(format, stride, pointer);
}

let __proc_glIsEnabled: Opaque = 0;

@dllname("glIsEnabled")
export function glIsEnabled(cap: u32): u8 {
    console.log("[opengl32_enh] call glIsEnabled");
    if (__proc_glIsEnabled === 0) {
        __proc_glIsEnabled = GetProcAddress(__ensureRealOpenGL32(), "glIsEnabled");
        if (__proc_glIsEnabled === null) {
            console.error("[opengl32_enh]   glIsEnabled: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIsEnabled: resolved OK");
        }
    }
    return (<(u32) => u8>__proc_glIsEnabled)(cap);
}

let __proc_glIsList: Opaque = 0;

@dllname("glIsList")
export function glIsList(list: u32): u8 {
    console.log("[opengl32_enh] call glIsList");
    if (__proc_glIsList === 0) {
        __proc_glIsList = GetProcAddress(__ensureRealOpenGL32(), "glIsList");
        if (__proc_glIsList === null) {
            console.error("[opengl32_enh]   glIsList: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIsList: resolved OK");
        }
    }
    return (<(u32) => u8>__proc_glIsList)(list);
}

let __proc_glIsTexture: Opaque = 0;

@dllname("glIsTexture")
export function glIsTexture(texture: u32): u8 {
    console.log("[opengl32_enh] call glIsTexture");
    if (__proc_glIsTexture === 0) {
        __proc_glIsTexture = GetProcAddress(__ensureRealOpenGL32(), "glIsTexture");
        if (__proc_glIsTexture === null) {
            console.error("[opengl32_enh]   glIsTexture: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glIsTexture: resolved OK");
        }
    }
    return (<(u32) => u8>__proc_glIsTexture)(texture);
}

let __proc_glLightModelf: Opaque = 0;

@dllname("glLightModelf")
export function glLightModelf(pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glLightModelf");
    if (__proc_glLightModelf === 0) {
        __proc_glLightModelf = GetProcAddress(__ensureRealOpenGL32(), "glLightModelf");
        if (__proc_glLightModelf === null) {
            console.error("[opengl32_enh]   glLightModelf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightModelf: resolved OK");
        }
    }
    (<(u32, f32) => void>__proc_glLightModelf)(pname, param);
}

let __proc_glLightModelfv: Opaque = 0;

@dllname("glLightModelfv")
export function glLightModelfv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glLightModelfv");
    if (__proc_glLightModelfv === 0) {
        __proc_glLightModelfv = GetProcAddress(__ensureRealOpenGL32(), "glLightModelfv");
        if (__proc_glLightModelfv === null) {
            console.error("[opengl32_enh]   glLightModelfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightModelfv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glLightModelfv)(pname, params);
}

let __proc_glLightModeli: Opaque = 0;

@dllname("glLightModeli")
export function glLightModeli(pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glLightModeli");
    if (__proc_glLightModeli === 0) {
        __proc_glLightModeli = GetProcAddress(__ensureRealOpenGL32(), "glLightModeli");
        if (__proc_glLightModeli === null) {
            console.error("[opengl32_enh]   glLightModeli: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightModeli: resolved OK");
        }
    }
    (<(u32, i32) => void>__proc_glLightModeli)(pname, param);
}

let __proc_glLightModeliv: Opaque = 0;

@dllname("glLightModeliv")
export function glLightModeliv(pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glLightModeliv");
    if (__proc_glLightModeliv === 0) {
        __proc_glLightModeliv = GetProcAddress(__ensureRealOpenGL32(), "glLightModeliv");
        if (__proc_glLightModeliv === null) {
            console.error("[opengl32_enh]   glLightModeliv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightModeliv: resolved OK");
        }
    }
    (<(u32, Opaque) => void>__proc_glLightModeliv)(pname, params);
}

let __proc_glLightf: Opaque = 0;

@dllname("glLightf")
export function glLightf(light: u32, pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glLightf");
    if (__proc_glLightf === 0) {
        __proc_glLightf = GetProcAddress(__ensureRealOpenGL32(), "glLightf");
        if (__proc_glLightf === null) {
            console.error("[opengl32_enh]   glLightf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightf: resolved OK");
        }
    }
    (<(u32, u32, f32) => void>__proc_glLightf)(light, pname, param);
}

let __proc_glLightfv: Opaque = 0;

@dllname("glLightfv")
export function glLightfv(light: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glLightfv");
    if (__proc_glLightfv === 0) {
        __proc_glLightfv = GetProcAddress(__ensureRealOpenGL32(), "glLightfv");
        if (__proc_glLightfv === null) {
            console.error("[opengl32_enh]   glLightfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glLightfv)(light, pname, params);
}

let __proc_glLighti: Opaque = 0;

@dllname("glLighti")
export function glLighti(light: u32, pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glLighti");
    if (__proc_glLighti === 0) {
        __proc_glLighti = GetProcAddress(__ensureRealOpenGL32(), "glLighti");
        if (__proc_glLighti === null) {
            console.error("[opengl32_enh]   glLighti: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLighti: resolved OK");
        }
    }
    (<(u32, u32, i32) => void>__proc_glLighti)(light, pname, param);
}

let __proc_glLightiv: Opaque = 0;

@dllname("glLightiv")
export function glLightiv(light: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glLightiv");
    if (__proc_glLightiv === 0) {
        __proc_glLightiv = GetProcAddress(__ensureRealOpenGL32(), "glLightiv");
        if (__proc_glLightiv === null) {
            console.error("[opengl32_enh]   glLightiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLightiv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glLightiv)(light, pname, params);
}

let __proc_glLineStipple: Opaque = 0;

@dllname("glLineStipple")
export function glLineStipple(factor: i32, pattern: u16): void {
    console.log("[opengl32_enh] call glLineStipple");
    if (__proc_glLineStipple === 0) {
        __proc_glLineStipple = GetProcAddress(__ensureRealOpenGL32(), "glLineStipple");
        if (__proc_glLineStipple === null) {
            console.error("[opengl32_enh]   glLineStipple: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLineStipple: resolved OK");
        }
    }
    (<(i32, u16) => void>__proc_glLineStipple)(factor, pattern);
}

let __proc_glLineWidth: Opaque = 0;

@dllname("glLineWidth")
export function glLineWidth(width: f32): void {
    console.log("[opengl32_enh] call glLineWidth");
    if (__proc_glLineWidth === 0) {
        __proc_glLineWidth = GetProcAddress(__ensureRealOpenGL32(), "glLineWidth");
        if (__proc_glLineWidth === null) {
            console.error("[opengl32_enh]   glLineWidth: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLineWidth: resolved OK");
        }
    }
    (<(f32) => void>__proc_glLineWidth)(width);
}

let __proc_glListBase: Opaque = 0;

@dllname("glListBase")
export function glListBase(base: u32): void {
    console.log("[opengl32_enh] call glListBase");
    if (__proc_glListBase === 0) {
        __proc_glListBase = GetProcAddress(__ensureRealOpenGL32(), "glListBase");
        if (__proc_glListBase === null) {
            console.error("[opengl32_enh]   glListBase: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glListBase: resolved OK");
        }
    }
    (<(u32) => void>__proc_glListBase)(base);
}

let __proc_glLoadIdentity: Opaque = 0;

@dllname("glLoadIdentity")
export function glLoadIdentity(): void {
    console.log("[opengl32_enh] call glLoadIdentity");
    if (__proc_glLoadIdentity === 0) {
        __proc_glLoadIdentity = GetProcAddress(__ensureRealOpenGL32(), "glLoadIdentity");
        if (__proc_glLoadIdentity === null) {
            console.error("[opengl32_enh]   glLoadIdentity: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLoadIdentity: resolved OK");
        }
    }
    (<() => void>__proc_glLoadIdentity)();
}

let __proc_glLoadMatrixd: Opaque = 0;

@dllname("glLoadMatrixd")
export function glLoadMatrixd(m: Opaque): void {
    console.log("[opengl32_enh] call glLoadMatrixd");
    if (__proc_glLoadMatrixd === 0) {
        __proc_glLoadMatrixd = GetProcAddress(__ensureRealOpenGL32(), "glLoadMatrixd");
        if (__proc_glLoadMatrixd === null) {
            console.error("[opengl32_enh]   glLoadMatrixd: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLoadMatrixd: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glLoadMatrixd)(m);
}

let __proc_glLoadMatrixf: Opaque = 0;

@dllname("glLoadMatrixf")
export function glLoadMatrixf(m: Opaque): void {
    console.log("[opengl32_enh] call glLoadMatrixf");
    if (__proc_glLoadMatrixf === 0) {
        __proc_glLoadMatrixf = GetProcAddress(__ensureRealOpenGL32(), "glLoadMatrixf");
        if (__proc_glLoadMatrixf === null) {
            console.error("[opengl32_enh]   glLoadMatrixf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLoadMatrixf: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glLoadMatrixf)(m);
}

let __proc_glLoadName: Opaque = 0;

@dllname("glLoadName")
export function glLoadName(name: u32): void {
    console.log("[opengl32_enh] call glLoadName");
    if (__proc_glLoadName === 0) {
        __proc_glLoadName = GetProcAddress(__ensureRealOpenGL32(), "glLoadName");
        if (__proc_glLoadName === null) {
            console.error("[opengl32_enh]   glLoadName: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLoadName: resolved OK");
        }
    }
    (<(u32) => void>__proc_glLoadName)(name);
}

let __proc_glLogicOp: Opaque = 0;

@dllname("glLogicOp")
export function glLogicOp(opcode: u32): void {
    console.log("[opengl32_enh] call glLogicOp");
    if (__proc_glLogicOp === 0) {
        __proc_glLogicOp = GetProcAddress(__ensureRealOpenGL32(), "glLogicOp");
        if (__proc_glLogicOp === null) {
            console.error("[opengl32_enh]   glLogicOp: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glLogicOp: resolved OK");
        }
    }
    (<(u32) => void>__proc_glLogicOp)(opcode);
}

let __proc_glMap1d: Opaque = 0;

@dllname("glMap1d")
export function glMap1d(target: u32, u1: f64, u2: f64, stride: i32, order: i32, points: Opaque): void {
    console.log("[opengl32_enh] call glMap1d");
    if (__proc_glMap1d === 0) {
        __proc_glMap1d = GetProcAddress(__ensureRealOpenGL32(), "glMap1d");
        if (__proc_glMap1d === null) {
            console.error("[opengl32_enh]   glMap1d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMap1d: resolved OK");
        }
    }
    (<(u32, f64, f64, i32, i32, Opaque) => void>__proc_glMap1d)(target, u1, u2, stride, order, points);
}

let __proc_glMap1f: Opaque = 0;

@dllname("glMap1f")
export function glMap1f(target: u32, u1: f32, u2: f32, stride: i32, order: i32, points: Opaque): void {
    console.log("[opengl32_enh] call glMap1f");
    if (__proc_glMap1f === 0) {
        __proc_glMap1f = GetProcAddress(__ensureRealOpenGL32(), "glMap1f");
        if (__proc_glMap1f === null) {
            console.error("[opengl32_enh]   glMap1f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMap1f: resolved OK");
        }
    }
    (<(u32, f32, f32, i32, i32, Opaque) => void>__proc_glMap1f)(target, u1, u2, stride, order, points);
}

let __proc_glMap2d: Opaque = 0;

@dllname("glMap2d")
export function glMap2d(target: u32, u1: f64, u2: f64, ustride: i32, uorder: i32, v1: f64, v2: f64, vstride: i32, vorder: i32, points: Opaque): void {
    console.log("[opengl32_enh] call glMap2d");
    if (__proc_glMap2d === 0) {
        __proc_glMap2d = GetProcAddress(__ensureRealOpenGL32(), "glMap2d");
        if (__proc_glMap2d === null) {
            console.error("[opengl32_enh]   glMap2d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMap2d: resolved OK");
        }
    }
    (<(u32, f64, f64, i32, i32, f64, f64, i32, i32, Opaque) => void>__proc_glMap2d)(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

let __proc_glMap2f: Opaque = 0;

@dllname("glMap2f")
export function glMap2f(target: u32, u1: f32, u2: f32, ustride: i32, uorder: i32, v1: f32, v2: f32, vstride: i32, vorder: i32, points: Opaque): void {
    console.log("[opengl32_enh] call glMap2f");
    if (__proc_glMap2f === 0) {
        __proc_glMap2f = GetProcAddress(__ensureRealOpenGL32(), "glMap2f");
        if (__proc_glMap2f === null) {
            console.error("[opengl32_enh]   glMap2f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMap2f: resolved OK");
        }
    }
    (<(u32, f32, f32, i32, i32, f32, f32, i32, i32, Opaque) => void>__proc_glMap2f)(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

let __proc_glMapGrid1d: Opaque = 0;

@dllname("glMapGrid1d")
export function glMapGrid1d(un: i32, u1: f64, u2: f64): void {
    console.log("[opengl32_enh] call glMapGrid1d");
    if (__proc_glMapGrid1d === 0) {
        __proc_glMapGrid1d = GetProcAddress(__ensureRealOpenGL32(), "glMapGrid1d");
        if (__proc_glMapGrid1d === null) {
            console.error("[opengl32_enh]   glMapGrid1d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMapGrid1d: resolved OK");
        }
    }
    (<(i32, f64, f64) => void>__proc_glMapGrid1d)(un, u1, u2);
}

let __proc_glMapGrid1f: Opaque = 0;

@dllname("glMapGrid1f")
export function glMapGrid1f(un: i32, u1: f32, u2: f32): void {
    console.log("[opengl32_enh] call glMapGrid1f");
    if (__proc_glMapGrid1f === 0) {
        __proc_glMapGrid1f = GetProcAddress(__ensureRealOpenGL32(), "glMapGrid1f");
        if (__proc_glMapGrid1f === null) {
            console.error("[opengl32_enh]   glMapGrid1f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMapGrid1f: resolved OK");
        }
    }
    (<(i32, f32, f32) => void>__proc_glMapGrid1f)(un, u1, u2);
}

let __proc_glMapGrid2d: Opaque = 0;

@dllname("glMapGrid2d")
export function glMapGrid2d(un: i32, u1: f64, u2: f64, vn: i32, v1: f64, v2: f64): void {
    console.log("[opengl32_enh] call glMapGrid2d");
    if (__proc_glMapGrid2d === 0) {
        __proc_glMapGrid2d = GetProcAddress(__ensureRealOpenGL32(), "glMapGrid2d");
        if (__proc_glMapGrid2d === null) {
            console.error("[opengl32_enh]   glMapGrid2d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMapGrid2d: resolved OK");
        }
    }
    (<(i32, f64, f64, i32, f64, f64) => void>__proc_glMapGrid2d)(un, u1, u2, vn, v1, v2);
}

let __proc_glMapGrid2f: Opaque = 0;

@dllname("glMapGrid2f")
export function glMapGrid2f(un: i32, u1: f32, u2: f32, vn: i32, v1: f32, v2: f32): void {
    console.log("[opengl32_enh] call glMapGrid2f");
    if (__proc_glMapGrid2f === 0) {
        __proc_glMapGrid2f = GetProcAddress(__ensureRealOpenGL32(), "glMapGrid2f");
        if (__proc_glMapGrid2f === null) {
            console.error("[opengl32_enh]   glMapGrid2f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMapGrid2f: resolved OK");
        }
    }
    (<(i32, f32, f32, i32, f32, f32) => void>__proc_glMapGrid2f)(un, u1, u2, vn, v1, v2);
}

let __proc_glMaterialf: Opaque = 0;

@dllname("glMaterialf")
export function glMaterialf(face: u32, pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glMaterialf");
    if (__proc_glMaterialf === 0) {
        __proc_glMaterialf = GetProcAddress(__ensureRealOpenGL32(), "glMaterialf");
        if (__proc_glMaterialf === null) {
            console.error("[opengl32_enh]   glMaterialf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMaterialf: resolved OK");
        }
    }
    (<(u32, u32, f32) => void>__proc_glMaterialf)(face, pname, param);
}

let __proc_glMaterialfv: Opaque = 0;

@dllname("glMaterialfv")
export function glMaterialfv(face: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glMaterialfv");
    if (__proc_glMaterialfv === 0) {
        __proc_glMaterialfv = GetProcAddress(__ensureRealOpenGL32(), "glMaterialfv");
        if (__proc_glMaterialfv === null) {
            console.error("[opengl32_enh]   glMaterialfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMaterialfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glMaterialfv)(face, pname, params);
}

let __proc_glMateriali: Opaque = 0;

@dllname("glMateriali")
export function glMateriali(face: u32, pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glMateriali");
    if (__proc_glMateriali === 0) {
        __proc_glMateriali = GetProcAddress(__ensureRealOpenGL32(), "glMateriali");
        if (__proc_glMateriali === null) {
            console.error("[opengl32_enh]   glMateriali: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMateriali: resolved OK");
        }
    }
    (<(u32, u32, i32) => void>__proc_glMateriali)(face, pname, param);
}

let __proc_glMaterialiv: Opaque = 0;

@dllname("glMaterialiv")
export function glMaterialiv(face: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glMaterialiv");
    if (__proc_glMaterialiv === 0) {
        __proc_glMaterialiv = GetProcAddress(__ensureRealOpenGL32(), "glMaterialiv");
        if (__proc_glMaterialiv === null) {
            console.error("[opengl32_enh]   glMaterialiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMaterialiv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glMaterialiv)(face, pname, params);
}

let __proc_glMatrixMode: Opaque = 0;

@dllname("glMatrixMode")
export function glMatrixMode(mode: u32): void {
    console.log("[opengl32_enh] call glMatrixMode");
    if (__proc_glMatrixMode === 0) {
        __proc_glMatrixMode = GetProcAddress(__ensureRealOpenGL32(), "glMatrixMode");
        if (__proc_glMatrixMode === null) {
            console.error("[opengl32_enh]   glMatrixMode: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMatrixMode: resolved OK");
        }
    }
    (<(u32) => void>__proc_glMatrixMode)(mode);
}

let __proc_glMultMatrixd: Opaque = 0;

@dllname("glMultMatrixd")
export function glMultMatrixd(m: Opaque): void {
    console.log("[opengl32_enh] call glMultMatrixd");
    if (__proc_glMultMatrixd === 0) {
        __proc_glMultMatrixd = GetProcAddress(__ensureRealOpenGL32(), "glMultMatrixd");
        if (__proc_glMultMatrixd === null) {
            console.error("[opengl32_enh]   glMultMatrixd: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMultMatrixd: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glMultMatrixd)(m);
}

let __proc_glMultMatrixf: Opaque = 0;

@dllname("glMultMatrixf")
export function glMultMatrixf(m: Opaque): void {
    console.log("[opengl32_enh] call glMultMatrixf");
    if (__proc_glMultMatrixf === 0) {
        __proc_glMultMatrixf = GetProcAddress(__ensureRealOpenGL32(), "glMultMatrixf");
        if (__proc_glMultMatrixf === null) {
            console.error("[opengl32_enh]   glMultMatrixf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glMultMatrixf: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glMultMatrixf)(m);
}

let __proc_glNewList: Opaque = 0;

@dllname("glNewList")
export function glNewList(list: u32, mode: u32): void {
    console.log("[opengl32_enh] call glNewList");
    if (__proc_glNewList === 0) {
        __proc_glNewList = GetProcAddress(__ensureRealOpenGL32(), "glNewList");
        if (__proc_glNewList === null) {
            console.error("[opengl32_enh]   glNewList: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNewList: resolved OK");
        }
    }
    (<(u32, u32) => void>__proc_glNewList)(list, mode);
}

let __proc_glNormal3b: Opaque = 0;

@dllname("glNormal3b")
export function glNormal3b(nx: i8, ny: i8, nz: i8): void {
    console.log("[opengl32_enh] call glNormal3b");
    if (__proc_glNormal3b === 0) {
        __proc_glNormal3b = GetProcAddress(__ensureRealOpenGL32(), "glNormal3b");
        if (__proc_glNormal3b === null) {
            console.error("[opengl32_enh]   glNormal3b: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3b: resolved OK");
        }
    }
    (<(i8, i8, i8) => void>__proc_glNormal3b)(nx, ny, nz);
}

let __proc_glNormal3bv: Opaque = 0;

@dllname("glNormal3bv")
export function glNormal3bv(v: Opaque): void {
    console.log("[opengl32_enh] call glNormal3bv");
    if (__proc_glNormal3bv === 0) {
        __proc_glNormal3bv = GetProcAddress(__ensureRealOpenGL32(), "glNormal3bv");
        if (__proc_glNormal3bv === null) {
            console.error("[opengl32_enh]   glNormal3bv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3bv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glNormal3bv)(v);
}

let __proc_glNormal3d: Opaque = 0;

@dllname("glNormal3d")
export function glNormal3d(nx: f64, ny: f64, nz: f64): void {
    console.log("[opengl32_enh] call glNormal3d");
    if (__proc_glNormal3d === 0) {
        __proc_glNormal3d = GetProcAddress(__ensureRealOpenGL32(), "glNormal3d");
        if (__proc_glNormal3d === null) {
            console.error("[opengl32_enh]   glNormal3d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3d: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glNormal3d)(nx, ny, nz);
}

let __proc_glNormal3dv: Opaque = 0;

@dllname("glNormal3dv")
export function glNormal3dv(v: Opaque): void {
    console.log("[opengl32_enh] call glNormal3dv");
    if (__proc_glNormal3dv === 0) {
        __proc_glNormal3dv = GetProcAddress(__ensureRealOpenGL32(), "glNormal3dv");
        if (__proc_glNormal3dv === null) {
            console.error("[opengl32_enh]   glNormal3dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glNormal3dv)(v);
}

let __proc_glNormal3f: Opaque = 0;

@dllname("glNormal3f")
export function glNormal3f(nx: f32, ny: f32, nz: f32): void {
    console.log("[opengl32_enh] call glNormal3f");
    if (__proc_glNormal3f === 0) {
        __proc_glNormal3f = GetProcAddress(__ensureRealOpenGL32(), "glNormal3f");
        if (__proc_glNormal3f === null) {
            console.error("[opengl32_enh]   glNormal3f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3f: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glNormal3f)(nx, ny, nz);
}

let __proc_glNormal3fv: Opaque = 0;

@dllname("glNormal3fv")
export function glNormal3fv(v: Opaque): void {
    console.log("[opengl32_enh] call glNormal3fv");
    if (__proc_glNormal3fv === 0) {
        __proc_glNormal3fv = GetProcAddress(__ensureRealOpenGL32(), "glNormal3fv");
        if (__proc_glNormal3fv === null) {
            console.error("[opengl32_enh]   glNormal3fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glNormal3fv)(v);
}

let __proc_glNormal3i: Opaque = 0;

@dllname("glNormal3i")
export function glNormal3i(nx: i32, ny: i32, nz: i32): void {
    console.log("[opengl32_enh] call glNormal3i");
    if (__proc_glNormal3i === 0) {
        __proc_glNormal3i = GetProcAddress(__ensureRealOpenGL32(), "glNormal3i");
        if (__proc_glNormal3i === null) {
            console.error("[opengl32_enh]   glNormal3i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3i: resolved OK");
        }
    }
    (<(i32, i32, i32) => void>__proc_glNormal3i)(nx, ny, nz);
}

let __proc_glNormal3iv: Opaque = 0;

@dllname("glNormal3iv")
export function glNormal3iv(v: Opaque): void {
    console.log("[opengl32_enh] call glNormal3iv");
    if (__proc_glNormal3iv === 0) {
        __proc_glNormal3iv = GetProcAddress(__ensureRealOpenGL32(), "glNormal3iv");
        if (__proc_glNormal3iv === null) {
            console.error("[opengl32_enh]   glNormal3iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glNormal3iv)(v);
}

let __proc_glNormal3s: Opaque = 0;

@dllname("glNormal3s")
export function glNormal3s(nx: i16, ny: i16, nz: i16): void {
    console.log("[opengl32_enh] call glNormal3s");
    if (__proc_glNormal3s === 0) {
        __proc_glNormal3s = GetProcAddress(__ensureRealOpenGL32(), "glNormal3s");
        if (__proc_glNormal3s === null) {
            console.error("[opengl32_enh]   glNormal3s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3s: resolved OK");
        }
    }
    (<(i16, i16, i16) => void>__proc_glNormal3s)(nx, ny, nz);
}

let __proc_glNormal3sv: Opaque = 0;

@dllname("glNormal3sv")
export function glNormal3sv(v: Opaque): void {
    console.log("[opengl32_enh] call glNormal3sv");
    if (__proc_glNormal3sv === 0) {
        __proc_glNormal3sv = GetProcAddress(__ensureRealOpenGL32(), "glNormal3sv");
        if (__proc_glNormal3sv === null) {
            console.error("[opengl32_enh]   glNormal3sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormal3sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glNormal3sv)(v);
}

let __proc_glNormalPointer: Opaque = 0;

@dllname("glNormalPointer")
export function glNormalPointer(type: u32, stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glNormalPointer");
    if (__proc_glNormalPointer === 0) {
        __proc_glNormalPointer = GetProcAddress(__ensureRealOpenGL32(), "glNormalPointer");
        if (__proc_glNormalPointer === null) {
            console.error("[opengl32_enh]   glNormalPointer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glNormalPointer: resolved OK");
        }
    }
    (<(u32, i32, Opaque) => void>__proc_glNormalPointer)(type, stride, pointer);
}

let __proc_glOrtho: Opaque = 0;

@dllname("glOrtho")
export function glOrtho(left: f64, right: f64, bottom: f64, top: f64, zNear: f64, zFar: f64): void {
    console.log("[opengl32_enh] call glOrtho");
    if (__proc_glOrtho === 0) {
        __proc_glOrtho = GetProcAddress(__ensureRealOpenGL32(), "glOrtho");
        if (__proc_glOrtho === null) {
            console.error("[opengl32_enh]   glOrtho: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glOrtho: resolved OK");
        }
    }
    (<(f64, f64, f64, f64, f64, f64) => void>__proc_glOrtho)(left, right, bottom, top, zNear, zFar);
}

let __proc_glPassThrough: Opaque = 0;

@dllname("glPassThrough")
export function glPassThrough(token: f32): void {
    console.log("[opengl32_enh] call glPassThrough");
    if (__proc_glPassThrough === 0) {
        __proc_glPassThrough = GetProcAddress(__ensureRealOpenGL32(), "glPassThrough");
        if (__proc_glPassThrough === null) {
            console.error("[opengl32_enh]   glPassThrough: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPassThrough: resolved OK");
        }
    }
    (<(f32) => void>__proc_glPassThrough)(token);
}

let __proc_glPixelMapfv: Opaque = 0;

@dllname("glPixelMapfv")
export function glPixelMapfv(map: u32, mapsize: i32, values: Opaque): void {
    console.log("[opengl32_enh] call glPixelMapfv");
    if (__proc_glPixelMapfv === 0) {
        __proc_glPixelMapfv = GetProcAddress(__ensureRealOpenGL32(), "glPixelMapfv");
        if (__proc_glPixelMapfv === null) {
            console.error("[opengl32_enh]   glPixelMapfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelMapfv: resolved OK");
        }
    }
    (<(u32, i32, Opaque) => void>__proc_glPixelMapfv)(map, mapsize, values);
}

let __proc_glPixelMapuiv: Opaque = 0;

@dllname("glPixelMapuiv")
export function glPixelMapuiv(map: u32, mapsize: i32, values: Opaque): void {
    console.log("[opengl32_enh] call glPixelMapuiv");
    if (__proc_glPixelMapuiv === 0) {
        __proc_glPixelMapuiv = GetProcAddress(__ensureRealOpenGL32(), "glPixelMapuiv");
        if (__proc_glPixelMapuiv === null) {
            console.error("[opengl32_enh]   glPixelMapuiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelMapuiv: resolved OK");
        }
    }
    (<(u32, i32, Opaque) => void>__proc_glPixelMapuiv)(map, mapsize, values);
}

let __proc_glPixelMapusv: Opaque = 0;

@dllname("glPixelMapusv")
export function glPixelMapusv(map: u32, mapsize: i32, values: Opaque): void {
    console.log("[opengl32_enh] call glPixelMapusv");
    if (__proc_glPixelMapusv === 0) {
        __proc_glPixelMapusv = GetProcAddress(__ensureRealOpenGL32(), "glPixelMapusv");
        if (__proc_glPixelMapusv === null) {
            console.error("[opengl32_enh]   glPixelMapusv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelMapusv: resolved OK");
        }
    }
    (<(u32, i32, Opaque) => void>__proc_glPixelMapusv)(map, mapsize, values);
}

let __proc_glPixelStoref: Opaque = 0;

@dllname("glPixelStoref")
export function glPixelStoref(pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glPixelStoref");
    if (__proc_glPixelStoref === 0) {
        __proc_glPixelStoref = GetProcAddress(__ensureRealOpenGL32(), "glPixelStoref");
        if (__proc_glPixelStoref === null) {
            console.error("[opengl32_enh]   glPixelStoref: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelStoref: resolved OK");
        }
    }
    (<(u32, f32) => void>__proc_glPixelStoref)(pname, param);
}

let __proc_glPixelStorei: Opaque = 0;

@dllname("glPixelStorei")
export function glPixelStorei(pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glPixelStorei");
    if (__proc_glPixelStorei === 0) {
        __proc_glPixelStorei = GetProcAddress(__ensureRealOpenGL32(), "glPixelStorei");
        if (__proc_glPixelStorei === null) {
            console.error("[opengl32_enh]   glPixelStorei: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelStorei: resolved OK");
        }
    }
    (<(u32, i32) => void>__proc_glPixelStorei)(pname, param);
}

let __proc_glPixelTransferf: Opaque = 0;

@dllname("glPixelTransferf")
export function glPixelTransferf(pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glPixelTransferf");
    if (__proc_glPixelTransferf === 0) {
        __proc_glPixelTransferf = GetProcAddress(__ensureRealOpenGL32(), "glPixelTransferf");
        if (__proc_glPixelTransferf === null) {
            console.error("[opengl32_enh]   glPixelTransferf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelTransferf: resolved OK");
        }
    }
    (<(u32, f32) => void>__proc_glPixelTransferf)(pname, param);
}

let __proc_glPixelTransferi: Opaque = 0;

@dllname("glPixelTransferi")
export function glPixelTransferi(pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glPixelTransferi");
    if (__proc_glPixelTransferi === 0) {
        __proc_glPixelTransferi = GetProcAddress(__ensureRealOpenGL32(), "glPixelTransferi");
        if (__proc_glPixelTransferi === null) {
            console.error("[opengl32_enh]   glPixelTransferi: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelTransferi: resolved OK");
        }
    }
    (<(u32, i32) => void>__proc_glPixelTransferi)(pname, param);
}

let __proc_glPixelZoom: Opaque = 0;

@dllname("glPixelZoom")
export function glPixelZoom(xfactor: f32, yfactor: f32): void {
    console.log("[opengl32_enh] call glPixelZoom");
    if (__proc_glPixelZoom === 0) {
        __proc_glPixelZoom = GetProcAddress(__ensureRealOpenGL32(), "glPixelZoom");
        if (__proc_glPixelZoom === null) {
            console.error("[opengl32_enh]   glPixelZoom: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPixelZoom: resolved OK");
        }
    }
    (<(f32, f32) => void>__proc_glPixelZoom)(xfactor, yfactor);
}

let __proc_glPointSize: Opaque = 0;

@dllname("glPointSize")
export function glPointSize(size: f32): void {
    console.log("[opengl32_enh] call glPointSize");
    if (__proc_glPointSize === 0) {
        __proc_glPointSize = GetProcAddress(__ensureRealOpenGL32(), "glPointSize");
        if (__proc_glPointSize === null) {
            console.error("[opengl32_enh]   glPointSize: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPointSize: resolved OK");
        }
    }
    (<(f32) => void>__proc_glPointSize)(size);
}

let __proc_glPolygonMode: Opaque = 0;

@dllname("glPolygonMode")
export function glPolygonMode(face: u32, mode: u32): void {
    console.log("[opengl32_enh] call glPolygonMode");
    if (__proc_glPolygonMode === 0) {
        __proc_glPolygonMode = GetProcAddress(__ensureRealOpenGL32(), "glPolygonMode");
        if (__proc_glPolygonMode === null) {
            console.error("[opengl32_enh]   glPolygonMode: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPolygonMode: resolved OK");
        }
    }
    (<(u32, u32) => void>__proc_glPolygonMode)(face, mode);
}

let __proc_glPolygonOffset: Opaque = 0;

@dllname("glPolygonOffset")
export function glPolygonOffset(factor: f32, units: f32): void {
    console.log("[opengl32_enh] call glPolygonOffset");
    if (__proc_glPolygonOffset === 0) {
        __proc_glPolygonOffset = GetProcAddress(__ensureRealOpenGL32(), "glPolygonOffset");
        if (__proc_glPolygonOffset === null) {
            console.error("[opengl32_enh]   glPolygonOffset: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPolygonOffset: resolved OK");
        }
    }
    (<(f32, f32) => void>__proc_glPolygonOffset)(factor, units);
}

let __proc_glPolygonStipple: Opaque = 0;

@dllname("glPolygonStipple")
export function glPolygonStipple(mask: Opaque): void {
    console.log("[opengl32_enh] call glPolygonStipple");
    if (__proc_glPolygonStipple === 0) {
        __proc_glPolygonStipple = GetProcAddress(__ensureRealOpenGL32(), "glPolygonStipple");
        if (__proc_glPolygonStipple === null) {
            console.error("[opengl32_enh]   glPolygonStipple: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPolygonStipple: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glPolygonStipple)(mask);
}

let __proc_glPopAttrib: Opaque = 0;

@dllname("glPopAttrib")
export function glPopAttrib(): void {
    console.log("[opengl32_enh] call glPopAttrib");
    if (__proc_glPopAttrib === 0) {
        __proc_glPopAttrib = GetProcAddress(__ensureRealOpenGL32(), "glPopAttrib");
        if (__proc_glPopAttrib === null) {
            console.error("[opengl32_enh]   glPopAttrib: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPopAttrib: resolved OK");
        }
    }
    (<() => void>__proc_glPopAttrib)();
}

let __proc_glPopClientAttrib: Opaque = 0;

@dllname("glPopClientAttrib")
export function glPopClientAttrib(): void {
    console.log("[opengl32_enh] call glPopClientAttrib");
    if (__proc_glPopClientAttrib === 0) {
        __proc_glPopClientAttrib = GetProcAddress(__ensureRealOpenGL32(), "glPopClientAttrib");
        if (__proc_glPopClientAttrib === null) {
            console.error("[opengl32_enh]   glPopClientAttrib: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPopClientAttrib: resolved OK");
        }
    }
    (<() => void>__proc_glPopClientAttrib)();
}

let __proc_glPopMatrix: Opaque = 0;

@dllname("glPopMatrix")
export function glPopMatrix(): void {
    console.log("[opengl32_enh] call glPopMatrix");
    if (__proc_glPopMatrix === 0) {
        __proc_glPopMatrix = GetProcAddress(__ensureRealOpenGL32(), "glPopMatrix");
        if (__proc_glPopMatrix === null) {
            console.error("[opengl32_enh]   glPopMatrix: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPopMatrix: resolved OK");
        }
    }
    (<() => void>__proc_glPopMatrix)();
}

let __proc_glPopName: Opaque = 0;

@dllname("glPopName")
export function glPopName(): void {
    console.log("[opengl32_enh] call glPopName");
    if (__proc_glPopName === 0) {
        __proc_glPopName = GetProcAddress(__ensureRealOpenGL32(), "glPopName");
        if (__proc_glPopName === null) {
            console.error("[opengl32_enh]   glPopName: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPopName: resolved OK");
        }
    }
    (<() => void>__proc_glPopName)();
}

let __proc_glPrioritizeTextures: Opaque = 0;

@dllname("glPrioritizeTextures")
export function glPrioritizeTextures(n: i32, textures: Opaque, priorities: Opaque): void {
    console.log("[opengl32_enh] call glPrioritizeTextures");
    if (__proc_glPrioritizeTextures === 0) {
        __proc_glPrioritizeTextures = GetProcAddress(__ensureRealOpenGL32(), "glPrioritizeTextures");
        if (__proc_glPrioritizeTextures === null) {
            console.error("[opengl32_enh]   glPrioritizeTextures: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPrioritizeTextures: resolved OK");
        }
    }
    (<(i32, Opaque, Opaque) => void>__proc_glPrioritizeTextures)(n, textures, priorities);
}

let __proc_glPushAttrib: Opaque = 0;

@dllname("glPushAttrib")
export function glPushAttrib(mask: u32): void {
    console.log("[opengl32_enh] call glPushAttrib");
    if (__proc_glPushAttrib === 0) {
        __proc_glPushAttrib = GetProcAddress(__ensureRealOpenGL32(), "glPushAttrib");
        if (__proc_glPushAttrib === null) {
            console.error("[opengl32_enh]   glPushAttrib: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPushAttrib: resolved OK");
        }
    }
    (<(u32) => void>__proc_glPushAttrib)(mask);
}

let __proc_glPushClientAttrib: Opaque = 0;

@dllname("glPushClientAttrib")
export function glPushClientAttrib(mask: u32): void {
    console.log("[opengl32_enh] call glPushClientAttrib");
    if (__proc_glPushClientAttrib === 0) {
        __proc_glPushClientAttrib = GetProcAddress(__ensureRealOpenGL32(), "glPushClientAttrib");
        if (__proc_glPushClientAttrib === null) {
            console.error("[opengl32_enh]   glPushClientAttrib: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPushClientAttrib: resolved OK");
        }
    }
    (<(u32) => void>__proc_glPushClientAttrib)(mask);
}

let __proc_glPushMatrix: Opaque = 0;

@dllname("glPushMatrix")
export function glPushMatrix(): void {
    console.log("[opengl32_enh] call glPushMatrix");
    if (__proc_glPushMatrix === 0) {
        __proc_glPushMatrix = GetProcAddress(__ensureRealOpenGL32(), "glPushMatrix");
        if (__proc_glPushMatrix === null) {
            console.error("[opengl32_enh]   glPushMatrix: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPushMatrix: resolved OK");
        }
    }
    (<() => void>__proc_glPushMatrix)();
}

let __proc_glPushName: Opaque = 0;

@dllname("glPushName")
export function glPushName(name: u32): void {
    console.log("[opengl32_enh] call glPushName");
    if (__proc_glPushName === 0) {
        __proc_glPushName = GetProcAddress(__ensureRealOpenGL32(), "glPushName");
        if (__proc_glPushName === null) {
            console.error("[opengl32_enh]   glPushName: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glPushName: resolved OK");
        }
    }
    (<(u32) => void>__proc_glPushName)(name);
}

let __proc_glRasterPos2d: Opaque = 0;

@dllname("glRasterPos2d")
export function glRasterPos2d(x: f64, y: f64): void {
    console.log("[opengl32_enh] call glRasterPos2d");
    if (__proc_glRasterPos2d === 0) {
        __proc_glRasterPos2d = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2d");
        if (__proc_glRasterPos2d === null) {
            console.error("[opengl32_enh]   glRasterPos2d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2d: resolved OK");
        }
    }
    (<(f64, f64) => void>__proc_glRasterPos2d)(x, y);
}

let __proc_glRasterPos2dv: Opaque = 0;

@dllname("glRasterPos2dv")
export function glRasterPos2dv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos2dv");
    if (__proc_glRasterPos2dv === 0) {
        __proc_glRasterPos2dv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2dv");
        if (__proc_glRasterPos2dv === null) {
            console.error("[opengl32_enh]   glRasterPos2dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos2dv)(v);
}

let __proc_glRasterPos2f: Opaque = 0;

@dllname("glRasterPos2f")
export function glRasterPos2f(x: f32, y: f32): void {
    console.log("[opengl32_enh] call glRasterPos2f");
    if (__proc_glRasterPos2f === 0) {
        __proc_glRasterPos2f = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2f");
        if (__proc_glRasterPos2f === null) {
            console.error("[opengl32_enh]   glRasterPos2f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2f: resolved OK");
        }
    }
    (<(f32, f32) => void>__proc_glRasterPos2f)(x, y);
}

let __proc_glRasterPos2fv: Opaque = 0;

@dllname("glRasterPos2fv")
export function glRasterPos2fv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos2fv");
    if (__proc_glRasterPos2fv === 0) {
        __proc_glRasterPos2fv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2fv");
        if (__proc_glRasterPos2fv === null) {
            console.error("[opengl32_enh]   glRasterPos2fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos2fv)(v);
}

let __proc_glRasterPos2i: Opaque = 0;

@dllname("glRasterPos2i")
export function glRasterPos2i(x: i32, y: i32): void {
    console.log("[opengl32_enh] call glRasterPos2i");
    if (__proc_glRasterPos2i === 0) {
        __proc_glRasterPos2i = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2i");
        if (__proc_glRasterPos2i === null) {
            console.error("[opengl32_enh]   glRasterPos2i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2i: resolved OK");
        }
    }
    (<(i32, i32) => void>__proc_glRasterPos2i)(x, y);
}

let __proc_glRasterPos2iv: Opaque = 0;

@dllname("glRasterPos2iv")
export function glRasterPos2iv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos2iv");
    if (__proc_glRasterPos2iv === 0) {
        __proc_glRasterPos2iv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2iv");
        if (__proc_glRasterPos2iv === null) {
            console.error("[opengl32_enh]   glRasterPos2iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos2iv)(v);
}

let __proc_glRasterPos2s: Opaque = 0;

@dllname("glRasterPos2s")
export function glRasterPos2s(x: i16, y: i16): void {
    console.log("[opengl32_enh] call glRasterPos2s");
    if (__proc_glRasterPos2s === 0) {
        __proc_glRasterPos2s = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2s");
        if (__proc_glRasterPos2s === null) {
            console.error("[opengl32_enh]   glRasterPos2s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2s: resolved OK");
        }
    }
    (<(i16, i16) => void>__proc_glRasterPos2s)(x, y);
}

let __proc_glRasterPos2sv: Opaque = 0;

@dllname("glRasterPos2sv")
export function glRasterPos2sv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos2sv");
    if (__proc_glRasterPos2sv === 0) {
        __proc_glRasterPos2sv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos2sv");
        if (__proc_glRasterPos2sv === null) {
            console.error("[opengl32_enh]   glRasterPos2sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos2sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos2sv)(v);
}

let __proc_glRasterPos3d: Opaque = 0;

@dllname("glRasterPos3d")
export function glRasterPos3d(x: f64, y: f64, z: f64): void {
    console.log("[opengl32_enh] call glRasterPos3d");
    if (__proc_glRasterPos3d === 0) {
        __proc_glRasterPos3d = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3d");
        if (__proc_glRasterPos3d === null) {
            console.error("[opengl32_enh]   glRasterPos3d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3d: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glRasterPos3d)(x, y, z);
}

let __proc_glRasterPos3dv: Opaque = 0;

@dllname("glRasterPos3dv")
export function glRasterPos3dv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos3dv");
    if (__proc_glRasterPos3dv === 0) {
        __proc_glRasterPos3dv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3dv");
        if (__proc_glRasterPos3dv === null) {
            console.error("[opengl32_enh]   glRasterPos3dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos3dv)(v);
}

let __proc_glRasterPos3f: Opaque = 0;

@dllname("glRasterPos3f")
export function glRasterPos3f(x: f32, y: f32, z: f32): void {
    console.log("[opengl32_enh] call glRasterPos3f");
    if (__proc_glRasterPos3f === 0) {
        __proc_glRasterPos3f = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3f");
        if (__proc_glRasterPos3f === null) {
            console.error("[opengl32_enh]   glRasterPos3f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3f: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glRasterPos3f)(x, y, z);
}

let __proc_glRasterPos3fv: Opaque = 0;

@dllname("glRasterPos3fv")
export function glRasterPos3fv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos3fv");
    if (__proc_glRasterPos3fv === 0) {
        __proc_glRasterPos3fv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3fv");
        if (__proc_glRasterPos3fv === null) {
            console.error("[opengl32_enh]   glRasterPos3fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos3fv)(v);
}

let __proc_glRasterPos3i: Opaque = 0;

@dllname("glRasterPos3i")
export function glRasterPos3i(x: i32, y: i32, z: i32): void {
    console.log("[opengl32_enh] call glRasterPos3i");
    if (__proc_glRasterPos3i === 0) {
        __proc_glRasterPos3i = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3i");
        if (__proc_glRasterPos3i === null) {
            console.error("[opengl32_enh]   glRasterPos3i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3i: resolved OK");
        }
    }
    (<(i32, i32, i32) => void>__proc_glRasterPos3i)(x, y, z);
}

let __proc_glRasterPos3iv: Opaque = 0;

@dllname("glRasterPos3iv")
export function glRasterPos3iv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos3iv");
    if (__proc_glRasterPos3iv === 0) {
        __proc_glRasterPos3iv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3iv");
        if (__proc_glRasterPos3iv === null) {
            console.error("[opengl32_enh]   glRasterPos3iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos3iv)(v);
}

let __proc_glRasterPos3s: Opaque = 0;

@dllname("glRasterPos3s")
export function glRasterPos3s(x: i16, y: i16, z: i16): void {
    console.log("[opengl32_enh] call glRasterPos3s");
    if (__proc_glRasterPos3s === 0) {
        __proc_glRasterPos3s = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3s");
        if (__proc_glRasterPos3s === null) {
            console.error("[opengl32_enh]   glRasterPos3s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3s: resolved OK");
        }
    }
    (<(i16, i16, i16) => void>__proc_glRasterPos3s)(x, y, z);
}

let __proc_glRasterPos3sv: Opaque = 0;

@dllname("glRasterPos3sv")
export function glRasterPos3sv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos3sv");
    if (__proc_glRasterPos3sv === 0) {
        __proc_glRasterPos3sv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos3sv");
        if (__proc_glRasterPos3sv === null) {
            console.error("[opengl32_enh]   glRasterPos3sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos3sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos3sv)(v);
}

let __proc_glRasterPos4d: Opaque = 0;

@dllname("glRasterPos4d")
export function glRasterPos4d(x: f64, y: f64, z: f64, w: f64): void {
    console.log("[opengl32_enh] call glRasterPos4d");
    if (__proc_glRasterPos4d === 0) {
        __proc_glRasterPos4d = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4d");
        if (__proc_glRasterPos4d === null) {
            console.error("[opengl32_enh]   glRasterPos4d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4d: resolved OK");
        }
    }
    (<(f64, f64, f64, f64) => void>__proc_glRasterPos4d)(x, y, z, w);
}

let __proc_glRasterPos4dv: Opaque = 0;

@dllname("glRasterPos4dv")
export function glRasterPos4dv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos4dv");
    if (__proc_glRasterPos4dv === 0) {
        __proc_glRasterPos4dv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4dv");
        if (__proc_glRasterPos4dv === null) {
            console.error("[opengl32_enh]   glRasterPos4dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos4dv)(v);
}

let __proc_glRasterPos4f: Opaque = 0;

@dllname("glRasterPos4f")
export function glRasterPos4f(x: f32, y: f32, z: f32, w: f32): void {
    console.log("[opengl32_enh] call glRasterPos4f");
    if (__proc_glRasterPos4f === 0) {
        __proc_glRasterPos4f = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4f");
        if (__proc_glRasterPos4f === null) {
            console.error("[opengl32_enh]   glRasterPos4f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4f: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glRasterPos4f)(x, y, z, w);
}

let __proc_glRasterPos4fv: Opaque = 0;

@dllname("glRasterPos4fv")
export function glRasterPos4fv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos4fv");
    if (__proc_glRasterPos4fv === 0) {
        __proc_glRasterPos4fv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4fv");
        if (__proc_glRasterPos4fv === null) {
            console.error("[opengl32_enh]   glRasterPos4fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos4fv)(v);
}

let __proc_glRasterPos4i: Opaque = 0;

@dllname("glRasterPos4i")
export function glRasterPos4i(x: i32, y: i32, z: i32, w: i32): void {
    console.log("[opengl32_enh] call glRasterPos4i");
    if (__proc_glRasterPos4i === 0) {
        __proc_glRasterPos4i = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4i");
        if (__proc_glRasterPos4i === null) {
            console.error("[opengl32_enh]   glRasterPos4i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4i: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glRasterPos4i)(x, y, z, w);
}

let __proc_glRasterPos4iv: Opaque = 0;

@dllname("glRasterPos4iv")
export function glRasterPos4iv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos4iv");
    if (__proc_glRasterPos4iv === 0) {
        __proc_glRasterPos4iv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4iv");
        if (__proc_glRasterPos4iv === null) {
            console.error("[opengl32_enh]   glRasterPos4iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos4iv)(v);
}

let __proc_glRasterPos4s: Opaque = 0;

@dllname("glRasterPos4s")
export function glRasterPos4s(x: i16, y: i16, z: i16, w: i16): void {
    console.log("[opengl32_enh] call glRasterPos4s");
    if (__proc_glRasterPos4s === 0) {
        __proc_glRasterPos4s = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4s");
        if (__proc_glRasterPos4s === null) {
            console.error("[opengl32_enh]   glRasterPos4s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4s: resolved OK");
        }
    }
    (<(i16, i16, i16, i16) => void>__proc_glRasterPos4s)(x, y, z, w);
}

let __proc_glRasterPos4sv: Opaque = 0;

@dllname("glRasterPos4sv")
export function glRasterPos4sv(v: Opaque): void {
    console.log("[opengl32_enh] call glRasterPos4sv");
    if (__proc_glRasterPos4sv === 0) {
        __proc_glRasterPos4sv = GetProcAddress(__ensureRealOpenGL32(), "glRasterPos4sv");
        if (__proc_glRasterPos4sv === null) {
            console.error("[opengl32_enh]   glRasterPos4sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRasterPos4sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glRasterPos4sv)(v);
}

let __proc_glReadBuffer: Opaque = 0;

@dllname("glReadBuffer")
export function glReadBuffer(mode: u32): void {
    console.log("[opengl32_enh] call glReadBuffer");
    if (__proc_glReadBuffer === 0) {
        __proc_glReadBuffer = GetProcAddress(__ensureRealOpenGL32(), "glReadBuffer");
        if (__proc_glReadBuffer === null) {
            console.error("[opengl32_enh]   glReadBuffer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glReadBuffer: resolved OK");
        }
    }
    (<(u32) => void>__proc_glReadBuffer)(mode);
}

let __proc_glReadPixels: Opaque = 0;

@dllname("glReadPixels")
export function glReadPixels(x: i32, y: i32, width: i32, height: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glReadPixels");
    if (__proc_glReadPixels === 0) {
        __proc_glReadPixels = GetProcAddress(__ensureRealOpenGL32(), "glReadPixels");
        if (__proc_glReadPixels === null) {
            console.error("[opengl32_enh]   glReadPixels: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glReadPixels: resolved OK");
        }
    }
    (<(i32, i32, i32, i32, u32, u32, Opaque) => void>__proc_glReadPixels)(x, y, width, height, format, type, pixels);
}

let __proc_glRectd: Opaque = 0;

@dllname("glRectd")
export function glRectd(x1: f64, y1: f64, x2: f64, y2: f64): void {
    console.log("[opengl32_enh] call glRectd");
    if (__proc_glRectd === 0) {
        __proc_glRectd = GetProcAddress(__ensureRealOpenGL32(), "glRectd");
        if (__proc_glRectd === null) {
            console.error("[opengl32_enh]   glRectd: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRectd: resolved OK");
        }
    }
    (<(f64, f64, f64, f64) => void>__proc_glRectd)(x1, y1, x2, y2);
}

let __proc_glRectdv: Opaque = 0;

@dllname("glRectdv")
export function glRectdv(v1: Opaque, v2: Opaque): void {
    console.log("[opengl32_enh] call glRectdv");
    if (__proc_glRectdv === 0) {
        __proc_glRectdv = GetProcAddress(__ensureRealOpenGL32(), "glRectdv");
        if (__proc_glRectdv === null) {
            console.error("[opengl32_enh]   glRectdv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRectdv: resolved OK");
        }
    }
    (<(Opaque, Opaque) => void>__proc_glRectdv)(v1, v2);
}

let __proc_glRectf: Opaque = 0;

@dllname("glRectf")
export function glRectf(x1: f32, y1: f32, x2: f32, y2: f32): void {
    console.log("[opengl32_enh] call glRectf");
    if (__proc_glRectf === 0) {
        __proc_glRectf = GetProcAddress(__ensureRealOpenGL32(), "glRectf");
        if (__proc_glRectf === null) {
            console.error("[opengl32_enh]   glRectf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRectf: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glRectf)(x1, y1, x2, y2);
}

let __proc_glRectfv: Opaque = 0;

@dllname("glRectfv")
export function glRectfv(v1: Opaque, v2: Opaque): void {
    console.log("[opengl32_enh] call glRectfv");
    if (__proc_glRectfv === 0) {
        __proc_glRectfv = GetProcAddress(__ensureRealOpenGL32(), "glRectfv");
        if (__proc_glRectfv === null) {
            console.error("[opengl32_enh]   glRectfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRectfv: resolved OK");
        }
    }
    (<(Opaque, Opaque) => void>__proc_glRectfv)(v1, v2);
}

let __proc_glRecti: Opaque = 0;

@dllname("glRecti")
export function glRecti(x1: i32, y1: i32, x2: i32, y2: i32): void {
    console.log("[opengl32_enh] call glRecti");
    if (__proc_glRecti === 0) {
        __proc_glRecti = GetProcAddress(__ensureRealOpenGL32(), "glRecti");
        if (__proc_glRecti === null) {
            console.error("[opengl32_enh]   glRecti: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRecti: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glRecti)(x1, y1, x2, y2);
}

let __proc_glRectiv: Opaque = 0;

@dllname("glRectiv")
export function glRectiv(v1: Opaque, v2: Opaque): void {
    console.log("[opengl32_enh] call glRectiv");
    if (__proc_glRectiv === 0) {
        __proc_glRectiv = GetProcAddress(__ensureRealOpenGL32(), "glRectiv");
        if (__proc_glRectiv === null) {
            console.error("[opengl32_enh]   glRectiv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRectiv: resolved OK");
        }
    }
    (<(Opaque, Opaque) => void>__proc_glRectiv)(v1, v2);
}

let __proc_glRects: Opaque = 0;

@dllname("glRects")
export function glRects(x1: i16, y1: i16, x2: i16, y2: i16): void {
    console.log("[opengl32_enh] call glRects");
    if (__proc_glRects === 0) {
        __proc_glRects = GetProcAddress(__ensureRealOpenGL32(), "glRects");
        if (__proc_glRects === null) {
            console.error("[opengl32_enh]   glRects: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRects: resolved OK");
        }
    }
    (<(i16, i16, i16, i16) => void>__proc_glRects)(x1, y1, x2, y2);
}

let __proc_glRectsv: Opaque = 0;

@dllname("glRectsv")
export function glRectsv(v1: Opaque, v2: Opaque): void {
    console.log("[opengl32_enh] call glRectsv");
    if (__proc_glRectsv === 0) {
        __proc_glRectsv = GetProcAddress(__ensureRealOpenGL32(), "glRectsv");
        if (__proc_glRectsv === null) {
            console.error("[opengl32_enh]   glRectsv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRectsv: resolved OK");
        }
    }
    (<(Opaque, Opaque) => void>__proc_glRectsv)(v1, v2);
}

let __proc_glRenderMode: Opaque = 0;

@dllname("glRenderMode")
export function glRenderMode(mode: u32): i32 {
    console.log("[opengl32_enh] call glRenderMode");
    if (__proc_glRenderMode === 0) {
        __proc_glRenderMode = GetProcAddress(__ensureRealOpenGL32(), "glRenderMode");
        if (__proc_glRenderMode === null) {
            console.error("[opengl32_enh]   glRenderMode: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRenderMode: resolved OK");
        }
    }
    return (<(u32) => i32>__proc_glRenderMode)(mode);
}

let __proc_glRotated: Opaque = 0;

@dllname("glRotated")
export function glRotated(angle: f64, x: f64, y: f64, z: f64): void {
    console.log("[opengl32_enh] call glRotated");
    if (__proc_glRotated === 0) {
        __proc_glRotated = GetProcAddress(__ensureRealOpenGL32(), "glRotated");
        if (__proc_glRotated === null) {
            console.error("[opengl32_enh]   glRotated: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRotated: resolved OK");
        }
    }
    (<(f64, f64, f64, f64) => void>__proc_glRotated)(angle, x, y, z);
}

let __proc_glRotatef: Opaque = 0;

@dllname("glRotatef")
export function glRotatef(angle: f32, x: f32, y: f32, z: f32): void {
    console.log("[opengl32_enh] call glRotatef");
    if (__proc_glRotatef === 0) {
        __proc_glRotatef = GetProcAddress(__ensureRealOpenGL32(), "glRotatef");
        if (__proc_glRotatef === null) {
            console.error("[opengl32_enh]   glRotatef: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glRotatef: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glRotatef)(angle, x, y, z);
}

let __proc_glScaled: Opaque = 0;

@dllname("glScaled")
export function glScaled(x: f64, y: f64, z: f64): void {
    console.log("[opengl32_enh] call glScaled");
    if (__proc_glScaled === 0) {
        __proc_glScaled = GetProcAddress(__ensureRealOpenGL32(), "glScaled");
        if (__proc_glScaled === null) {
            console.error("[opengl32_enh]   glScaled: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glScaled: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glScaled)(x, y, z);
}

let __proc_glScalef: Opaque = 0;

@dllname("glScalef")
export function glScalef(x: f32, y: f32, z: f32): void {
    console.log("[opengl32_enh] call glScalef");
    if (__proc_glScalef === 0) {
        __proc_glScalef = GetProcAddress(__ensureRealOpenGL32(), "glScalef");
        if (__proc_glScalef === null) {
            console.error("[opengl32_enh]   glScalef: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glScalef: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glScalef)(x, y, z);
}

let __proc_glScissor: Opaque = 0;

@dllname("glScissor")
export function glScissor(x: i32, y: i32, width: i32, height: i32): void {
    console.log("[opengl32_enh] call glScissor");
    if (__proc_glScissor === 0) {
        __proc_glScissor = GetProcAddress(__ensureRealOpenGL32(), "glScissor");
        if (__proc_glScissor === null) {
            console.error("[opengl32_enh]   glScissor: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glScissor: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glScissor)(x, y, width, height);
}

let __proc_glSelectBuffer: Opaque = 0;

@dllname("glSelectBuffer")
export function glSelectBuffer(size: i32, buffer: Opaque): void {
    console.log("[opengl32_enh] call glSelectBuffer");
    if (__proc_glSelectBuffer === 0) {
        __proc_glSelectBuffer = GetProcAddress(__ensureRealOpenGL32(), "glSelectBuffer");
        if (__proc_glSelectBuffer === null) {
            console.error("[opengl32_enh]   glSelectBuffer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glSelectBuffer: resolved OK");
        }
    }
    (<(i32, Opaque) => void>__proc_glSelectBuffer)(size, buffer);
}

let __proc_glShadeModel: Opaque = 0;

@dllname("glShadeModel")
export function glShadeModel(mode: u32): void {
    console.log("[opengl32_enh] call glShadeModel");
    if (__proc_glShadeModel === 0) {
        __proc_glShadeModel = GetProcAddress(__ensureRealOpenGL32(), "glShadeModel");
        if (__proc_glShadeModel === null) {
            console.error("[opengl32_enh]   glShadeModel: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glShadeModel: resolved OK");
        }
    }
    (<(u32) => void>__proc_glShadeModel)(mode);
}

let __proc_glStencilFunc: Opaque = 0;

@dllname("glStencilFunc")
export function glStencilFunc(func: u32, ref: i32, mask: u32): void {
    console.log("[opengl32_enh] call glStencilFunc");
    if (__proc_glStencilFunc === 0) {
        __proc_glStencilFunc = GetProcAddress(__ensureRealOpenGL32(), "glStencilFunc");
        if (__proc_glStencilFunc === null) {
            console.error("[opengl32_enh]   glStencilFunc: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glStencilFunc: resolved OK");
        }
    }
    (<(u32, i32, u32) => void>__proc_glStencilFunc)(func, ref, mask);
}

let __proc_glStencilMask: Opaque = 0;

@dllname("glStencilMask")
export function glStencilMask(mask: u32): void {
    console.log("[opengl32_enh] call glStencilMask");
    if (__proc_glStencilMask === 0) {
        __proc_glStencilMask = GetProcAddress(__ensureRealOpenGL32(), "glStencilMask");
        if (__proc_glStencilMask === null) {
            console.error("[opengl32_enh]   glStencilMask: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glStencilMask: resolved OK");
        }
    }
    (<(u32) => void>__proc_glStencilMask)(mask);
}

let __proc_glStencilOp: Opaque = 0;

@dllname("glStencilOp")
export function glStencilOp(fail: u32, zfail: u32, zpass: u32): void {
    console.log("[opengl32_enh] call glStencilOp");
    if (__proc_glStencilOp === 0) {
        __proc_glStencilOp = GetProcAddress(__ensureRealOpenGL32(), "glStencilOp");
        if (__proc_glStencilOp === null) {
            console.error("[opengl32_enh]   glStencilOp: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glStencilOp: resolved OK");
        }
    }
    (<(u32, u32, u32) => void>__proc_glStencilOp)(fail, zfail, zpass);
}

let __proc_glTexCoord1d: Opaque = 0;

@dllname("glTexCoord1d")
export function glTexCoord1d(s: f64): void {
    console.log("[opengl32_enh] call glTexCoord1d");
    if (__proc_glTexCoord1d === 0) {
        __proc_glTexCoord1d = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1d");
        if (__proc_glTexCoord1d === null) {
            console.error("[opengl32_enh]   glTexCoord1d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1d: resolved OK");
        }
    }
    (<(f64) => void>__proc_glTexCoord1d)(s);
}

let __proc_glTexCoord1dv: Opaque = 0;

@dllname("glTexCoord1dv")
export function glTexCoord1dv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord1dv");
    if (__proc_glTexCoord1dv === 0) {
        __proc_glTexCoord1dv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1dv");
        if (__proc_glTexCoord1dv === null) {
            console.error("[opengl32_enh]   glTexCoord1dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord1dv)(v);
}

let __proc_glTexCoord1f: Opaque = 0;

@dllname("glTexCoord1f")
export function glTexCoord1f(s: f32): void {
    console.log("[opengl32_enh] call glTexCoord1f");
    if (__proc_glTexCoord1f === 0) {
        __proc_glTexCoord1f = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1f");
        if (__proc_glTexCoord1f === null) {
            console.error("[opengl32_enh]   glTexCoord1f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1f: resolved OK");
        }
    }
    (<(f32) => void>__proc_glTexCoord1f)(s);
}

let __proc_glTexCoord1fv: Opaque = 0;

@dllname("glTexCoord1fv")
export function glTexCoord1fv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord1fv");
    if (__proc_glTexCoord1fv === 0) {
        __proc_glTexCoord1fv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1fv");
        if (__proc_glTexCoord1fv === null) {
            console.error("[opengl32_enh]   glTexCoord1fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord1fv)(v);
}

let __proc_glTexCoord1i: Opaque = 0;

@dllname("glTexCoord1i")
export function glTexCoord1i(s: i32): void {
    console.log("[opengl32_enh] call glTexCoord1i");
    if (__proc_glTexCoord1i === 0) {
        __proc_glTexCoord1i = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1i");
        if (__proc_glTexCoord1i === null) {
            console.error("[opengl32_enh]   glTexCoord1i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1i: resolved OK");
        }
    }
    (<(i32) => void>__proc_glTexCoord1i)(s);
}

let __proc_glTexCoord1iv: Opaque = 0;

@dllname("glTexCoord1iv")
export function glTexCoord1iv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord1iv");
    if (__proc_glTexCoord1iv === 0) {
        __proc_glTexCoord1iv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1iv");
        if (__proc_glTexCoord1iv === null) {
            console.error("[opengl32_enh]   glTexCoord1iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord1iv)(v);
}

let __proc_glTexCoord1s: Opaque = 0;

@dllname("glTexCoord1s")
export function glTexCoord1s(s: i16): void {
    console.log("[opengl32_enh] call glTexCoord1s");
    if (__proc_glTexCoord1s === 0) {
        __proc_glTexCoord1s = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1s");
        if (__proc_glTexCoord1s === null) {
            console.error("[opengl32_enh]   glTexCoord1s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1s: resolved OK");
        }
    }
    (<(i16) => void>__proc_glTexCoord1s)(s);
}

let __proc_glTexCoord1sv: Opaque = 0;

@dllname("glTexCoord1sv")
export function glTexCoord1sv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord1sv");
    if (__proc_glTexCoord1sv === 0) {
        __proc_glTexCoord1sv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord1sv");
        if (__proc_glTexCoord1sv === null) {
            console.error("[opengl32_enh]   glTexCoord1sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord1sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord1sv)(v);
}

let __proc_glTexCoord2d: Opaque = 0;

@dllname("glTexCoord2d")
export function glTexCoord2d(s: f64, t: f64): void {
    console.log("[opengl32_enh] call glTexCoord2d");
    if (__proc_glTexCoord2d === 0) {
        __proc_glTexCoord2d = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2d");
        if (__proc_glTexCoord2d === null) {
            console.error("[opengl32_enh]   glTexCoord2d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2d: resolved OK");
        }
    }
    (<(f64, f64) => void>__proc_glTexCoord2d)(s, t);
}

let __proc_glTexCoord2dv: Opaque = 0;

@dllname("glTexCoord2dv")
export function glTexCoord2dv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord2dv");
    if (__proc_glTexCoord2dv === 0) {
        __proc_glTexCoord2dv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2dv");
        if (__proc_glTexCoord2dv === null) {
            console.error("[opengl32_enh]   glTexCoord2dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord2dv)(v);
}

let __proc_glTexCoord2f: Opaque = 0;

@dllname("glTexCoord2f")
export function glTexCoord2f(s: f32, t: f32): void {
    console.log("[opengl32_enh] call glTexCoord2f");
    if (__proc_glTexCoord2f === 0) {
        __proc_glTexCoord2f = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2f");
        if (__proc_glTexCoord2f === null) {
            console.error("[opengl32_enh]   glTexCoord2f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2f: resolved OK");
        }
    }
    (<(f32, f32) => void>__proc_glTexCoord2f)(s, t);
}

let __proc_glTexCoord2fv: Opaque = 0;

@dllname("glTexCoord2fv")
export function glTexCoord2fv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord2fv");
    if (__proc_glTexCoord2fv === 0) {
        __proc_glTexCoord2fv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2fv");
        if (__proc_glTexCoord2fv === null) {
            console.error("[opengl32_enh]   glTexCoord2fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord2fv)(v);
}

let __proc_glTexCoord2i: Opaque = 0;

@dllname("glTexCoord2i")
export function glTexCoord2i(s: i32, t: i32): void {
    console.log("[opengl32_enh] call glTexCoord2i");
    if (__proc_glTexCoord2i === 0) {
        __proc_glTexCoord2i = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2i");
        if (__proc_glTexCoord2i === null) {
            console.error("[opengl32_enh]   glTexCoord2i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2i: resolved OK");
        }
    }
    (<(i32, i32) => void>__proc_glTexCoord2i)(s, t);
}

let __proc_glTexCoord2iv: Opaque = 0;

@dllname("glTexCoord2iv")
export function glTexCoord2iv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord2iv");
    if (__proc_glTexCoord2iv === 0) {
        __proc_glTexCoord2iv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2iv");
        if (__proc_glTexCoord2iv === null) {
            console.error("[opengl32_enh]   glTexCoord2iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord2iv)(v);
}

let __proc_glTexCoord2s: Opaque = 0;

@dllname("glTexCoord2s")
export function glTexCoord2s(s: i16, t: i16): void {
    console.log("[opengl32_enh] call glTexCoord2s");
    if (__proc_glTexCoord2s === 0) {
        __proc_glTexCoord2s = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2s");
        if (__proc_glTexCoord2s === null) {
            console.error("[opengl32_enh]   glTexCoord2s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2s: resolved OK");
        }
    }
    (<(i16, i16) => void>__proc_glTexCoord2s)(s, t);
}

let __proc_glTexCoord2sv: Opaque = 0;

@dllname("glTexCoord2sv")
export function glTexCoord2sv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord2sv");
    if (__proc_glTexCoord2sv === 0) {
        __proc_glTexCoord2sv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord2sv");
        if (__proc_glTexCoord2sv === null) {
            console.error("[opengl32_enh]   glTexCoord2sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord2sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord2sv)(v);
}

let __proc_glTexCoord3d: Opaque = 0;

@dllname("glTexCoord3d")
export function glTexCoord3d(s: f64, t: f64, r: f64): void {
    console.log("[opengl32_enh] call glTexCoord3d");
    if (__proc_glTexCoord3d === 0) {
        __proc_glTexCoord3d = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3d");
        if (__proc_glTexCoord3d === null) {
            console.error("[opengl32_enh]   glTexCoord3d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3d: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glTexCoord3d)(s, t, r);
}

let __proc_glTexCoord3dv: Opaque = 0;

@dllname("glTexCoord3dv")
export function glTexCoord3dv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord3dv");
    if (__proc_glTexCoord3dv === 0) {
        __proc_glTexCoord3dv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3dv");
        if (__proc_glTexCoord3dv === null) {
            console.error("[opengl32_enh]   glTexCoord3dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord3dv)(v);
}

let __proc_glTexCoord3f: Opaque = 0;

@dllname("glTexCoord3f")
export function glTexCoord3f(s: f32, t: f32, r: f32): void {
    console.log("[opengl32_enh] call glTexCoord3f");
    if (__proc_glTexCoord3f === 0) {
        __proc_glTexCoord3f = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3f");
        if (__proc_glTexCoord3f === null) {
            console.error("[opengl32_enh]   glTexCoord3f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3f: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glTexCoord3f)(s, t, r);
}

let __proc_glTexCoord3fv: Opaque = 0;

@dllname("glTexCoord3fv")
export function glTexCoord3fv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord3fv");
    if (__proc_glTexCoord3fv === 0) {
        __proc_glTexCoord3fv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3fv");
        if (__proc_glTexCoord3fv === null) {
            console.error("[opengl32_enh]   glTexCoord3fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord3fv)(v);
}

let __proc_glTexCoord3i: Opaque = 0;

@dllname("glTexCoord3i")
export function glTexCoord3i(s: i32, t: i32, r: i32): void {
    console.log("[opengl32_enh] call glTexCoord3i");
    if (__proc_glTexCoord3i === 0) {
        __proc_glTexCoord3i = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3i");
        if (__proc_glTexCoord3i === null) {
            console.error("[opengl32_enh]   glTexCoord3i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3i: resolved OK");
        }
    }
    (<(i32, i32, i32) => void>__proc_glTexCoord3i)(s, t, r);
}

let __proc_glTexCoord3iv: Opaque = 0;

@dllname("glTexCoord3iv")
export function glTexCoord3iv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord3iv");
    if (__proc_glTexCoord3iv === 0) {
        __proc_glTexCoord3iv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3iv");
        if (__proc_glTexCoord3iv === null) {
            console.error("[opengl32_enh]   glTexCoord3iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord3iv)(v);
}

let __proc_glTexCoord3s: Opaque = 0;

@dllname("glTexCoord3s")
export function glTexCoord3s(s: i16, t: i16, r: i16): void {
    console.log("[opengl32_enh] call glTexCoord3s");
    if (__proc_glTexCoord3s === 0) {
        __proc_glTexCoord3s = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3s");
        if (__proc_glTexCoord3s === null) {
            console.error("[opengl32_enh]   glTexCoord3s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3s: resolved OK");
        }
    }
    (<(i16, i16, i16) => void>__proc_glTexCoord3s)(s, t, r);
}

let __proc_glTexCoord3sv: Opaque = 0;

@dllname("glTexCoord3sv")
export function glTexCoord3sv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord3sv");
    if (__proc_glTexCoord3sv === 0) {
        __proc_glTexCoord3sv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord3sv");
        if (__proc_glTexCoord3sv === null) {
            console.error("[opengl32_enh]   glTexCoord3sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord3sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord3sv)(v);
}

let __proc_glTexCoord4d: Opaque = 0;

@dllname("glTexCoord4d")
export function glTexCoord4d(s: f64, t: f64, r: f64, q: f64): void {
    console.log("[opengl32_enh] call glTexCoord4d");
    if (__proc_glTexCoord4d === 0) {
        __proc_glTexCoord4d = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4d");
        if (__proc_glTexCoord4d === null) {
            console.error("[opengl32_enh]   glTexCoord4d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4d: resolved OK");
        }
    }
    (<(f64, f64, f64, f64) => void>__proc_glTexCoord4d)(s, t, r, q);
}

let __proc_glTexCoord4dv: Opaque = 0;

@dllname("glTexCoord4dv")
export function glTexCoord4dv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord4dv");
    if (__proc_glTexCoord4dv === 0) {
        __proc_glTexCoord4dv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4dv");
        if (__proc_glTexCoord4dv === null) {
            console.error("[opengl32_enh]   glTexCoord4dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord4dv)(v);
}

let __proc_glTexCoord4f: Opaque = 0;

@dllname("glTexCoord4f")
export function glTexCoord4f(s: f32, t: f32, r: f32, q: f32): void {
    console.log("[opengl32_enh] call glTexCoord4f");
    if (__proc_glTexCoord4f === 0) {
        __proc_glTexCoord4f = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4f");
        if (__proc_glTexCoord4f === null) {
            console.error("[opengl32_enh]   glTexCoord4f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4f: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glTexCoord4f)(s, t, r, q);
}

let __proc_glTexCoord4fv: Opaque = 0;

@dllname("glTexCoord4fv")
export function glTexCoord4fv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord4fv");
    if (__proc_glTexCoord4fv === 0) {
        __proc_glTexCoord4fv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4fv");
        if (__proc_glTexCoord4fv === null) {
            console.error("[opengl32_enh]   glTexCoord4fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord4fv)(v);
}

let __proc_glTexCoord4i: Opaque = 0;

@dllname("glTexCoord4i")
export function glTexCoord4i(s: i32, t: i32, r: i32, q: i32): void {
    console.log("[opengl32_enh] call glTexCoord4i");
    if (__proc_glTexCoord4i === 0) {
        __proc_glTexCoord4i = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4i");
        if (__proc_glTexCoord4i === null) {
            console.error("[opengl32_enh]   glTexCoord4i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4i: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glTexCoord4i)(s, t, r, q);
}

let __proc_glTexCoord4iv: Opaque = 0;

@dllname("glTexCoord4iv")
export function glTexCoord4iv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord4iv");
    if (__proc_glTexCoord4iv === 0) {
        __proc_glTexCoord4iv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4iv");
        if (__proc_glTexCoord4iv === null) {
            console.error("[opengl32_enh]   glTexCoord4iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord4iv)(v);
}

let __proc_glTexCoord4s: Opaque = 0;

@dllname("glTexCoord4s")
export function glTexCoord4s(s: i16, t: i16, r: i16, q: i16): void {
    console.log("[opengl32_enh] call glTexCoord4s");
    if (__proc_glTexCoord4s === 0) {
        __proc_glTexCoord4s = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4s");
        if (__proc_glTexCoord4s === null) {
            console.error("[opengl32_enh]   glTexCoord4s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4s: resolved OK");
        }
    }
    (<(i16, i16, i16, i16) => void>__proc_glTexCoord4s)(s, t, r, q);
}

let __proc_glTexCoord4sv: Opaque = 0;

@dllname("glTexCoord4sv")
export function glTexCoord4sv(v: Opaque): void {
    console.log("[opengl32_enh] call glTexCoord4sv");
    if (__proc_glTexCoord4sv === 0) {
        __proc_glTexCoord4sv = GetProcAddress(__ensureRealOpenGL32(), "glTexCoord4sv");
        if (__proc_glTexCoord4sv === null) {
            console.error("[opengl32_enh]   glTexCoord4sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoord4sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glTexCoord4sv)(v);
}

let __proc_glTexCoordPointer: Opaque = 0;

@dllname("glTexCoordPointer")
export function glTexCoordPointer(size: i32, type: u32, stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glTexCoordPointer");
    if (__proc_glTexCoordPointer === 0) {
        __proc_glTexCoordPointer = GetProcAddress(__ensureRealOpenGL32(), "glTexCoordPointer");
        if (__proc_glTexCoordPointer === null) {
            console.error("[opengl32_enh]   glTexCoordPointer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexCoordPointer: resolved OK");
        }
    }
    (<(i32, u32, i32, Opaque) => void>__proc_glTexCoordPointer)(size, type, stride, pointer);
}

let __proc_glTexEnvf: Opaque = 0;

@dllname("glTexEnvf")
export function glTexEnvf(target: u32, pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glTexEnvf");
    if (__proc_glTexEnvf === 0) {
        __proc_glTexEnvf = GetProcAddress(__ensureRealOpenGL32(), "glTexEnvf");
        if (__proc_glTexEnvf === null) {
            console.error("[opengl32_enh]   glTexEnvf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexEnvf: resolved OK");
        }
    }
    (<(u32, u32, f32) => void>__proc_glTexEnvf)(target, pname, param);
}

let __proc_glTexEnvfv: Opaque = 0;

@dllname("glTexEnvfv")
export function glTexEnvfv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexEnvfv");
    if (__proc_glTexEnvfv === 0) {
        __proc_glTexEnvfv = GetProcAddress(__ensureRealOpenGL32(), "glTexEnvfv");
        if (__proc_glTexEnvfv === null) {
            console.error("[opengl32_enh]   glTexEnvfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexEnvfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexEnvfv)(target, pname, params);
}

let __proc_glTexEnvi: Opaque = 0;

@dllname("glTexEnvi")
export function glTexEnvi(target: u32, pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glTexEnvi");
    if (__proc_glTexEnvi === 0) {
        __proc_glTexEnvi = GetProcAddress(__ensureRealOpenGL32(), "glTexEnvi");
        if (__proc_glTexEnvi === null) {
            console.error("[opengl32_enh]   glTexEnvi: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexEnvi: resolved OK");
        }
    }
    (<(u32, u32, i32) => void>__proc_glTexEnvi)(target, pname, param);
}

let __proc_glTexEnviv: Opaque = 0;

@dllname("glTexEnviv")
export function glTexEnviv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexEnviv");
    if (__proc_glTexEnviv === 0) {
        __proc_glTexEnviv = GetProcAddress(__ensureRealOpenGL32(), "glTexEnviv");
        if (__proc_glTexEnviv === null) {
            console.error("[opengl32_enh]   glTexEnviv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexEnviv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexEnviv)(target, pname, params);
}

let __proc_glTexGend: Opaque = 0;

@dllname("glTexGend")
export function glTexGend(coord: u32, pname: u32, param: f64): void {
    console.log("[opengl32_enh] call glTexGend");
    if (__proc_glTexGend === 0) {
        __proc_glTexGend = GetProcAddress(__ensureRealOpenGL32(), "glTexGend");
        if (__proc_glTexGend === null) {
            console.error("[opengl32_enh]   glTexGend: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexGend: resolved OK");
        }
    }
    (<(u32, u32, f64) => void>__proc_glTexGend)(coord, pname, param);
}

let __proc_glTexGendv: Opaque = 0;

@dllname("glTexGendv")
export function glTexGendv(coord: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexGendv");
    if (__proc_glTexGendv === 0) {
        __proc_glTexGendv = GetProcAddress(__ensureRealOpenGL32(), "glTexGendv");
        if (__proc_glTexGendv === null) {
            console.error("[opengl32_enh]   glTexGendv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexGendv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexGendv)(coord, pname, params);
}

let __proc_glTexGenf: Opaque = 0;

@dllname("glTexGenf")
export function glTexGenf(coord: u32, pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glTexGenf");
    if (__proc_glTexGenf === 0) {
        __proc_glTexGenf = GetProcAddress(__ensureRealOpenGL32(), "glTexGenf");
        if (__proc_glTexGenf === null) {
            console.error("[opengl32_enh]   glTexGenf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexGenf: resolved OK");
        }
    }
    (<(u32, u32, f32) => void>__proc_glTexGenf)(coord, pname, param);
}

let __proc_glTexGenfv: Opaque = 0;

@dllname("glTexGenfv")
export function glTexGenfv(coord: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexGenfv");
    if (__proc_glTexGenfv === 0) {
        __proc_glTexGenfv = GetProcAddress(__ensureRealOpenGL32(), "glTexGenfv");
        if (__proc_glTexGenfv === null) {
            console.error("[opengl32_enh]   glTexGenfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexGenfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexGenfv)(coord, pname, params);
}

let __proc_glTexGeni: Opaque = 0;

@dllname("glTexGeni")
export function glTexGeni(coord: u32, pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glTexGeni");
    if (__proc_glTexGeni === 0) {
        __proc_glTexGeni = GetProcAddress(__ensureRealOpenGL32(), "glTexGeni");
        if (__proc_glTexGeni === null) {
            console.error("[opengl32_enh]   glTexGeni: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexGeni: resolved OK");
        }
    }
    (<(u32, u32, i32) => void>__proc_glTexGeni)(coord, pname, param);
}

let __proc_glTexGeniv: Opaque = 0;

@dllname("glTexGeniv")
export function glTexGeniv(coord: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexGeniv");
    if (__proc_glTexGeniv === 0) {
        __proc_glTexGeniv = GetProcAddress(__ensureRealOpenGL32(), "glTexGeniv");
        if (__proc_glTexGeniv === null) {
            console.error("[opengl32_enh]   glTexGeniv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexGeniv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexGeniv)(coord, pname, params);
}

let __proc_glTexImage1D: Opaque = 0;

@dllname("glTexImage1D")
export function glTexImage1D(target: u32, level: i32, internalformat: i32, width: i32, border: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glTexImage1D");
    if (__proc_glTexImage1D === 0) {
        __proc_glTexImage1D = GetProcAddress(__ensureRealOpenGL32(), "glTexImage1D");
        if (__proc_glTexImage1D === null) {
            console.error("[opengl32_enh]   glTexImage1D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexImage1D: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, i32, u32, u32, Opaque) => void>__proc_glTexImage1D)(target, level, internalformat, width, border, format, type, pixels);
}

let __proc_glTexImage2D: Opaque = 0;

@dllname("glTexImage2D")
export function glTexImage2D(target: u32, level: i32, internalformat: i32, width: i32, height: i32, border: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glTexImage2D");
    if (__proc_glTexImage2D === 0) {
        __proc_glTexImage2D = GetProcAddress(__ensureRealOpenGL32(), "glTexImage2D");
        if (__proc_glTexImage2D === null) {
            console.error("[opengl32_enh]   glTexImage2D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexImage2D: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, i32, i32, u32, u32, Opaque) => void>__proc_glTexImage2D)(target, level, internalformat, width, height, border, format, type, pixels);
}

let __proc_glTexParameterf: Opaque = 0;

@dllname("glTexParameterf")
export function glTexParameterf(target: u32, pname: u32, param: f32): void {
    console.log("[opengl32_enh] call glTexParameterf");
    if (__proc_glTexParameterf === 0) {
        __proc_glTexParameterf = GetProcAddress(__ensureRealOpenGL32(), "glTexParameterf");
        if (__proc_glTexParameterf === null) {
            console.error("[opengl32_enh]   glTexParameterf: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexParameterf: resolved OK");
        }
    }
    (<(u32, u32, f32) => void>__proc_glTexParameterf)(target, pname, param);
}

let __proc_glTexParameterfv: Opaque = 0;

@dllname("glTexParameterfv")
export function glTexParameterfv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexParameterfv");
    if (__proc_glTexParameterfv === 0) {
        __proc_glTexParameterfv = GetProcAddress(__ensureRealOpenGL32(), "glTexParameterfv");
        if (__proc_glTexParameterfv === null) {
            console.error("[opengl32_enh]   glTexParameterfv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexParameterfv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexParameterfv)(target, pname, params);
}

let __proc_glTexParameteri: Opaque = 0;

@dllname("glTexParameteri")
export function glTexParameteri(target: u32, pname: u32, param: i32): void {
    console.log("[opengl32_enh] call glTexParameteri");
    if (__proc_glTexParameteri === 0) {
        __proc_glTexParameteri = GetProcAddress(__ensureRealOpenGL32(), "glTexParameteri");
        if (__proc_glTexParameteri === null) {
            console.error("[opengl32_enh]   glTexParameteri: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexParameteri: resolved OK");
        }
    }
    (<(u32, u32, i32) => void>__proc_glTexParameteri)(target, pname, param);
}

let __proc_glTexParameteriv: Opaque = 0;

@dllname("glTexParameteriv")
export function glTexParameteriv(target: u32, pname: u32, params: Opaque): void {
    console.log("[opengl32_enh] call glTexParameteriv");
    if (__proc_glTexParameteriv === 0) {
        __proc_glTexParameteriv = GetProcAddress(__ensureRealOpenGL32(), "glTexParameteriv");
        if (__proc_glTexParameteriv === null) {
            console.error("[opengl32_enh]   glTexParameteriv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexParameteriv: resolved OK");
        }
    }
    (<(u32, u32, Opaque) => void>__proc_glTexParameteriv)(target, pname, params);
}

let __proc_glTexSubImage1D: Opaque = 0;

@dllname("glTexSubImage1D")
export function glTexSubImage1D(target: u32, level: i32, xoffset: i32, width: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glTexSubImage1D");
    if (__proc_glTexSubImage1D === 0) {
        __proc_glTexSubImage1D = GetProcAddress(__ensureRealOpenGL32(), "glTexSubImage1D");
        if (__proc_glTexSubImage1D === null) {
            console.error("[opengl32_enh]   glTexSubImage1D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexSubImage1D: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, u32, u32, Opaque) => void>__proc_glTexSubImage1D)(target, level, xoffset, width, format, type, pixels);
}

let __proc_glTexSubImage2D: Opaque = 0;

@dllname("glTexSubImage2D")
export function glTexSubImage2D(target: u32, level: i32, xoffset: i32, yoffset: i32, width: i32, height: i32, format: u32, type: u32, pixels: Opaque): void {
    console.log("[opengl32_enh] call glTexSubImage2D");
    if (__proc_glTexSubImage2D === 0) {
        __proc_glTexSubImage2D = GetProcAddress(__ensureRealOpenGL32(), "glTexSubImage2D");
        if (__proc_glTexSubImage2D === null) {
            console.error("[opengl32_enh]   glTexSubImage2D: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTexSubImage2D: resolved OK");
        }
    }
    (<(u32, i32, i32, i32, i32, i32, u32, u32, Opaque) => void>__proc_glTexSubImage2D)(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

let __proc_glTranslated: Opaque = 0;

@dllname("glTranslated")
export function glTranslated(x: f64, y: f64, z: f64): void {
    console.log("[opengl32_enh] call glTranslated");
    if (__proc_glTranslated === 0) {
        __proc_glTranslated = GetProcAddress(__ensureRealOpenGL32(), "glTranslated");
        if (__proc_glTranslated === null) {
            console.error("[opengl32_enh]   glTranslated: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTranslated: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glTranslated)(x, y, z);
}

let __proc_glTranslatef: Opaque = 0;

@dllname("glTranslatef")
export function glTranslatef(x: f32, y: f32, z: f32): void {
    console.log("[opengl32_enh] call glTranslatef");
    if (__proc_glTranslatef === 0) {
        __proc_glTranslatef = GetProcAddress(__ensureRealOpenGL32(), "glTranslatef");
        if (__proc_glTranslatef === null) {
            console.error("[opengl32_enh]   glTranslatef: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glTranslatef: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glTranslatef)(x, y, z);
}

let __proc_glVertex2d: Opaque = 0;

@dllname("glVertex2d")
export function glVertex2d(x: f64, y: f64): void {
    console.log("[opengl32_enh] call glVertex2d");
    if (__proc_glVertex2d === 0) {
        __proc_glVertex2d = GetProcAddress(__ensureRealOpenGL32(), "glVertex2d");
        if (__proc_glVertex2d === null) {
            console.error("[opengl32_enh]   glVertex2d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2d: resolved OK");
        }
    }
    (<(f64, f64) => void>__proc_glVertex2d)(x, y);
}

let __proc_glVertex2dv: Opaque = 0;

@dllname("glVertex2dv")
export function glVertex2dv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex2dv");
    if (__proc_glVertex2dv === 0) {
        __proc_glVertex2dv = GetProcAddress(__ensureRealOpenGL32(), "glVertex2dv");
        if (__proc_glVertex2dv === null) {
            console.error("[opengl32_enh]   glVertex2dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex2dv)(v);
}

let __proc_glVertex2f: Opaque = 0;

@dllname("glVertex2f")
export function glVertex2f(x: f32, y: f32): void {
    console.log("[opengl32_enh] call glVertex2f");
    if (__proc_glVertex2f === 0) {
        __proc_glVertex2f = GetProcAddress(__ensureRealOpenGL32(), "glVertex2f");
        if (__proc_glVertex2f === null) {
            console.error("[opengl32_enh]   glVertex2f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2f: resolved OK");
        }
    }
    (<(f32, f32) => void>__proc_glVertex2f)(x, y);
}

let __proc_glVertex2fv: Opaque = 0;

@dllname("glVertex2fv")
export function glVertex2fv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex2fv");
    if (__proc_glVertex2fv === 0) {
        __proc_glVertex2fv = GetProcAddress(__ensureRealOpenGL32(), "glVertex2fv");
        if (__proc_glVertex2fv === null) {
            console.error("[opengl32_enh]   glVertex2fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex2fv)(v);
}

let __proc_glVertex2i: Opaque = 0;

@dllname("glVertex2i")
export function glVertex2i(x: i32, y: i32): void {
    console.log("[opengl32_enh] call glVertex2i");
    if (__proc_glVertex2i === 0) {
        __proc_glVertex2i = GetProcAddress(__ensureRealOpenGL32(), "glVertex2i");
        if (__proc_glVertex2i === null) {
            console.error("[opengl32_enh]   glVertex2i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2i: resolved OK");
        }
    }
    (<(i32, i32) => void>__proc_glVertex2i)(x, y);
}

let __proc_glVertex2iv: Opaque = 0;

@dllname("glVertex2iv")
export function glVertex2iv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex2iv");
    if (__proc_glVertex2iv === 0) {
        __proc_glVertex2iv = GetProcAddress(__ensureRealOpenGL32(), "glVertex2iv");
        if (__proc_glVertex2iv === null) {
            console.error("[opengl32_enh]   glVertex2iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex2iv)(v);
}

let __proc_glVertex2s: Opaque = 0;

@dllname("glVertex2s")
export function glVertex2s(x: i16, y: i16): void {
    console.log("[opengl32_enh] call glVertex2s");
    if (__proc_glVertex2s === 0) {
        __proc_glVertex2s = GetProcAddress(__ensureRealOpenGL32(), "glVertex2s");
        if (__proc_glVertex2s === null) {
            console.error("[opengl32_enh]   glVertex2s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2s: resolved OK");
        }
    }
    (<(i16, i16) => void>__proc_glVertex2s)(x, y);
}

let __proc_glVertex2sv: Opaque = 0;

@dllname("glVertex2sv")
export function glVertex2sv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex2sv");
    if (__proc_glVertex2sv === 0) {
        __proc_glVertex2sv = GetProcAddress(__ensureRealOpenGL32(), "glVertex2sv");
        if (__proc_glVertex2sv === null) {
            console.error("[opengl32_enh]   glVertex2sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex2sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex2sv)(v);
}

let __proc_glVertex3d: Opaque = 0;

@dllname("glVertex3d")
export function glVertex3d(x: f64, y: f64, z: f64): void {
    console.log("[opengl32_enh] call glVertex3d");
    if (__proc_glVertex3d === 0) {
        __proc_glVertex3d = GetProcAddress(__ensureRealOpenGL32(), "glVertex3d");
        if (__proc_glVertex3d === null) {
            console.error("[opengl32_enh]   glVertex3d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3d: resolved OK");
        }
    }
    (<(f64, f64, f64) => void>__proc_glVertex3d)(x, y, z);
}

let __proc_glVertex3dv: Opaque = 0;

@dllname("glVertex3dv")
export function glVertex3dv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex3dv");
    if (__proc_glVertex3dv === 0) {
        __proc_glVertex3dv = GetProcAddress(__ensureRealOpenGL32(), "glVertex3dv");
        if (__proc_glVertex3dv === null) {
            console.error("[opengl32_enh]   glVertex3dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex3dv)(v);
}

let __proc_glVertex3f: Opaque = 0;

@dllname("glVertex3f")
export function glVertex3f(x: f32, y: f32, z: f32): void {
    console.log("[opengl32_enh] call glVertex3f");
    if (__proc_glVertex3f === 0) {
        __proc_glVertex3f = GetProcAddress(__ensureRealOpenGL32(), "glVertex3f");
        if (__proc_glVertex3f === null) {
            console.error("[opengl32_enh]   glVertex3f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3f: resolved OK");
        }
    }
    (<(f32, f32, f32) => void>__proc_glVertex3f)(x, y, z);
}

let __proc_glVertex3fv: Opaque = 0;

@dllname("glVertex3fv")
export function glVertex3fv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex3fv");
    if (__proc_glVertex3fv === 0) {
        __proc_glVertex3fv = GetProcAddress(__ensureRealOpenGL32(), "glVertex3fv");
        if (__proc_glVertex3fv === null) {
            console.error("[opengl32_enh]   glVertex3fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex3fv)(v);
}

let __proc_glVertex3i: Opaque = 0;

@dllname("glVertex3i")
export function glVertex3i(x: i32, y: i32, z: i32): void {
    console.log("[opengl32_enh] call glVertex3i");
    if (__proc_glVertex3i === 0) {
        __proc_glVertex3i = GetProcAddress(__ensureRealOpenGL32(), "glVertex3i");
        if (__proc_glVertex3i === null) {
            console.error("[opengl32_enh]   glVertex3i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3i: resolved OK");
        }
    }
    (<(i32, i32, i32) => void>__proc_glVertex3i)(x, y, z);
}

let __proc_glVertex3iv: Opaque = 0;

@dllname("glVertex3iv")
export function glVertex3iv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex3iv");
    if (__proc_glVertex3iv === 0) {
        __proc_glVertex3iv = GetProcAddress(__ensureRealOpenGL32(), "glVertex3iv");
        if (__proc_glVertex3iv === null) {
            console.error("[opengl32_enh]   glVertex3iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex3iv)(v);
}

let __proc_glVertex3s: Opaque = 0;

@dllname("glVertex3s")
export function glVertex3s(x: i16, y: i16, z: i16): void {
    console.log("[opengl32_enh] call glVertex3s");
    if (__proc_glVertex3s === 0) {
        __proc_glVertex3s = GetProcAddress(__ensureRealOpenGL32(), "glVertex3s");
        if (__proc_glVertex3s === null) {
            console.error("[opengl32_enh]   glVertex3s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3s: resolved OK");
        }
    }
    (<(i16, i16, i16) => void>__proc_glVertex3s)(x, y, z);
}

let __proc_glVertex3sv: Opaque = 0;

@dllname("glVertex3sv")
export function glVertex3sv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex3sv");
    if (__proc_glVertex3sv === 0) {
        __proc_glVertex3sv = GetProcAddress(__ensureRealOpenGL32(), "glVertex3sv");
        if (__proc_glVertex3sv === null) {
            console.error("[opengl32_enh]   glVertex3sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex3sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex3sv)(v);
}

let __proc_glVertex4d: Opaque = 0;

@dllname("glVertex4d")
export function glVertex4d(x: f64, y: f64, z: f64, w: f64): void {
    console.log("[opengl32_enh] call glVertex4d");
    if (__proc_glVertex4d === 0) {
        __proc_glVertex4d = GetProcAddress(__ensureRealOpenGL32(), "glVertex4d");
        if (__proc_glVertex4d === null) {
            console.error("[opengl32_enh]   glVertex4d: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4d: resolved OK");
        }
    }
    (<(f64, f64, f64, f64) => void>__proc_glVertex4d)(x, y, z, w);
}

let __proc_glVertex4dv: Opaque = 0;

@dllname("glVertex4dv")
export function glVertex4dv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex4dv");
    if (__proc_glVertex4dv === 0) {
        __proc_glVertex4dv = GetProcAddress(__ensureRealOpenGL32(), "glVertex4dv");
        if (__proc_glVertex4dv === null) {
            console.error("[opengl32_enh]   glVertex4dv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4dv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex4dv)(v);
}

let __proc_glVertex4f: Opaque = 0;

@dllname("glVertex4f")
export function glVertex4f(x: f32, y: f32, z: f32, w: f32): void {
    console.log("[opengl32_enh] call glVertex4f");
    if (__proc_glVertex4f === 0) {
        __proc_glVertex4f = GetProcAddress(__ensureRealOpenGL32(), "glVertex4f");
        if (__proc_glVertex4f === null) {
            console.error("[opengl32_enh]   glVertex4f: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4f: resolved OK");
        }
    }
    (<(f32, f32, f32, f32) => void>__proc_glVertex4f)(x, y, z, w);
}

let __proc_glVertex4fv: Opaque = 0;

@dllname("glVertex4fv")
export function glVertex4fv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex4fv");
    if (__proc_glVertex4fv === 0) {
        __proc_glVertex4fv = GetProcAddress(__ensureRealOpenGL32(), "glVertex4fv");
        if (__proc_glVertex4fv === null) {
            console.error("[opengl32_enh]   glVertex4fv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4fv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex4fv)(v);
}

let __proc_glVertex4i: Opaque = 0;

@dllname("glVertex4i")
export function glVertex4i(x: i32, y: i32, z: i32, w: i32): void {
    console.log("[opengl32_enh] call glVertex4i");
    if (__proc_glVertex4i === 0) {
        __proc_glVertex4i = GetProcAddress(__ensureRealOpenGL32(), "glVertex4i");
        if (__proc_glVertex4i === null) {
            console.error("[opengl32_enh]   glVertex4i: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4i: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glVertex4i)(x, y, z, w);
}

let __proc_glVertex4iv: Opaque = 0;

@dllname("glVertex4iv")
export function glVertex4iv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex4iv");
    if (__proc_glVertex4iv === 0) {
        __proc_glVertex4iv = GetProcAddress(__ensureRealOpenGL32(), "glVertex4iv");
        if (__proc_glVertex4iv === null) {
            console.error("[opengl32_enh]   glVertex4iv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4iv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex4iv)(v);
}

let __proc_glVertex4s: Opaque = 0;

@dllname("glVertex4s")
export function glVertex4s(x: i16, y: i16, z: i16, w: i16): void {
    console.log("[opengl32_enh] call glVertex4s");
    if (__proc_glVertex4s === 0) {
        __proc_glVertex4s = GetProcAddress(__ensureRealOpenGL32(), "glVertex4s");
        if (__proc_glVertex4s === null) {
            console.error("[opengl32_enh]   glVertex4s: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4s: resolved OK");
        }
    }
    (<(i16, i16, i16, i16) => void>__proc_glVertex4s)(x, y, z, w);
}

let __proc_glVertex4sv: Opaque = 0;

@dllname("glVertex4sv")
export function glVertex4sv(v: Opaque): void {
    console.log("[opengl32_enh] call glVertex4sv");
    if (__proc_glVertex4sv === 0) {
        __proc_glVertex4sv = GetProcAddress(__ensureRealOpenGL32(), "glVertex4sv");
        if (__proc_glVertex4sv === null) {
            console.error("[opengl32_enh]   glVertex4sv: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertex4sv: resolved OK");
        }
    }
    (<(Opaque) => void>__proc_glVertex4sv)(v);
}

let __proc_glVertexPointer: Opaque = 0;

@dllname("glVertexPointer")
export function glVertexPointer(size: i32, type: u32, stride: i32, pointer: Opaque): void {
    console.log("[opengl32_enh] call glVertexPointer");
    if (__proc_glVertexPointer === 0) {
        __proc_glVertexPointer = GetProcAddress(__ensureRealOpenGL32(), "glVertexPointer");
        if (__proc_glVertexPointer === null) {
            console.error("[opengl32_enh]   glVertexPointer: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glVertexPointer: resolved OK");
        }
    }
    (<(i32, u32, i32, Opaque) => void>__proc_glVertexPointer)(size, type, stride, pointer);
}

let __proc_glViewport: Opaque = 0;

@dllname("glViewport")
export function glViewport(x: i32, y: i32, width: i32, height: i32): void {
    console.log("[opengl32_enh] call glViewport");
    if (__proc_glViewport === 0) {
        __proc_glViewport = GetProcAddress(__ensureRealOpenGL32(), "glViewport");
        if (__proc_glViewport === null) {
            console.error("[opengl32_enh]   glViewport: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   glViewport: resolved OK");
        }
    }
    (<(i32, i32, i32, i32) => void>__proc_glViewport)(x, y, width, height);
}

let __proc_wglCopyContext: Opaque = 0;

@dllname("wglCopyContext")
export function wglCopyContext(p0: Opaque, p1: Opaque, p2: u32): i32 {
    console.log("[opengl32_enh] call wglCopyContext");
    if (__proc_wglCopyContext === 0) {
        __proc_wglCopyContext = GetProcAddress(__ensureRealOpenGL32(), "wglCopyContext");
        if (__proc_wglCopyContext === null) {
            console.error("[opengl32_enh]   wglCopyContext: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglCopyContext: resolved OK");
        }
    }
    return (<(Opaque, Opaque, u32) => i32>__proc_wglCopyContext)(p0, p1, p2);
}

let __proc_wglCreateContext: Opaque = 0;

@dllname("wglCreateContext")
export function wglCreateContext(p0: Opaque): Opaque {
    console.log("[opengl32_enh] call wglCreateContext");
    if (__proc_wglCreateContext === 0) {
        __proc_wglCreateContext = GetProcAddress(__ensureRealOpenGL32(), "wglCreateContext");
        if (__proc_wglCreateContext === null) {
            console.error("[opengl32_enh]   wglCreateContext: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglCreateContext: resolved OK");
        }
    }
    return (<(Opaque) => Opaque>__proc_wglCreateContext)(p0);
}

let __proc_wglCreateLayerContext: Opaque = 0;

@dllname("wglCreateLayerContext")
export function wglCreateLayerContext(p0: Opaque, p1: i32): Opaque {
    console.log("[opengl32_enh] call wglCreateLayerContext");
    if (__proc_wglCreateLayerContext === 0) {
        __proc_wglCreateLayerContext = GetProcAddress(__ensureRealOpenGL32(), "wglCreateLayerContext");
        if (__proc_wglCreateLayerContext === null) {
            console.error("[opengl32_enh]   wglCreateLayerContext: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglCreateLayerContext: resolved OK");
        }
    }
    return (<(Opaque, i32) => Opaque>__proc_wglCreateLayerContext)(p0, p1);
}

let __proc_wglDeleteContext: Opaque = 0;

@dllname("wglDeleteContext")
export function wglDeleteContext(p0: Opaque): i32 {
    console.log("[opengl32_enh] call wglDeleteContext");
    if (__proc_wglDeleteContext === 0) {
        __proc_wglDeleteContext = GetProcAddress(__ensureRealOpenGL32(), "wglDeleteContext");
        if (__proc_wglDeleteContext === null) {
            console.error("[opengl32_enh]   wglDeleteContext: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglDeleteContext: resolved OK");
        }
    }
    return (<(Opaque) => i32>__proc_wglDeleteContext)(p0);
}

let __proc_wglGetCurrentContext: Opaque = 0;

@dllname("wglGetCurrentContext")
export function wglGetCurrentContext(): Opaque {
    console.log("[opengl32_enh] call wglGetCurrentContext");
    if (__proc_wglGetCurrentContext === 0) {
        __proc_wglGetCurrentContext = GetProcAddress(__ensureRealOpenGL32(), "wglGetCurrentContext");
        if (__proc_wglGetCurrentContext === null) {
            console.error("[opengl32_enh]   wglGetCurrentContext: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglGetCurrentContext: resolved OK");
        }
    }
    return (<() => Opaque>__proc_wglGetCurrentContext)();
}

let __proc_wglGetCurrentDC: Opaque = 0;

@dllname("wglGetCurrentDC")
export function wglGetCurrentDC(): Opaque {
    console.log("[opengl32_enh] call wglGetCurrentDC");
    if (__proc_wglGetCurrentDC === 0) {
        __proc_wglGetCurrentDC = GetProcAddress(__ensureRealOpenGL32(), "wglGetCurrentDC");
        if (__proc_wglGetCurrentDC === null) {
            console.error("[opengl32_enh]   wglGetCurrentDC: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglGetCurrentDC: resolved OK");
        }
    }
    return (<() => Opaque>__proc_wglGetCurrentDC)();
}

let __proc_wglGetProcAddress: Opaque = 0;

@dllname("wglGetProcAddress")
export function wglGetProcAddress(p0: Opaque): Opaque {
    console.log("[opengl32_enh] call wglGetProcAddress");
    if (__proc_wglGetProcAddress === 0) {
        __proc_wglGetProcAddress = GetProcAddress(__ensureRealOpenGL32(), "wglGetProcAddress");
        if (__proc_wglGetProcAddress === null) {
            console.error("[opengl32_enh]   wglGetProcAddress: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglGetProcAddress: resolved OK");
        }
    }
    return (<(Opaque) => Opaque>__proc_wglGetProcAddress)(p0);
}

let __proc_wglMakeCurrent: Opaque = 0;

@dllname("wglMakeCurrent")
export function wglMakeCurrent(p0: Opaque, p1: Opaque): i32 {
    console.log("[opengl32_enh] call wglMakeCurrent");
    if (__proc_wglMakeCurrent === 0) {
        __proc_wglMakeCurrent = GetProcAddress(__ensureRealOpenGL32(), "wglMakeCurrent");
        if (__proc_wglMakeCurrent === null) {
            console.error("[opengl32_enh]   wglMakeCurrent: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglMakeCurrent: resolved OK");
        }
    }
    return (<(Opaque, Opaque) => i32>__proc_wglMakeCurrent)(p0, p1);
}

let __proc_wglShareLists: Opaque = 0;

@dllname("wglShareLists")
export function wglShareLists(p0: Opaque, p1: Opaque): i32 {
    console.log("[opengl32_enh] call wglShareLists");
    if (__proc_wglShareLists === 0) {
        __proc_wglShareLists = GetProcAddress(__ensureRealOpenGL32(), "wglShareLists");
        if (__proc_wglShareLists === null) {
            console.error("[opengl32_enh]   wglShareLists: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglShareLists: resolved OK");
        }
    }
    return (<(Opaque, Opaque) => i32>__proc_wglShareLists)(p0, p1);
}

let __proc_wglUseFontBitmapsA: Opaque = 0;

@dllname("wglUseFontBitmapsA")
export function wglUseFontBitmapsA(p0: Opaque, p1: u32, p2: u32, p3: u32): i32 {
    console.log("[opengl32_enh] call wglUseFontBitmapsA");
    if (__proc_wglUseFontBitmapsA === 0) {
        __proc_wglUseFontBitmapsA = GetProcAddress(__ensureRealOpenGL32(), "wglUseFontBitmapsA");
        if (__proc_wglUseFontBitmapsA === null) {
            console.error("[opengl32_enh]   wglUseFontBitmapsA: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglUseFontBitmapsA: resolved OK");
        }
    }
    return (<(Opaque, u32, u32, u32) => i32>__proc_wglUseFontBitmapsA)(p0, p1, p2, p3);
}

let __proc_wglUseFontBitmapsW: Opaque = 0;

@dllname("wglUseFontBitmapsW")
export function wglUseFontBitmapsW(p0: Opaque, p1: u32, p2: u32, p3: u32): i32 {
    console.log("[opengl32_enh] call wglUseFontBitmapsW");
    if (__proc_wglUseFontBitmapsW === 0) {
        __proc_wglUseFontBitmapsW = GetProcAddress(__ensureRealOpenGL32(), "wglUseFontBitmapsW");
        if (__proc_wglUseFontBitmapsW === null) {
            console.error("[opengl32_enh]   wglUseFontBitmapsW: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglUseFontBitmapsW: resolved OK");
        }
    }
    return (<(Opaque, u32, u32, u32) => i32>__proc_wglUseFontBitmapsW)(p0, p1, p2, p3);
}

let __proc_wglUseFontOutlinesA: Opaque = 0;

@dllname("wglUseFontOutlinesA")
export function wglUseFontOutlinesA(p0: Opaque, p1: u32, p2: u32, p3: u32, p4: f32, p5: f32, p6: i32, p7: Opaque): i32 {
    console.log("[opengl32_enh] call wglUseFontOutlinesA");
    if (__proc_wglUseFontOutlinesA === 0) {
        __proc_wglUseFontOutlinesA = GetProcAddress(__ensureRealOpenGL32(), "wglUseFontOutlinesA");
        if (__proc_wglUseFontOutlinesA === null) {
            console.error("[opengl32_enh]   wglUseFontOutlinesA: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglUseFontOutlinesA: resolved OK");
        }
    }
    return (<(Opaque, u32, u32, u32, f32, f32, i32, Opaque) => i32>__proc_wglUseFontOutlinesA)(p0, p1, p2, p3, p4, p5, p6, p7);
}

let __proc_wglUseFontOutlinesW: Opaque = 0;

@dllname("wglUseFontOutlinesW")
export function wglUseFontOutlinesW(p0: Opaque, p1: u32, p2: u32, p3: u32, p4: f32, p5: f32, p6: i32, p7: Opaque): i32 {
    console.log("[opengl32_enh] call wglUseFontOutlinesW");
    if (__proc_wglUseFontOutlinesW === 0) {
        __proc_wglUseFontOutlinesW = GetProcAddress(__ensureRealOpenGL32(), "wglUseFontOutlinesW");
        if (__proc_wglUseFontOutlinesW === null) {
            console.error("[opengl32_enh]   wglUseFontOutlinesW: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglUseFontOutlinesW: resolved OK");
        }
    }
    return (<(Opaque, u32, u32, u32, f32, f32, i32, Opaque) => i32>__proc_wglUseFontOutlinesW)(p0, p1, p2, p3, p4, p5, p6, p7);
}

let __proc_wglDescribeLayerPlane: Opaque = 0;

@dllname("wglDescribeLayerPlane")
export function wglDescribeLayerPlane(p0: Opaque, p1: i32, p2: i32, p3: u32, p4: Opaque): i32 {
    console.log("[opengl32_enh] call wglDescribeLayerPlane");
    if (__proc_wglDescribeLayerPlane === 0) {
        __proc_wglDescribeLayerPlane = GetProcAddress(__ensureRealOpenGL32(), "wglDescribeLayerPlane");
        if (__proc_wglDescribeLayerPlane === null) {
            console.error("[opengl32_enh]   wglDescribeLayerPlane: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglDescribeLayerPlane: resolved OK");
        }
    }
    return (<(Opaque, i32, i32, u32, Opaque) => i32>__proc_wglDescribeLayerPlane)(p0, p1, p2, p3, p4);
}

let __proc_wglSetLayerPaletteEntries: Opaque = 0;

@dllname("wglSetLayerPaletteEntries")
export function wglSetLayerPaletteEntries(p0: Opaque, p1: i32, p2: i32, p3: i32, p4: Opaque): i32 {
    console.log("[opengl32_enh] call wglSetLayerPaletteEntries");
    if (__proc_wglSetLayerPaletteEntries === 0) {
        __proc_wglSetLayerPaletteEntries = GetProcAddress(__ensureRealOpenGL32(), "wglSetLayerPaletteEntries");
        if (__proc_wglSetLayerPaletteEntries === null) {
            console.error("[opengl32_enh]   wglSetLayerPaletteEntries: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglSetLayerPaletteEntries: resolved OK");
        }
    }
    return (<(Opaque, i32, i32, i32, Opaque) => i32>__proc_wglSetLayerPaletteEntries)(p0, p1, p2, p3, p4);
}

let __proc_wglGetLayerPaletteEntries: Opaque = 0;

@dllname("wglGetLayerPaletteEntries")
export function wglGetLayerPaletteEntries(p0: Opaque, p1: i32, p2: i32, p3: i32, p4: Opaque): i32 {
    console.log("[opengl32_enh] call wglGetLayerPaletteEntries");
    if (__proc_wglGetLayerPaletteEntries === 0) {
        __proc_wglGetLayerPaletteEntries = GetProcAddress(__ensureRealOpenGL32(), "wglGetLayerPaletteEntries");
        if (__proc_wglGetLayerPaletteEntries === null) {
            console.error("[opengl32_enh]   wglGetLayerPaletteEntries: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglGetLayerPaletteEntries: resolved OK");
        }
    }
    return (<(Opaque, i32, i32, i32, Opaque) => i32>__proc_wglGetLayerPaletteEntries)(p0, p1, p2, p3, p4);
}

let __proc_wglRealizeLayerPalette: Opaque = 0;

@dllname("wglRealizeLayerPalette")
export function wglRealizeLayerPalette(p0: Opaque, p1: i32, p2: i32): i32 {
    console.log("[opengl32_enh] call wglRealizeLayerPalette");
    if (__proc_wglRealizeLayerPalette === 0) {
        __proc_wglRealizeLayerPalette = GetProcAddress(__ensureRealOpenGL32(), "wglRealizeLayerPalette");
        if (__proc_wglRealizeLayerPalette === null) {
            console.error("[opengl32_enh]   wglRealizeLayerPalette: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglRealizeLayerPalette: resolved OK");
        }
    }
    return (<(Opaque, i32, i32) => i32>__proc_wglRealizeLayerPalette)(p0, p1, p2);
}

let __proc_wglSwapLayerBuffers: Opaque = 0;

@dllname("wglSwapLayerBuffers")
export function wglSwapLayerBuffers(p0: Opaque, p1: u32): i32 {
    console.log("[opengl32_enh] call wglSwapLayerBuffers");
    if (__proc_wglSwapLayerBuffers === 0) {
        __proc_wglSwapLayerBuffers = GetProcAddress(__ensureRealOpenGL32(), "wglSwapLayerBuffers");
        if (__proc_wglSwapLayerBuffers === null) {
            console.error("[opengl32_enh]   wglSwapLayerBuffers: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglSwapLayerBuffers: resolved OK");
        }
    }
    return (<(Opaque, u32) => i32>__proc_wglSwapLayerBuffers)(p0, p1);
}

let __proc_wglSwapMultipleBuffers: Opaque = 0;

@dllname("wglSwapMultipleBuffers")
export function wglSwapMultipleBuffers(p0: u32, p1: Opaque): u32 {
    console.log("[opengl32_enh] call wglSwapMultipleBuffers");
    if (__proc_wglSwapMultipleBuffers === 0) {
        __proc_wglSwapMultipleBuffers = GetProcAddress(__ensureRealOpenGL32(), "wglSwapMultipleBuffers");
        if (__proc_wglSwapMultipleBuffers === null) {
            console.error("[opengl32_enh]   wglSwapMultipleBuffers: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglSwapMultipleBuffers: resolved OK");
        }
    }
    return (<(u32, Opaque) => u32>__proc_wglSwapMultipleBuffers)(p0, p1);
}

let __proc_wglChoosePixelFormat: Opaque = 0;

@dllname("wglChoosePixelFormat")
export function wglChoosePixelFormat(p0: Opaque, p1: Opaque): i32 {
    console.log("[opengl32_enh] call wglChoosePixelFormat");
    if (__proc_wglChoosePixelFormat === 0) {
        __proc_wglChoosePixelFormat = GetProcAddress(__ensureRealOpenGL32(), "wglChoosePixelFormat");
        if (__proc_wglChoosePixelFormat === null) {
            console.error("[opengl32_enh]   wglChoosePixelFormat: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglChoosePixelFormat: resolved OK");
        }
    }
    return (<(Opaque, Opaque) => i32>__proc_wglChoosePixelFormat)(p0, p1);
}

let __proc_wglDescribePixelFormat: Opaque = 0;

@dllname("wglDescribePixelFormat")
export function wglDescribePixelFormat(p0: Opaque, p1: i32, p2: u32, p3: Opaque): i32 {
    console.log("[opengl32_enh] call wglDescribePixelFormat");
    if (__proc_wglDescribePixelFormat === 0) {
        __proc_wglDescribePixelFormat = GetProcAddress(__ensureRealOpenGL32(), "wglDescribePixelFormat");
        if (__proc_wglDescribePixelFormat === null) {
            console.error("[opengl32_enh]   wglDescribePixelFormat: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglDescribePixelFormat: resolved OK");
        }
    }
    return (<(Opaque, i32, u32, Opaque) => i32>__proc_wglDescribePixelFormat)(p0, p1, p2, p3);
}

let __proc_wglGetPixelFormat: Opaque = 0;

@dllname("wglGetPixelFormat")
export function wglGetPixelFormat(p0: Opaque): i32 {
    console.log("[opengl32_enh] call wglGetPixelFormat");
    if (__proc_wglGetPixelFormat === 0) {
        __proc_wglGetPixelFormat = GetProcAddress(__ensureRealOpenGL32(), "wglGetPixelFormat");
        if (__proc_wglGetPixelFormat === null) {
            console.error("[opengl32_enh]   wglGetPixelFormat: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglGetPixelFormat: resolved OK");
        }
    }
    return (<(Opaque) => i32>__proc_wglGetPixelFormat)(p0);
}

let __proc_wglSetPixelFormat: Opaque = 0;

@dllname("wglSetPixelFormat")
export function wglSetPixelFormat(p0: Opaque, p1: i32, p2: Opaque): i32 {
    console.log("[opengl32_enh] call wglSetPixelFormat");
    if (__proc_wglSetPixelFormat === 0) {
        __proc_wglSetPixelFormat = GetProcAddress(__ensureRealOpenGL32(), "wglSetPixelFormat");
        if (__proc_wglSetPixelFormat === null) {
            console.error("[opengl32_enh]   wglSetPixelFormat: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglSetPixelFormat: resolved OK");
        }
    }
    return (<(Opaque, i32, Opaque) => i32>__proc_wglSetPixelFormat)(p0, p1, p2);
}

let __proc_wglSwapBuffers: Opaque = 0;

@dllname("wglSwapBuffers")
export function wglSwapBuffers(p0: Opaque): i32 {
    console.log("[opengl32_enh] call wglSwapBuffers");
    if (__proc_wglSwapBuffers === 0) {
        __proc_wglSwapBuffers = GetProcAddress(__ensureRealOpenGL32(), "wglSwapBuffers");
        if (__proc_wglSwapBuffers === null) {
            console.error("[opengl32_enh]   wglSwapBuffers: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglSwapBuffers: resolved OK");
        }
    }
    return (<(Opaque) => i32>__proc_wglSwapBuffers)(p0);
}

let __proc_wglGetDefaultProcAddress: Opaque = 0;

@dllname("wglGetDefaultProcAddress")
export function wglGetDefaultProcAddress(p0: Opaque): Opaque {
    console.log("[opengl32_enh] call wglGetDefaultProcAddress");
    if (__proc_wglGetDefaultProcAddress === 0) {
        __proc_wglGetDefaultProcAddress = GetProcAddress(__ensureRealOpenGL32(), "wglGetDefaultProcAddress");
        if (__proc_wglGetDefaultProcAddress === null) {
            console.error("[opengl32_enh]   wglGetDefaultProcAddress: FAILED to resolve, GetLastError=" + (<number>GetLastError()).toString());
        } else {
            console.log("[opengl32_enh]   wglGetDefaultProcAddress: resolved OK");
        }
    }
    return (<(Opaque) => Opaque>__proc_wglGetDefaultProcAddress)(p0);
}
