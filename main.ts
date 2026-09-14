declare function LoadLibraryA(libraryName: Opaque): Opaque;
declare function FreeLibrary(library: Opaque): boolean;
declare function GetProcAddress(library: Opaque, functionName: Opaque): Opaque;
declare function GetLastError(): number;

// OpenGL functions: Window
declare function wglCreateContext(moduleName: Opaque): Opaque;
declare function wglMakeCurrent(moduleName: Opaque, context: Opaque): boolean;
declare function wglDeleteContext(context: Opaque): boolean;

// main entry
console.log("Hello from TSLANG!");

const module = LoadLibraryA("C:\\Windows\\System32\\opengl32.dll");
if (module === null) {
    console.error("Failed to load library. C:\\Windows\\System32\\opengl32.dll");
} else {
    console.log("Library loaded successfully.");

    // const wglCreateContextFunc = GetProcAddress(module, "wglCreateContext");
    // if (wglCreateContextFunc === null) {
    //     console.error("Failed to get function address for wglCreateContext.");
    // }
    // else
    // {
    //     console.log("calling wglCreateContextFunc...");
    //     let hdc: Opaque = 0; // Placeholder for device context handle
    //     const hglrc = (<(p: Opaque) => Opaque>wglCreateContextFunc) (hdc); 
    //     if (hglrc === null) {
    //         console.error("Failed to create OpenGL context.");
    //     }
    // }

    const wglMakeCurrentFunc = GetProcAddress(module, "wglMakeCurrent");
    if (wglMakeCurrentFunc === null) {
        console.error("Failed to get function address for wglMakeCurrent.");
    }
    else {
        console.log("calling wglMakeCurrentFunc...");
        const hdc: Opaque = 0; // Placeholder for device context handle
        const hglrc: Opaque = 0; // Placeholder for OpenGL rendering context handle
        const result = (<(hdc: Opaque, hglrc: Opaque) => boolean>wglMakeCurrentFunc)(hdc, hglrc);
        if (!result) {
            console.error("Failed to make OpenGL context current.");
        }

        const wglDeleteContextFunc = GetProcAddress(module, "wglDeleteContext");
        if (wglDeleteContextFunc === null) {
            console.error("Failed to get function address for wglDeleteContext.");
        }
        else {
            console.log("calling wglDeleteContextFunc...");
            const deleteResult = (<(hglrc: Opaque) => boolean>wglDeleteContextFunc)(hglrc);
            if (!deleteResult) {
                console.error("Failed to delete OpenGL context.");
            }
        }
    }

    const result = FreeLibrary(module);
    if (!result) {
        console.error("Failed to free library.");
    }
}

console.log("Done.");