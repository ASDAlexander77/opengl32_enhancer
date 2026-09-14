declare function LoadLibraryA(libraryName: Opaque): Opaque;
declare function FreeLibrary(library: Opaque): boolean;
declare function GetProcAddress(library: Opaque, functionName: Opaque): Opaque;
declare function GetLastError(): number;

// OpenGL functions: Window
declare function wglCreateContext(moduleName: Opaque): Opaque;

// main entry
console.log("Hello from TSLANG!");

const module = LoadLibraryA("C:\\Windows\\System32\\opengl32.dll");
if (module === null) {
    console.error("Failed to load library. C:\\Windows\\System32\\opengl32.dll");
} else {
    console.log("Library loaded successfully.");

    const wglCreateContextFunc = GetProcAddress(module, "wglCreateContext");
    if (wglCreateContextFunc === null) {
        console.error("Failed to get function address for wglCreateContext.");
    }
    else
    {
        console.log("calling wglCreateContextFunc...");
        let hdc: Opaque = 0; // Placeholder for device context handle
        const hglrc = (<(p: Opaque) => Opaque>wglCreateContextFunc) (hdc); 
        if (hglrc === null) {
            console.error("Failed to create OpenGL context.");
        }
    }

    const result = FreeLibrary(module);
    if (!result) {
        console.error("Failed to free library.");
    }
}

console.log("Done.");