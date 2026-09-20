// See debug_log.h.
#include <cstdio>

#include "debug_log.h"

// Same no-<windows.h> discipline as the rest of this DLL - see wrapper.cpp's header comment.
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HMODULE;
typedef void* HWND;

extern "C" {
    __declspec(dllimport) BOOL __stdcall GetModuleHandleExA(DWORD dwFlags, const char* lpModuleName, HMODULE* phModule);
    __declspec(dllimport) DWORD __stdcall GetModuleFileNameA(HMODULE hModule, char* lpFilename, DWORD nSize);
    __declspec(dllimport) HWND __stdcall GetConsoleWindow(void);
}

namespace {

const DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 0x00000004;
const DWORD GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x00000002;

// Any function defined in this module works as the address anchor for GetModuleHandleExA - it
// identifies "the module this code is running as", which is this DLL when linked into the proxy
// or the test .exe itself when linked into debug_log_test. Same technique as config.cpp's
// GetIniPathNextToThisModule, and for the same reason: the game's working directory is not
// reliably the folder the proxy was dropped into.
void AddressAnchor() {}

bool g_redirected = false;

}  // namespace

bool GetDebugLogPath(char* outPath, size_t outPathSize) {
    HMODULE hModule = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<const char*>(&AddressAnchor), &hModule)) {
        return false;
    }
    char modulePath[512];
    DWORD len = GetModuleFileNameA(hModule, modulePath, sizeof(modulePath));
    if (len == 0 || len >= sizeof(modulePath)) {
        return false;
    }
    char* lastSlash = nullptr;
    for (char* p = modulePath; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            lastSlash = p;
        }
    }
    if (lastSlash == nullptr) {
        return false;
    }
    *(lastSlash + 1) = '\0';
    return snprintf(outPath, outPathSize, "%sopengl32_enhancer.log", modulePath) > 0;
}

bool RedirectStdoutToDebugLog() {
    if (g_redirected) {
        return true;
    }
    if (GetConsoleWindow() != nullptr) {
        return false;
    }

    char logPath[512];
    if (!GetDebugLogPath(logPath, sizeof(logPath))) {
        return false;
    }
    if (freopen(logPath, "w", stdout) == nullptr) {
        return false;
    }

    // A game can sit at a loading screen for a long time between diagnostics, and can be killed
    // outright rather than exiting cleanly, which would strand a buffered log. The file has to be
    // worth reading while the game is still running - which is exactly when it is needed.
    //
    // Unbuffered rather than line buffered, because MSVC does not implement _IOLBF: it documents
    // the mode as equivalent to _IOFBF, so asking for line buffering here silently produced a 4KB
    // block-buffered log. The symptom is subtle and was mistaken for the game hanging - the last
    // line in the file is always a truncated fragment sitting on a block boundary, everything
    // since the last flush is lost when the process is killed, and a quiet run (one where
    // cameraLogInterval is 0, say) can write almost nothing to disk for minutes at a time.
    //
    // The cost is a write per printf, which this DLL can afford precisely because it refuses to
    // log per call: see generators/gen_wrapper_cpp.py's ANAX_TRACE, which is compile-time off for
    // exactly this reason.
    setvbuf(stdout, nullptr, _IONBF, 0);
    g_redirected = true;
    return true;
}
