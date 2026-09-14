// Minimal 32-bit load test for opengl32_enh32.dll: loads it, resolves a couple of exports,
// calls them, and reports. Proves the 32-bit proxy actually loads into a 32-bit process
// and forwards to the real (SysWOW64) opengl32.dll without crashing.
#include <cstdio>

extern "C" {
    __declspec(dllimport) void* __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
    __declspec(dllimport) unsigned long __stdcall GetLastError(void);
}

typedef unsigned int (__stdcall *PFN_glGetError)(void);
typedef void* (__stdcall *PFN_wglGetCurrentContext)(void);

int main() {
    printf("Loading opengl32_enh32.dll...\n");
    void* mod = LoadLibraryA("opengl32_enh32.dll");
    if (!mod) {
        printf("FAILED to load opengl32_enh32.dll, GetLastError=%lu\n", GetLastError());
        return 1;
    }
    printf("opengl32_enh32.dll loaded OK\n");

    PFN_glGetError pGetError = (PFN_glGetError)GetProcAddress(mod, "glGetError");
    if (!pGetError) {
        printf("glGetError not found in opengl32_enh32.dll\n");
        return 1;
    }
    unsigned int err = pGetError();
    printf("glGetError() via opengl32_enh32.dll -> %u\n", err);

    PFN_wglGetCurrentContext pCtx = (PFN_wglGetCurrentContext)GetProcAddress(mod, "wglGetCurrentContext");
    if (!pCtx) {
        printf("wglGetCurrentContext not found in opengl32_enh32.dll\n");
        return 1;
    }
    void* ctx = pCtx();
    printf("wglGetCurrentContext() via opengl32_enh32.dll -> %s\n", ctx == nullptr ? "null" : "non-null");

    printf("Done - no crash means the 32-bit proxy forwarded correctly.\n");

    // Negative control: the same wrapper32.cpp source, compiled as x64 (target
    // opengl32_enh32_x64 in the "default"/x64 preset's build/ dir, a sibling of this
    // build-x86/ dir). A 32-bit process must not be able to load it - proves the
    // bitness-mismatch failure this whole x86 build exists to avoid.
    printf("\nLoading a 64-bit DLL from this 32-bit process (expected to fail)...\n");
    void* mod64 = LoadLibraryA("..\\build\\opengl32_enh32_x64.dll");
    if (mod64) {
        printf("UNEXPECTED: 64-bit DLL loaded into a 32-bit process (mod=%p)\n", mod64);
        return 1;
    }
    unsigned long err64 = GetLastError();
    printf("LoadLibraryA of the 64-bit DLL failed as expected, GetLastError=%lu", err64);
    if (err64 == 193) {
        printf(" (ERROR_BAD_EXE_FORMAT - confirms the bitness mismatch)\n");
    } else {
        printf(" (expected 193/ERROR_BAD_EXE_FORMAT - got a different error, check the path/build)\n");
        return 1;
    }

    return 0;
}
