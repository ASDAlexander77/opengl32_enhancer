// Creates a real window (no GL context needed - ApplyWindowSizeOverride only touches the
// window, not rendering) and checks ApplyWindowSizeOverride() against config.h's
// windowWidth/windowHeight. See gl_loader_test.cpp for why <windows.h> is safe here (this is a
// standalone .exe, not linked into wrapper.cpp, which is the one file in this DLL that avoids
// it). Uses GetMutableAnaxConfig() directly to drive scenarios, the same way post_effects_test.cpp
// does, rather than juggling multiple fixture .ini files for a module this small.
#include <windows.h>
#include <cstdio>

#include "config.h"
#include "window_override.h"

namespace {

int g_failures = 0;

bool Check(bool condition, const char* what) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++g_failures;
    }
    return condition;
}

// Client-area width/height of hwnd, as GetClientRect sees it - this is what a game's own
// resize handling would read, so it's the right thing to assert against rather than the
// window's outer rect (which also includes borders/titlebar).
void GetClientSize(HWND hwnd, int& width, int& height) {
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxWindowOverrideTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "window_override_test", WS_OVERLAPPEDWINDOW,
        0, 0, 320, 240, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }
    HDC hdc = GetDC(hwnd);

    int width = 0, height = 0;
    GetClientSize(hwnd, width, height);
    printf("  initial client size: %dx%d\n", width, height);

    // windowWidth/windowHeight default to 0 - leave the game's own window size alone.
    {
        AnaxConfig& config = GetMutableAnaxConfig();
        config.windowWidth = 0;
        config.windowHeight = 0;

        ApplyWindowSizeOverride(hdc);

        int w, h;
        GetClientSize(hwnd, w, h);
        Check(w == width && h == height, "windowWidth=0, windowHeight=0 leaves the window untouched");
    }

    // Only one of the two set - a partial override is more likely a typo than a deliberate
    // request, so this must no-op too (see config.h's windowWidth/windowHeight comment).
    {
        AnaxConfig& config = GetMutableAnaxConfig();
        config.windowWidth = 800;
        config.windowHeight = 0;

        ApplyWindowSizeOverride(hdc);

        int w, h;
        GetClientSize(hwnd, w, h);
        Check(w == width && h == height, "windowWidth set alone (windowHeight=0) leaves the window untouched");
    }

    // Both set - the window's CLIENT area must land exactly on the configured size, regardless
    // of how much border/titlebar WS_OVERLAPPEDWINDOW adds around it.
    {
        AnaxConfig& config = GetMutableAnaxConfig();
        config.windowWidth = 640;
        config.windowHeight = 480;

        ApplyWindowSizeOverride(hdc);

        int w, h;
        GetClientSize(hwnd, w, h);
        Check(w == 640 && h == 480, "windowWidth=640, windowHeight=480 resizes the client area exactly");
    }

    // A second override to a different size must apply again (a vid_restart calls
    // wglCreateContext again on the same window) - not just once, ever.
    {
        AnaxConfig& config = GetMutableAnaxConfig();
        config.windowWidth = 1024;
        config.windowHeight = 768;

        ApplyWindowSizeOverride(hdc);

        int w, h;
        GetClientSize(hwnd, w, h);
        Check(w == 1024 && h == 768, "a later call with a different size resizes again");
    }

    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed.\n");
    return 0;
}
