#pragma once

// Forces the game window to a configured client size at the moment it creates its GL context -
// see generators/gen_wrapper_cpp.py's wglCreateContext special case. By the time a game calls
// wglCreateContext its window already exists and this DC already has SetPixelFormat applied to
// it (WGL requires that ordering), so this is the earliest point a proxy can reliably resize it,
// and it fires again on every subsequent context re-creation (a `vid_restart`/video-mode change
// recreates the GL context on id Tech 2-era engines, but not necessarily the window itself, so a
// game that reset its own window size on such a restart would otherwise silently undo this).
//
// Independent of the effect= pipeline: nothing here touches rendering, only the OS window. See
// GetAnaxConfig().windowWidth/windowHeight, opengl32_enhancer.ini's `windowWidth`/`windowHeight`
// (both default 0, meaning "leave the game's own window size alone").

// Called from the generated wglCreateContext wrapper with the HDC the game passed in, before
// forwarding to the real wglCreateContext. No-ops if windowWidth/windowHeight aren't both set to
// a positive value, if the HDC has no window behind it (WindowFromDC failed), or if SetWindowPos
// itself fails - logged via printf either way, never fatal to context creation.
void ApplyWindowSizeOverride(void* hdc);
