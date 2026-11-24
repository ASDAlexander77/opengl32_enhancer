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
    //
    // That x64 build is currently disabled outright (CMakeLists.txt stops with
    // "64bit: Unsupported for now"), so build/ usually does not exist at all and this control
    // has nothing to load. Treat that as SKIPPED rather than failed: a missing fixture is not
    // evidence the proxy is broken, and failing on it would make this test permanently red for
    // everyone, training people to ignore it. Re-enable the x64 build and it starts asserting
    // again by itself. ERROR_MOD_NOT_FOUND (126) / ERROR_PATH_NOT_FOUND (3) mean "not built";
    // any other non-193 error is a genuine surprise and still fails.
    printf("\nLoading a 64-bit DLL from this 32-bit process (expected to fail)...\n");
    void* mod64 = LoadLibraryA("..\\build\\opengl32_enh32_x64.dll");
    if (mod64) {
        printf("UNEXPECTED: 64-bit DLL loaded into a 32-bit process (mod=%p)\n", mod64);
        return 1;
    }
    unsigned long err64 = GetLastError();
    if (err64 == 193) {
        printf("LoadLibraryA of the 64-bit DLL failed as expected, GetLastError=193"
               " (ERROR_BAD_EXE_FORMAT - confirms the bitness mismatch)\n");
    } else if (err64 == 126 || err64 == 3) {
        printf("SKIPPED: no x64 build to test against (GetLastError=%lu). The x64 target is\n"
               "disabled in CMakeLists.txt, so ..\\build\\opengl32_enh32_x64.dll was never\n"
               "produced. Re-enable it to restore this negative control.\n", err64);
    } else {
        printf("LoadLibraryA of the 64-bit DLL failed with GetLastError=%lu, which is neither\n"
               "193 (ERROR_BAD_EXE_FORMAT, the expected bitness mismatch) nor a missing-file\n"
               "error - check the path/build.\n", err64);
        return 1;
    }

    return 0;
}
