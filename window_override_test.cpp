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

// The largest CLIENT area a window of this style can actually reach on this desktop.
//
// DefWindowProc caps a sizable window at SM_CXMAXTRACK x SM_CYMAXTRACK - the desktop plus about
// 20px - when it handles WM_GETMINMAXINFO, and SetWindowPos silently honours that cap while
// still returning TRUE. So asking for a client area larger than this yields a smaller window and
// no error at all, and a check asserting on the exact requested size would fail for reasons that
// have nothing to do with ApplyWindowSizeOverride. This is what broke CI: GitHub's headless
// windows runners have a 1024x768 desktop, where a 1024x768 CLIENT area needs a 1040x807 outer
// window and that 807 does not fit under a max track height of roughly 788.
void GetMaxClientSize(HWND hwnd, int& width, int& height) {
    RECT decoration = {0, 0, 0, 0};
    AdjustWindowRectEx(&decoration, (DWORD)GetWindowLongA(hwnd, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongA(hwnd, GWL_EXSTYLE));
    width = GetSystemMetrics(SM_CXMAXTRACK) - (decoration.right - decoration.left);
    height = GetSystemMetrics(SM_CYMAXTRACK) - (decoration.bottom - decoration.top);
}

// Shrinks a size the checks below would like to use down to one the OS will actually grant.
void ClampToMaxClient(int maxWidth, int maxHeight, int& width, int& height) {
    if (width > maxWidth) {
        width = maxWidth;
    }
    if (height > maxHeight) {
        height = maxHeight;
    }
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

    // Both resize checks below assert on an exact client size, so they must ask for sizes this
    // desktop can actually host - see GetMaxClientSize.
    int maxWidth = 0, maxHeight = 0;
    GetMaxClientSize(hwnd, maxWidth, maxHeight);
    printf("  largest client size this desktop allows: %dx%d\n", maxWidth, maxHeight);

    int firstWidth = 640, firstHeight = 480;
    ClampToMaxClient(maxWidth, maxHeight, firstWidth, firstHeight);
    int secondWidth = 1024, secondHeight = 768;
    ClampToMaxClient(maxWidth, maxHeight, secondWidth, secondHeight);
    if (secondWidth == firstWidth && secondHeight == firstHeight) {
        // A desktop too small to host either size - go smaller instead, so the "resizes again"
        // check still asks for a size genuinely different from the first one.
        secondWidth = firstWidth / 2;
        secondHeight = firstHeight / 2;
    }

    char label[160];

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
        config.windowWidth = firstWidth;
        config.windowHeight = firstHeight;

        ApplyWindowSizeOverride(hdc);

        int w, h;
        GetClientSize(hwnd, w, h);
        snprintf(label, sizeof(label),
                 "windowWidth=%d, windowHeight=%d resizes the client area exactly",
                 firstWidth, firstHeight);
        Check(w == firstWidth && h == firstHeight, label);
    }

    // A second override to a different size must apply again (a vid_restart calls
    // wglCreateContext again on the same window) - not just once, ever.
    {
        AnaxConfig& config = GetMutableAnaxConfig();
        config.windowWidth = secondWidth;
        config.windowHeight = secondHeight;

        ApplyWindowSizeOverride(hdc);

        int w, h;
        GetClientSize(hwnd, w, h);
        snprintf(label, sizeof(label), "a later call with a different size (%dx%d) resizes again",
                 secondWidth, secondHeight);
        Check(w == secondWidth && h == secondHeight, label);
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
