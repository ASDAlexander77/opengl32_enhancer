// See window_override.h.
#include <cstdint>
#include <cstdio>

#include "config.h"
#include "debug_log.h"
#include "window_override.h"

// Same no-<windows.h> discipline as the rest of this DLL - see wrapper.cpp's header comment.
typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;
typedef unsigned int UINT;
typedef void* HWND;
typedef void* HDC;
typedef void* HMONITOR;
typedef void* HMODULE;

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

struct MONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
};

extern "C" {
    __declspec(dllimport) DWORD __stdcall GetLastError(void);
    __declspec(dllimport) HWND __stdcall WindowFromDC(HDC hdc);
    __declspec(dllimport) LONG __stdcall GetWindowLongA(HWND hWnd, int nIndex);
    __declspec(dllimport) BOOL __stdcall AdjustWindowRectEx(RECT* lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
    __declspec(dllimport) BOOL __stdcall SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
    __declspec(dllimport) BOOL __stdcall GetClientRect(HWND hWnd, RECT* lpRect);
    __declspec(dllimport) HMONITOR __stdcall MonitorFromWindow(HWND hwnd, DWORD dwFlags);
    __declspec(dllimport) BOOL __stdcall GetMonitorInfoA(HMONITOR hMonitor, MONITORINFO* lpmi);
    __declspec(dllimport) BOOL __stdcall SetProcessDPIAware(void);
    __declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* lpModuleName);
    __declspec(dllimport) void* __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
}

namespace {

const int GWL_STYLE = -16;
const int GWL_EXSTYLE = -20;
const UINT SWP_NOMOVE = 0x0002;
const UINT SWP_NOZORDER = 0x0004;
const UINT SWP_NOACTIVATE = 0x0010;
const DWORD WS_CAPTION = 0x00C00000;
const DWORD WS_POPUP = 0x80000000;
const DWORD MONITOR_DEFAULTTONEAREST = 0x00000002;

}  // namespace

void FitClientSizeToMonitor(int requestedWidth, int requestedHeight, int monitorWidth,
                            int monitorHeight, int& outWidth, int& outHeight) {
    outWidth = requestedWidth;
    outHeight = requestedHeight;
    if (requestedWidth <= 0 || requestedHeight <= 0 || monitorWidth <= 0 || monitorHeight <= 0) {
        return;
    }
    if (requestedWidth <= monitorWidth && requestedHeight <= monitorHeight) {
        return;
    }
    // The tighter of the two axes decides, so the result fits on both and keeps the requested
    // shape - the upscale stages map one scalar ratio onto both axes.
    double sx = (double)monitorWidth / (double)requestedWidth;
    double sy = (double)monitorHeight / (double)requestedHeight;
    double s = sx < sy ? sx : sy;
    outWidth = (int)((double)requestedWidth * s);
    outHeight = (int)((double)requestedHeight * s);
    if (outWidth > monitorWidth) { outWidth = monitorWidth; }
    if (outHeight > monitorHeight) { outHeight = monitorHeight; }
    if (outWidth < 1) { outWidth = 1; }
    if (outHeight < 1) { outHeight = 1; }
}

void EnsureProcessDpiAware() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    const AnaxConfig& config = GetAnaxConfig();
    if (!config.dpiAware) {
        printf("[opengl32_enh_cpp] dpi: dpiAware=0, leaving Windows' DPI scaling in place\n");
        return;
    }

    // Per-monitor v2 is Windows 10 1703+; resolved dynamically so the DLL still loads on older
    // systems, which fall back to the Vista-era system-aware call. DPI_AWARENESS_CONTEXT is a
    // pseudo-handle; PER_MONITOR_AWARE_V2 is ((DPI_AWARENESS_CONTEXT)-4).
    typedef BOOL (__stdcall *PfnSetProcessDpiAwarenessContext)(void*);
    HMODULE user32 = GetModuleHandleA("user32.dll");
    PfnSetProcessDpiAwarenessContext setContext = user32
        ? (PfnSetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext")
        : nullptr;
    if (setContext != nullptr && setContext((void*)(intptr_t)-4)) {
        printf("[opengl32_enh_cpp] dpi: process declared per-monitor DPI aware - the game's "
               "window is no longer bitmap-stretched by display scaling\n");
        return;
    }
    if (SetProcessDPIAware()) {
        printf("[opengl32_enh_cpp] dpi: process declared system DPI aware\n");
        return;
    }
    // Most likely the game's own manifest already set an awareness, which wins and is fine.
    printf("[opengl32_enh_cpp] dpi: could not change DPI awareness (GetLastError=%lu) - "
           "already set by the game, most likely\n", GetLastError());
}

void ApplyWindowSizeOverride(void* hdcRaw) {
    // The earliest point this DLL runs any of its own logic in a game - wglCreateContext comes
    // before the first wglSwapBuffers - so this is where the log starts, early enough to capture
    // the config load that GetAnaxConfig() triggers on the line below. No-ops outside a game.
    RedirectStdoutToDebugLog();

    const AnaxConfig& config = GetAnaxConfig();
    if (config.windowWidth <= 0 || config.windowHeight <= 0) {
        return;
    }

    HDC hdc = (HDC)hdcRaw;
    HWND hwnd = WindowFromDC(hdc);
    if (hwnd == nullptr) {
        printf("[opengl32_enh_cpp] windowSize: WindowFromDC found no window for this DC, skipping override\n");
        return;
    }

    DWORD style = (DWORD)GetWindowLongA(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongA(hwnd, GWL_EXSTYLE);

    int clientWidth = config.windowWidth;
    int clientHeight = config.windowHeight;

    // A fullscreen game window is a caption-less popup covering the monitor, and the monitor is
    // in whatever display mode the game just switched it to. Growing that window past the
    // monitor does not make the picture bigger, it pushes most of it off-screen: what is left
    // visible is the top-left corner of an upscaled frame - the "zoomed in" fullscreen. So in
    // fullscreen the request is fitted to the monitor (aspect preserved) and the window is put
    // back at the monitor's origin; a windowed game keeps the exact size it asked for.
    bool fullscreen = (style & WS_POPUP) != 0 && (style & WS_CAPTION) != WS_CAPTION;
    int monitorX = 0, monitorY = 0;
    if (fullscreen) {
        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (monitor != nullptr && GetMonitorInfoA(monitor, &info)) {
            int monitorWidth = (int)(info.rcMonitor.right - info.rcMonitor.left);
            int monitorHeight = (int)(info.rcMonitor.bottom - info.rcMonitor.top);
            monitorX = (int)info.rcMonitor.left;
            monitorY = (int)info.rcMonitor.top;
            FitClientSizeToMonitor(config.windowWidth, config.windowHeight, monitorWidth,
                                   monitorHeight, clientWidth, clientHeight);
            if (clientWidth != config.windowWidth || clientHeight != config.windowHeight) {
                printf("[opengl32_enh_cpp] windowSize: fullscreen window on a %dx%d monitor - "
                       "%dx%d would run off the screen, using %dx%d instead\n",
                       monitorWidth, monitorHeight, config.windowWidth, config.windowHeight,
                       clientWidth, clientHeight);
            }
            // Centred, so a fitted size that does not fill one axis is letterboxed evenly.
            monitorX += (monitorWidth - clientWidth) / 2;
            monitorY += (monitorHeight - clientHeight) / 2;
        }
    }

    // The desired size is the CLIENT area (what the game actually renders into); AdjustWindowRectEx
    // grows that by however much border/titlebar/menu the window's own style adds, so the client
    // area ends up exactly windowWidth x windowHeight regardless of the window's decoration.
    RECT rect = {0, 0, (LONG)clientWidth, (LONG)clientHeight};
    AdjustWindowRectEx(&rect, style, 0, exStyle);

    int width = (int)(rect.right - rect.left);
    int height = (int)(rect.bottom - rect.top);

    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (fullscreen ? 0 : SWP_NOMOVE);
    if (!SetWindowPos(hwnd, nullptr, monitorX, monitorY, width, height, flags)) {
        printf("[opengl32_enh_cpp] windowSize: SetWindowPos FAILED, GetLastError=%lu\n", GetLastError());
        return;
    }

    printf("[opengl32_enh_cpp] windowSize: overrode window client size to %dx%d%s\n",
           clientWidth, clientHeight, fullscreen ? " (fullscreen)" : "");
}

bool GetWindowClientSize(void* hdcRaw, int& outWidth, int& outHeight) {
    HDC hdc = (HDC)hdcRaw;
    HWND hwnd = WindowFromDC(hdc);
    if (hwnd == nullptr) {
        return false;
    }
    RECT rect = {};
    if (!GetClientRect(hwnd, &rect)) {
        return false;
    }
    outWidth = (int)(rect.right - rect.left);
    outHeight = (int)(rect.bottom - rect.top);
    return true;
}
