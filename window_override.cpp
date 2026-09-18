// See window_override.h.
#include <cstdio>

#include "config.h"
#include "window_override.h"

// Same no-<windows.h> discipline as the rest of this DLL - see wrapper.cpp's header comment.
typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;
typedef unsigned int UINT;
typedef void* HWND;
typedef void* HDC;

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

extern "C" {
    __declspec(dllimport) DWORD __stdcall GetLastError(void);
    __declspec(dllimport) HWND __stdcall WindowFromDC(HDC hdc);
    __declspec(dllimport) LONG __stdcall GetWindowLongA(HWND hWnd, int nIndex);
    __declspec(dllimport) BOOL __stdcall AdjustWindowRectEx(RECT* lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
    __declspec(dllimport) BOOL __stdcall SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
}

namespace {

const int GWL_STYLE = -16;
const int GWL_EXSTYLE = -20;
const UINT SWP_NOMOVE = 0x0002;
const UINT SWP_NOZORDER = 0x0004;
const UINT SWP_NOACTIVATE = 0x0010;

}  // namespace

void ApplyWindowSizeOverride(void* hdcRaw) {
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

    // The desired size is the CLIENT area (what the game actually renders into); AdjustWindowRectEx
    // grows that by however much border/titlebar/menu the window's own style adds, so the client
    // area ends up exactly windowWidth x windowHeight regardless of the window's decoration.
    RECT rect = {0, 0, (LONG)config.windowWidth, (LONG)config.windowHeight};
    DWORD style = (DWORD)GetWindowLongA(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongA(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rect, style, 0, exStyle);

    int width = (int)(rect.right - rect.left);
    int height = (int)(rect.bottom - rect.top);

    if (!SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        printf("[opengl32_enh_cpp] windowSize: SetWindowPos FAILED, GetLastError=%lu\n", GetLastError());
        return;
    }

    printf("[opengl32_enh_cpp] windowSize: overrode window client size to %dx%d\n",
           config.windowWidth, config.windowHeight);
}
