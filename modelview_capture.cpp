// See modelview_capture.h.
#include <windows.h>
#include <cstdio>

#include "modelview_capture.h"

namespace {

const unsigned int kGlModelviewMatrix = 0x0BA6;

typedef void (__stdcall *PfnGlGetFloatv)(unsigned int pname, float* params);

// Resolved here rather than through gl_loader.h on purpose: that loader gates GL 4.3 compute
// support and its GL 1.1 fallbacks behind one shared flag, so on a machine where the compute
// stages cannot run it hands back nothing at all - and matrix capture has no reason to die with
// them. GetSystemDirectoryA rather than a hardcoded path, matching wrapper.cpp: Windows need
// not be installed on C: or be called "Windows".
PfnGlGetFloatv ResolveGlGetFloatv() {
    static PfnGlGetFloatv fn = nullptr;
    static bool tried = false;
    if (tried) {
        return fn;
    }
    tried = true;

    char sysDir[MAX_PATH];
    UINT sysDirLen = GetSystemDirectoryA(sysDir, sizeof(sysDir));
    if (sysDirLen == 0 || sysDirLen >= sizeof(sysDir)) {
        printf("[opengl32_enh_cpp] modelview: FAILED to get system directory, GetLastError=%lu\n",
               GetLastError());
        return nullptr;
    }
    char dllPath[MAX_PATH + 16];
    snprintf(dllPath, sizeof(dllPath), "%s\\opengl32.dll", sysDir);

    HMODULE real = LoadLibraryA(dllPath);
    if (real == nullptr) {
        printf("[opengl32_enh_cpp] modelview: FAILED to load real opengl32.dll, GetLastError=%lu\n",
               GetLastError());
        return nullptr;
    }
    fn = (PfnGlGetFloatv)GetProcAddress(real, "glGetFloatv");
    if (fn == nullptr) {
        printf("[opengl32_enh_cpp] modelview: FAILED to resolve glGetFloatv, GetLastError=%lu\n",
               GetLastError());
    }
    return fn;
}

CameraMatrix g_current;
CameraMatrix g_previous;
bool g_hasCurrent = false;
bool g_hasPrevious = false;

bool g_armed = false;    // between glFrustum and glOrtho (or the end of the frame)
bool g_pending = false;  // a depth-0 modelview edit happened while armed
bool g_latched = false;  // the camera for this frame has been read
bool g_loggedFirst = false;

// Reads the driver's modelview matrix, if one is pending and none has been taken this frame.
// Idempotent within a frame, which is what lets all three latch points call it unconditionally.
void LatchCamera() {
    if (!g_pending || g_latched) {
        return;
    }
    PfnGlGetFloatv glGetFloatv = ResolveGlGetFloatv();
    if (glGetFloatv == nullptr) {
        return;
    }

    CameraMatrix taken;
    glGetFloatv(kGlModelviewMatrix, taken.m);
    g_current = taken;
    g_hasCurrent = true;
    g_latched = true;
    g_pending = false;

    if (!g_loggedFirst) {
        printf("[opengl32_enh_cpp] modelview: captured first camera matrix, camera-relative "
               "stages can now be placed\n");
        g_loggedFirst = true;
    }
}

}  // namespace

void NotifyWorldProjection() {
    g_armed = true;
}

void NotifyTwoDProjection() {
}

void NotifyMatrixMode(unsigned int mode) {
    (void)mode;
}

void NotifyMatrixPush() {
}

void NotifyMatrixPop() {
}

void NotifyMatrixEdited() {
    if (g_armed && !g_latched) {
        g_pending = true;
    }
}

void FinalizeCameraForFrame() {
    LatchCamera();
}

void AdvanceCameraHistory() {
    if (g_latched) {
        g_previous = g_current;
        g_hasPrevious = true;
    }
    g_armed = false;
    g_pending = false;
    g_latched = false;
}

bool GetCapturedCamera(CameraMatrix& out) {
    if (!g_hasCurrent) {
        return false;
    }
    out = g_current;
    return true;
}

bool GetPreviousCamera(CameraMatrix& out) {
    if (!g_hasPrevious) {
        return false;
    }
    out = g_previous;
    return true;
}

void DecomposeCamera(const CameraMatrix& in, CameraPose& out) {
    (void)in;
    (void)out;
}
