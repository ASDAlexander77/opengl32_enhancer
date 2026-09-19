// Creates a real OpenGL context and drives modelview_capture.h's hooks alongside the real GL
// calls a Quake II-family engine makes, then checks the captured matrix against what the driver
// itself reports. The point is to verify the GATE - which matrix gets picked - not matrix
// arithmetic, which this module never performs. See modelview_capture.h.
#include <windows.h>
#include <cstdio>

#include "modelview_capture.h"

namespace {

const unsigned int GL_PROJECTION       = 0x1701;
const unsigned int GL_MODELVIEW        = 0x1700;
const unsigned int GL_MODELVIEW_MATRIX = 0x0BA6;

typedef void (__stdcall *PfnMatrixMode)(unsigned int);
typedef void (__stdcall *PfnLoadIdentity)(void);
typedef void (__stdcall *PfnFrustum)(double, double, double, double, double, double);
typedef void (__stdcall *PfnOrtho)(double, double, double, double, double, double);
typedef void (__stdcall *PfnRotatef)(float, float, float, float);
typedef void (__stdcall *PfnTranslatef)(float, float, float);
typedef void (__stdcall *PfnPushMatrix)(void);
typedef void (__stdcall *PfnPopMatrix)(void);
typedef void (__stdcall *PfnGetFloatv)(unsigned int, float*);

PfnMatrixMode   pMatrixMode   = nullptr;
PfnLoadIdentity pLoadIdentity = nullptr;
PfnFrustum      pFrustum      = nullptr;
PfnOrtho        pOrtho        = nullptr;
PfnRotatef      pRotatef      = nullptr;
PfnTranslatef   pTranslatef   = nullptr;
PfnPushMatrix   pPushMatrix   = nullptr;
PfnPopMatrix    pPopMatrix    = nullptr;
PfnGetFloatv    pGetFloatv    = nullptr;

int g_failures = 0;

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
}

bool MatricesEqual(const float* a, const float* b) {
    for (int i = 0; i < 16; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

void PrintMatrix(const char* label, const float* m) {
    printf("  %s = [%.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f]\n",
           label, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
           m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
}

// The projection half of Quake II's R_SetupGL: glMatrixMode(GL_PROJECTION), glLoadIdentity,
// then MYgluPerspective, which reaches GL as glFrustum. Both the real call and the hook.
void BeginWorldPass() {
    pMatrixMode(GL_PROJECTION);
    NotifyMatrixMode(GL_PROJECTION);
    pLoadIdentity();
    NotifyMatrixEdited();
    pFrustum(-1.0, 1.0, -0.75, 0.75, 1.0, 4096.0);
    NotifyWorldProjection();
}

// The modelview half: glLoadIdentity, five glRotatef, one glTranslatef, exactly as R_SetupGL
// issues them. eye is the world-space view origin the engine passes as -vieworg.
void SetWorldCamera(float pitch, float yaw, float roll, float eyeX, float eyeY, float eyeZ) {
    pMatrixMode(GL_MODELVIEW);
    NotifyMatrixMode(GL_MODELVIEW);
    pLoadIdentity();
    NotifyMatrixEdited();
    pRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    NotifyMatrixEdited();
    pRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    NotifyMatrixEdited();
    pRotatef(-roll, 1.0f, 0.0f, 0.0f);
    NotifyMatrixEdited();
    pRotatef(-pitch, 0.0f, 1.0f, 0.0f);
    NotifyMatrixEdited();
    pRotatef(-yaw, 0.0f, 0.0f, 1.0f);
    NotifyMatrixEdited();
    pTranslatef(-eyeX, -eyeY, -eyeZ);
    NotifyMatrixEdited();
}

}  // namespace

int main() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxModelviewCaptureTestWindow";
    if (!RegisterClassA(&wc)) {
        printf("FAIL: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "modelview_capture_test", WS_OVERLAPPEDWINDOW,
        0, 0, 256, 256, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAIL: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        printf("FAIL: ChoosePixelFormat/SetPixelFormat, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || !wglMakeCurrent(hdc, hglrc)) {
        printf("FAIL: wglCreateContext/wglMakeCurrent, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HMODULE realGl = GetModuleHandleA("opengl32.dll");
    pMatrixMode   = (PfnMatrixMode)GetProcAddress(realGl, "glMatrixMode");
    pLoadIdentity = (PfnLoadIdentity)GetProcAddress(realGl, "glLoadIdentity");
    pFrustum      = (PfnFrustum)GetProcAddress(realGl, "glFrustum");
    pOrtho        = (PfnOrtho)GetProcAddress(realGl, "glOrtho");
    pRotatef      = (PfnRotatef)GetProcAddress(realGl, "glRotatef");
    pTranslatef   = (PfnTranslatef)GetProcAddress(realGl, "glTranslatef");
    pPushMatrix   = (PfnPushMatrix)GetProcAddress(realGl, "glPushMatrix");
    pPopMatrix    = (PfnPopMatrix)GetProcAddress(realGl, "glPopMatrix");
    pGetFloatv    = (PfnGetFloatv)GetProcAddress(realGl, "glGetFloatv");
    if (pMatrixMode == nullptr || pLoadIdentity == nullptr || pFrustum == nullptr ||
        pOrtho == nullptr || pRotatef == nullptr || pTranslatef == nullptr ||
        pPushMatrix == nullptr || pPopMatrix == nullptr || pGetFloatv == nullptr) {
        printf("FAIL: could not resolve the GL 1.1 matrix entry points\n");
        return 1;
    }

    // --- Case 1: nothing captured before any world pass. MUST run first: the module keeps
    // process-global state and deliberately exports no test-only reset, so this is the only
    // point at which "not captured yet" can be observed.
    {
        CameraMatrix camera;
        Check(!GetCapturedCamera(camera),
              "no glFrustum yet: GetCapturedCamera() returns false rather than guessing");
    }

    // --- Case 2: a full R_SetupGL sequence, latched at swap, equals what the driver holds.
    {
        BeginWorldPass();
        SetWorldCamera(10.0f, 45.0f, 0.0f, 100.0f, 200.0f, 30.0f);

        float fromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, fromDriver);

        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        Check(got, "after a world pass: GetCapturedCamera() returns true");
        if (got && !MatricesEqual(camera.m, fromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("driver  ", fromDriver);
        }
        Check(got && MatricesEqual(camera.m, fromDriver),
              "captured matrix is bit-identical to the driver's GL_MODELVIEW_MATRIX");

        AdvanceCameraHistory();
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf(g_failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
