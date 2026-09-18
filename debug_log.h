#pragma once

#include <cstddef>

// Gives this DLL somewhere to say things when it is loaded into a game.
//
// Every diagnostic in this project is a printf. That works for the tests and for
// config_editor.exe (which deliberately gets a console back - see CMakeLists.txt), but a game is
// a GUI-subsystem process with no console at all, so in the one place the proxy actually runs,
// every one of those printfs has been going nowhere. That is not a cosmetic gap: it is why a
// window-size override that Windows silently clamped looked like "the setting is ignored" rather
// than reporting itself, and why anything that goes wrong inside a real game has to be diagnosed
// by eye instead of by reading a log.
//
// Redirecting stdout, rather than introducing a logging call, is deliberate: it captures every
// existing printf in the codebase without touching a single call site, and keeps "how do I report
// something" answered the same way everywhere.

// The path the log is written to: opengl32_enhancer.log, in the directory this module lives in -
// the game's own folder, next to opengl32.dll and opengl32_enhancer.ini, so it lands where
// someone looking for it would look. Returns false if the module path could not be resolved.
bool GetDebugLogPath(char* outPath, size_t outPathSize);

// Points stdout at that file, so every printf in this DLL is recorded. Idempotent - safe to call
// on every entry point, and only the first call does anything.
//
// Does nothing and returns false when the process already has a console, which is how
// config_editor.exe and the test executables keep printing to the terminal where someone is
// watching. Returns false too if the path could not be resolved or the file could not be opened
// (a read-only game directory, most likely) - callers carry on regardless, since losing the log
// must never stop the proxy from rendering.
bool RedirectStdoutToDebugLog();
