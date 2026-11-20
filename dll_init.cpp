// Load-time initialization for the proxy DLL only (never linked into the tests or the editor).
//
// A static initializer rather than a DllMain: the static CRT already owns DllMain and runs this
// from inside it, after the CRT itself is up, which is what makes the stdio and config reads
// below safe. Nothing here loads a library or waits on another thread - the two things that are
// actually dangerous under the loader lock. user32 is already loaded (this DLL imports it).
//
// What has to happen this early: DPI awareness. A window's DPI awareness is fixed when it is
// created, and a Quake II-family renderer LoadLibrary's opengl32 (gl_driver) BEFORE it creates
// its window - so load time is the last moment the game's window can still be made DPI-aware.
// wglCreateContext, the next earliest hook, already has a window behind it.
#include "debug_log.h"
#include "window_override.h"

namespace {

struct DllInit {
    DllInit() {
        RedirectStdoutToDebugLog();
        EnsureProcessDpiAware();
        InstallBorderlessFullscreen();
    }
};

DllInit g_dllInit;

}  // namespace
