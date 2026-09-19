// See modelview_capture.h.
#include <windows.h>
#include <cstdio>

#include "modelview_capture.h"

namespace {

const unsigned int kGlModelviewMatrix = 0x0BA6;
const unsigned int kGlModelview = 0x1700;

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

// GL's initial matrix mode is GL_MODELVIEW, so that is the honest starting value.
unsigned int g_matrixMode = kGlModelview;
int g_modelviewDepth = 0;

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

    // glGetFloatv writes nothing at all when there is no current context, which would leave the
    // default-constructed identity in place and record a camera at the world origin as though it
    // were real - worse than recording nothing, because a consumer cannot tell the difference. A
    // view matrix built from the fixed-function stack always has m[15] == 1, so a sentinel that
    // survives the call means nothing was written and there is nothing to record.
    CameraMatrix taken;
    taken.m[15] = 0.0f;
    glGetFloatv(kGlModelviewMatrix, taken.m);
    if (taken.m[15] != 1.0f) {
        return;
    }
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
    // Safe as a latch point because an engine switches the PROJECTION to ortho before it
    // touches the modelview - the modelview still holds the camera at this instant.
    LatchCamera();
    g_armed = false;
}

void NotifyMatrixMode(unsigned int mode) {
    g_matrixMode = mode;
}

void NotifyMatrixPush() {
    if (g_matrixMode != kGlModelview) {
        return;
    }
    // The primary latch point: the first push away from depth 0 is the engine starting to draw
    // something with its own transform, and a push copies the top of stack without modifying
    // it, so the camera is still there to be read.
    if (g_modelviewDepth == 0) {
        LatchCamera();
    }
    ++g_modelviewDepth;
}

void NotifyMatrixPop() {
    if (g_matrixMode != kGlModelview) {
        return;
    }
    if (g_modelviewDepth > 0) {
        --g_modelviewDepth;
    }
}

void NotifyMatrixEdited() {
    // Depth 0 is what separates camera setup from an entity's own transform; see the header.
    if (g_armed && !g_latched && g_matrixMode == kGlModelview && g_modelviewDepth == 0) {
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
    g_modelviewDepth = 0;
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
    // Column-major: element (row, col) is m[col * 4 + row]. The view matrix is [R | t] with R
    // the world->view rotation, so R's ROWS are the camera's axes in world coordinates - and a
    // row of R is a stride-4 walk of the stored array.
    const float* m = in.m;

    out.right[0]   =  m[0];  out.right[1]   =  m[4];  out.right[2]   =  m[8];
    out.up[0]      =  m[1];  out.up[1]      =  m[5];  out.up[2]      =  m[9];
    out.forward[0] = -m[2];  out.forward[1] = -m[6];  out.forward[2] = -m[10];

    // The eye in world space is -R^T * t. R is orthonormal for any camera built from rotations
    // and a translation, so the transpose is the inverse and no general inversion is needed -
    // a game that scales its modelview before drawing the world would break that assumption,
    // and no id Tech 2-era engine does.
    const float tx = m[12], ty = m[13], tz = m[14];
    out.position[0] = -(m[0] * tx + m[1] * ty + m[2]  * tz);
    out.position[1] = -(m[4] * tx + m[5] * ty + m[6]  * tz);
    out.position[2] = -(m[8] * tx + m[9] * ty + m[10] * tz);
}
