// See debug_log.h. No GL and no GPU, so this is an unlabeled test that runs anywhere, CI
// included (see CMakeLists.txt's "gpu" label note).
//
// Testing a stdout redirect from inside the process doing the redirecting would hijack this
// test's own output - ctest would stop seeing the PASS lines it is meant to read. So the test
// re-executes ITSELF as a child with --child and inspects what the child left behind. The child
// is spawned DETACHED_PROCESS on purpose: that guarantees it has no console, which is precisely
// the condition RedirectStdoutToDebugLog() keys off and the condition a GUI game like
// Anachronox always presents. Spawning it normally would inherit this test's console and the
// redirect would correctly decline to happen, testing nothing.
#include <windows.h>
#include <cstdio>
#include <cstring>

#include "debug_log.h"

namespace {

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

const char* kSentinel = "debug_log_test sentinel line";

bool FileContains(const char* path, const char* needle) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    char buffer[4096] = {};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    buffer[read] = '\0';
    return strstr(buffer, needle) != nullptr;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--child") == 0) {
        // No console here (DETACHED_PROCESS), so this must land in the log file.
        if (!RedirectStdoutToDebugLog()) {
            return 2;
        }
        printf("%s\n", kSentinel);
        fflush(stdout);
        return 0;
    }

    char logPath[512] = {};
    if (!Check(GetDebugLogPath(logPath, sizeof(logPath)),
               "GetDebugLogPath() resolves a path next to this module")) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("  log path: %s\n", logPath);

    size_t pathLen = strlen(logPath);
    const char* kExpectedName = "opengl32_enhancer.log";
    size_t nameLen = strlen(kExpectedName);
    Check(pathLen > nameLen && strcmp(logPath + pathLen - nameLen, kExpectedName) == 0,
          "the log is named opengl32_enhancer.log, beside the module");

    // A stale file from a previous run would make the check below pass without the child ever
    // having written anything.
    remove(logPath);
    Check(!FileContains(logPath, kSentinel), "no stale log file is left over from a previous run");

    char exePath[512] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    char command[1100] = {};
    snprintf(command, sizeof(command), "\"%s\" --child", exePath);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    bool spawned = CreateProcessA(nullptr, command, nullptr, nullptr, FALSE,
                                   DETACHED_PROCESS, nullptr, nullptr, &si, &pi) != 0;
    if (!Check(spawned, "spawned a console-less child process")) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }

    WaitForSingleObject(pi.hProcess, 10000);
    DWORD childExit = 1;
    GetExitCodeProcess(pi.hProcess, &childExit);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    Check(childExit == 0, "the child redirected its stdout without erroring");
    Check(FileContains(logPath, kSentinel),
          "a printf in a console-less process lands in the log file");

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
