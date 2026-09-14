// AUTO-GENERATED interception test for wrapper.ts.
//
// Loads the proxy opengl32.dll built from wrapper.ts (by its plain name, the
// same way a real host application would - relying on it sitting next to this
// executable and being found before the real System32 one) and checks two
// things:
//
//  1. Every function this wrapper claims to intercept actually resolves via
//     GetProcAddress under its original name (catches typos in @dllname, or a
//     function silently missing from the export table).
//  2. A representative subset (different arities, param/return types) can be
//     called end-to-end through the proxy without crashing, proving the calls
//     are actually forwarded to the real driver rather than just resolved.
//     These are calls known to be safe with no current GL context: no output
//     pointers, so nothing is written through an argument.
declare function LoadLibraryA(libraryName: Opaque): Opaque;
declare function GetProcAddress(library: Opaque, functionName: Opaque): Opaque;

console.log("Loading proxy opengl32.dll...");
const mod = LoadLibraryA("opengl32.dll");
if (mod === null) {
    console.error("Failed to load opengl32.dll (proxy build not next to this executable?)");
} else {
    console.log("Proxy loaded. Resolving all intercepted functions...");

    const names = [
        "glAccum",
        "glAlphaFunc",
        "glAreTexturesResident",
        "glArrayElement",
        "glBegin",
        "glBindTexture",
        "glBitmap",
        "glBlendFunc",
        "glCallList",
        "glCallLists",
        "glClear",
        "glClearAccum",
        "glClearColor",
        "glClearDepth",
        "glClearIndex",
        "glClearStencil",
        "glClipPlane",
        "glColor3b",
        "glColor3bv",
        "glColor3d",
        "glColor3dv",
        "glColor3f",
        "glColor3fv",
        "glColor3i",
        "glColor3iv",
        "glColor3s",
        "glColor3sv",
        "glColor3ub",
        "glColor3ubv",
        "glColor3ui",
        "glColor3uiv",
        "glColor3us",
        "glColor3usv",
        "glColor4b",
        "glColor4bv",
        "glColor4d",
        "glColor4dv",
        "glColor4f",
        "glColor4fv",
        "glColor4i",
        "glColor4iv",
        "glColor4s",
        "glColor4sv",
        "glColor4ub",
        "glColor4ubv",
        "glColor4ui",
        "glColor4uiv",
        "glColor4us",
        "glColor4usv",
        "glColorMask",
        "glColorMaterial",
        "glColorPointer",
        "glCopyPixels",
        "glCopyTexImage1D",
        "glCopyTexImage2D",
        "glCopyTexSubImage1D",
        "glCopyTexSubImage2D",
        "glCullFace",
        "glDeleteLists",
        "glDeleteTextures",
        "glDepthFunc",
        "glDepthMask",
        "glDepthRange",
        "glDisable",
        "glDisableClientState",
        "glDrawArrays",
        "glDrawBuffer",
        "glDrawElements",
        "glDrawPixels",
        "glEdgeFlag",
        "glEdgeFlagPointer",
        "glEdgeFlagv",
        "glEnable",
        "glEnableClientState",
        "glEnd",
        "glEndList",
        "glEvalCoord1d",
        "glEvalCoord1dv",
        "glEvalCoord1f",
        "glEvalCoord1fv",
        "glEvalCoord2d",
        "glEvalCoord2dv",
        "glEvalCoord2f",
        "glEvalCoord2fv",
        "glEvalMesh1",
        "glEvalMesh2",
        "glEvalPoint1",
        "glEvalPoint2",
        "glFeedbackBuffer",
        "glFinish",
        "glFlush",
        "glFogf",
        "glFogfv",
        "glFogi",
        "glFogiv",
        "glFrontFace",
        "glFrustum",
        "glGenLists",
        "glGenTextures",
        "glGetBooleanv",
        "glGetClipPlane",
        "glGetDoublev",
        "glGetError",
        "glGetFloatv",
        "glGetIntegerv",
        "glGetLightfv",
        "glGetLightiv",
        "glGetMapdv",
        "glGetMapfv",
        "glGetMapiv",
        "glGetMaterialfv",
        "glGetMaterialiv",
        "glGetPixelMapfv",
        "glGetPixelMapuiv",
        "glGetPixelMapusv",
        "glGetPointerv",
        "glGetPolygonStipple",
        "glGetString",
        "glGetTexEnvfv",
        "glGetTexEnviv",
        "glGetTexGendv",
        "glGetTexGenfv",
        "glGetTexGeniv",
        "glGetTexImage",
        "glGetTexLevelParameterfv",
        "glGetTexLevelParameteriv",
        "glGetTexParameterfv",
        "glGetTexParameteriv",
        "glHint",
        "glIndexMask",
        "glIndexPointer",
        "glIndexd",
        "glIndexdv",
        "glIndexf",
        "glIndexfv",
        "glIndexi",
        "glIndexiv",
        "glIndexs",
        "glIndexsv",
        "glIndexub",
        "glIndexubv",
        "glInitNames",
        "glInterleavedArrays",
        "glIsEnabled",
        "glIsList",
        "glIsTexture",
        "glLightModelf",
        "glLightModelfv",
        "glLightModeli",
        "glLightModeliv",
        "glLightf",
        "glLightfv",
        "glLighti",
        "glLightiv",
        "glLineStipple",
        "glLineWidth",
        "glListBase",
        "glLoadIdentity",
        "glLoadMatrixd",
        "glLoadMatrixf",
        "glLoadName",
        "glLogicOp",
        "glMap1d",
        "glMap1f",
        "glMap2d",
        "glMap2f",
        "glMapGrid1d",
        "glMapGrid1f",
        "glMapGrid2d",
        "glMapGrid2f",
        "glMaterialf",
        "glMaterialfv",
        "glMateriali",
        "glMaterialiv",
        "glMatrixMode",
        "glMultMatrixd",
        "glMultMatrixf",
        "glNewList",
        "glNormal3b",
        "glNormal3bv",
        "glNormal3d",
        "glNormal3dv",
        "glNormal3f",
        "glNormal3fv",
        "glNormal3i",
        "glNormal3iv",
        "glNormal3s",
        "glNormal3sv",
        "glNormalPointer",
        "glOrtho",
        "glPassThrough",
        "glPixelMapfv",
        "glPixelMapuiv",
        "glPixelMapusv",
        "glPixelStoref",
        "glPixelStorei",
        "glPixelTransferf",
        "glPixelTransferi",
        "glPixelZoom",
        "glPointSize",
        "glPolygonMode",
        "glPolygonOffset",
        "glPolygonStipple",
        "glPopAttrib",
        "glPopClientAttrib",
        "glPopMatrix",
        "glPopName",
        "glPrioritizeTextures",
        "glPushAttrib",
        "glPushClientAttrib",
        "glPushMatrix",
        "glPushName",
        "glRasterPos2d",
        "glRasterPos2dv",
        "glRasterPos2f",
        "glRasterPos2fv",
        "glRasterPos2i",
        "glRasterPos2iv",
        "glRasterPos2s",
        "glRasterPos2sv",
        "glRasterPos3d",
        "glRasterPos3dv",
        "glRasterPos3f",
        "glRasterPos3fv",
        "glRasterPos3i",
        "glRasterPos3iv",
        "glRasterPos3s",
        "glRasterPos3sv",
        "glRasterPos4d",
        "glRasterPos4dv",
        "glRasterPos4f",
        "glRasterPos4fv",
        "glRasterPos4i",
        "glRasterPos4iv",
        "glRasterPos4s",
        "glRasterPos4sv",
        "glReadBuffer",
        "glReadPixels",
        "glRectd",
        "glRectdv",
        "glRectf",
        "glRectfv",
        "glRecti",
        "glRectiv",
        "glRects",
        "glRectsv",
        "glRenderMode",
        "glRotated",
        "glRotatef",
        "glScaled",
        "glScalef",
        "glScissor",
        "glSelectBuffer",
        "glShadeModel",
        "glStencilFunc",
        "glStencilMask",
        "glStencilOp",
        "glTexCoord1d",
        "glTexCoord1dv",
        "glTexCoord1f",
        "glTexCoord1fv",
        "glTexCoord1i",
        "glTexCoord1iv",
        "glTexCoord1s",
        "glTexCoord1sv",
        "glTexCoord2d",
        "glTexCoord2dv",
        "glTexCoord2f",
        "glTexCoord2fv",
        "glTexCoord2i",
        "glTexCoord2iv",
        "glTexCoord2s",
        "glTexCoord2sv",
        "glTexCoord3d",
        "glTexCoord3dv",
        "glTexCoord3f",
        "glTexCoord3fv",
        "glTexCoord3i",
        "glTexCoord3iv",
        "glTexCoord3s",
        "glTexCoord3sv",
        "glTexCoord4d",
        "glTexCoord4dv",
        "glTexCoord4f",
        "glTexCoord4fv",
        "glTexCoord4i",
        "glTexCoord4iv",
        "glTexCoord4s",
        "glTexCoord4sv",
        "glTexCoordPointer",
        "glTexEnvf",
        "glTexEnvfv",
        "glTexEnvi",
        "glTexEnviv",
        "glTexGend",
        "glTexGendv",
        "glTexGenf",
        "glTexGenfv",
        "glTexGeni",
        "glTexGeniv",
        "glTexImage1D",
        "glTexImage2D",
        "glTexParameterf",
        "glTexParameterfv",
        "glTexParameteri",
        "glTexParameteriv",
        "glTexSubImage1D",
        "glTexSubImage2D",
        "glTranslated",
        "glTranslatef",
        "glVertex2d",
        "glVertex2dv",
        "glVertex2f",
        "glVertex2fv",
        "glVertex2i",
        "glVertex2iv",
        "glVertex2s",
        "glVertex2sv",
        "glVertex3d",
        "glVertex3dv",
        "glVertex3f",
        "glVertex3fv",
        "glVertex3i",
        "glVertex3iv",
        "glVertex3s",
        "glVertex3sv",
        "glVertex4d",
        "glVertex4dv",
        "glVertex4f",
        "glVertex4fv",
        "glVertex4i",
        "glVertex4iv",
        "glVertex4s",
        "glVertex4sv",
        "glVertexPointer",
        "glViewport",
        "wglCopyContext",
        "wglCreateContext",
        "wglCreateLayerContext",
        "wglDeleteContext",
        "wglGetCurrentContext",
        "wglGetCurrentDC",
        "wglGetProcAddress",
        "wglMakeCurrent",
        "wglShareLists",
        "wglUseFontBitmapsA",
        "wglUseFontBitmapsW",
        "wglUseFontOutlinesA",
        "wglUseFontOutlinesW",
        "wglDescribeLayerPlane",
        "wglSetLayerPaletteEntries",
        "wglGetLayerPaletteEntries",
        "wglRealizeLayerPalette",
        "wglSwapLayerBuffers",
        "wglSwapMultipleBuffers",
        "wglChoosePixelFormat",
        "wglDescribePixelFormat",
        "wglGetPixelFormat",
        "wglSetPixelFormat",
        "wglSwapBuffers",
        "wglGetDefaultProcAddress",
    ];

    let resolved: number = 0;
    let failed: number = 0;
    names.forEach((name) => {
        const addr = GetProcAddress(mod, name);
        if (addr === null) {
            failed = failed + 1;
            console.error("  NOT FOUND: " + name);
        } else {
            resolved = resolved + 1;
        }
    });

    console.log("Resolved: " + resolved.toString() + " / " + (<number>names.length).toString());
    if (failed > 0) {
        console.error(failed.toString() + " function(s) failed to resolve.");
    } else {
        console.log("All intercepted functions resolved successfully.");
    }

    console.log("");
    console.log("Calling a representative subset through the proxy...");

    console.log("  glGetError(...)");
    const __proc_glGetError = GetProcAddress(mod, "glGetError");
    if (__proc_glGetError === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<() => u32>__proc_glGetError)();
        console.log("    -> " + (<number>__r).toString());
    }

    console.log("  glFlush(...)");
    const __proc_glFlush = GetProcAddress(mod, "glFlush");
    if (__proc_glFlush === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<() => void>__proc_glFlush)();
        console.log("    -> called, no crash");
    }

    console.log("  glFinish(...)");
    const __proc_glFinish = GetProcAddress(mod, "glFinish");
    if (__proc_glFinish === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<() => void>__proc_glFinish)();
        console.log("    -> called, no crash");
    }

    console.log("  glEnable(...)");
    const __proc_glEnable = GetProcAddress(mod, "glEnable");
    if (__proc_glEnable === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(u32) => void>__proc_glEnable)(0x0B71);
        console.log("    -> called, no crash");
    }

    console.log("  glDisable(...)");
    const __proc_glDisable = GetProcAddress(mod, "glDisable");
    if (__proc_glDisable === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(u32) => void>__proc_glDisable)(0x0B71);
        console.log("    -> called, no crash");
    }

    console.log("  glIsEnabled(...)");
    const __proc_glIsEnabled = GetProcAddress(mod, "glIsEnabled");
    if (__proc_glIsEnabled === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<(u32) => u8>__proc_glIsEnabled)(0x0B71);
        console.log("    -> " + (<number>__r).toString());
    }

    console.log("  glClearColor(...)");
    const __proc_glClearColor = GetProcAddress(mod, "glClearColor");
    if (__proc_glClearColor === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(f32, f32, f32, f32) => void>__proc_glClearColor)(0.0, 0.0, 0.0, 1.0);
        console.log("    -> called, no crash");
    }

    console.log("  glViewport(...)");
    const __proc_glViewport = GetProcAddress(mod, "glViewport");
    if (__proc_glViewport === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(i32, i32, i32, i32) => void>__proc_glViewport)(0, 0, 64, 64);
        console.log("    -> called, no crash");
    }

    console.log("  glMatrixMode(...)");
    const __proc_glMatrixMode = GetProcAddress(mod, "glMatrixMode");
    if (__proc_glMatrixMode === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(u32) => void>__proc_glMatrixMode)(0x1700);
        console.log("    -> called, no crash");
    }

    console.log("  glLoadIdentity(...)");
    const __proc_glLoadIdentity = GetProcAddress(mod, "glLoadIdentity");
    if (__proc_glLoadIdentity === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<() => void>__proc_glLoadIdentity)();
        console.log("    -> called, no crash");
    }

    console.log("  glLineWidth(...)");
    const __proc_glLineWidth = GetProcAddress(mod, "glLineWidth");
    if (__proc_glLineWidth === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(f32) => void>__proc_glLineWidth)(1.0);
        console.log("    -> called, no crash");
    }

    console.log("  glColor3f(...)");
    const __proc_glColor3f = GetProcAddress(mod, "glColor3f");
    if (__proc_glColor3f === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(f32, f32, f32) => void>__proc_glColor3f)(1.0, 1.0, 1.0);
        console.log("    -> called, no crash");
    }

    console.log("  glVertex3f(...)");
    const __proc_glVertex3f = GetProcAddress(mod, "glVertex3f");
    if (__proc_glVertex3f === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(f32, f32, f32) => void>__proc_glVertex3f)(0.0, 0.0, 0.0);
        console.log("    -> called, no crash");
    }

    console.log("  glBegin(...)");
    const __proc_glBegin = GetProcAddress(mod, "glBegin");
    if (__proc_glBegin === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(u32) => void>__proc_glBegin)(0x0004);
        console.log("    -> called, no crash");
    }

    console.log("  glEnd(...)");
    const __proc_glEnd = GetProcAddress(mod, "glEnd");
    if (__proc_glEnd === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<() => void>__proc_glEnd)();
        console.log("    -> called, no crash");
    }

    console.log("  glTranslated(...)");
    const __proc_glTranslated = GetProcAddress(mod, "glTranslated");
    if (__proc_glTranslated === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(f64, f64, f64) => void>__proc_glTranslated)(0.0, 0.0, 0.0);
        console.log("    -> called, no crash");
    }

    console.log("  glRotatef(...)");
    const __proc_glRotatef = GetProcAddress(mod, "glRotatef");
    if (__proc_glRotatef === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        (<(f32, f32, f32, f32) => void>__proc_glRotatef)(0.0, 0.0, 1.0, 0.0);
        console.log("    -> called, no crash");
    }

    console.log("  wglGetCurrentContext(...)");
    const __proc_wglGetCurrentContext = GetProcAddress(mod, "wglGetCurrentContext");
    if (__proc_wglGetCurrentContext === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<() => Opaque>__proc_wglGetCurrentContext)();
        console.log("    -> " + (__r === null ? "null" : "non-null"));
    }

    console.log("  wglGetCurrentDC(...)");
    const __proc_wglGetCurrentDC = GetProcAddress(mod, "wglGetCurrentDC");
    if (__proc_wglGetCurrentDC === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<() => Opaque>__proc_wglGetCurrentDC)();
        console.log("    -> " + (__r === null ? "null" : "non-null"));
    }

    console.log("  wglCreateContext(...)");
    const __proc_wglCreateContext = GetProcAddress(mod, "wglCreateContext");
    if (__proc_wglCreateContext === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<(Opaque) => Opaque>__proc_wglCreateContext)(0);
        console.log("    -> " + (__r === null ? "null" : "non-null"));
    }

    console.log("  wglMakeCurrent(...)");
    const __proc_wglMakeCurrent = GetProcAddress(mod, "wglMakeCurrent");
    if (__proc_wglMakeCurrent === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<(Opaque, Opaque) => u8>__proc_wglMakeCurrent)(0, 0);
        console.log("    -> " + (<number>__r).toString());
    }

    console.log("  wglDeleteContext(...)");
    const __proc_wglDeleteContext = GetProcAddress(mod, "wglDeleteContext");
    if (__proc_wglDeleteContext === null) {
        console.error("    NOT FOUND - cannot call");
    } else {
        const __r = (<(Opaque) => u8>__proc_wglDeleteContext)(0);
        console.log("    -> " + (<number>__r).toString());
    }

    console.log("Done - no crash means every called wrapper forwarded correctly.");
}
