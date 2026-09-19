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

// Quake II's R_SetGL2D: the HUD pass. Note the order - the projection is switched to ortho
// BEFORE the modelview is touched, which is exactly what makes glOrtho a safe latch point.
void BeginHudPass() {
    pMatrixMode(GL_PROJECTION);
    NotifyMatrixMode(GL_PROJECTION);
    pLoadIdentity();
    NotifyMatrixEdited();
    pOrtho(0.0, 256.0, 256.0, 0.0, -99999.0, 99999.0);
    NotifyTwoDProjection();
    pMatrixMode(GL_MODELVIEW);
    NotifyMatrixMode(GL_MODELVIEW);
    pLoadIdentity();
    NotifyMatrixEdited();
}

// R_RotateForEntity, as every Quake II entity path issues it: inside a push/pop pair.
void DrawEntity(float x, float y, float z, float yaw) {
    pPushMatrix();
    NotifyMatrixPush();
    pTranslatef(x, y, z);
    NotifyMatrixEdited();
    pRotatef(yaw, 0.0f, 0.0f, 1.0f);
    NotifyMatrixEdited();
    pPopMatrix();
    NotifyMatrixPop();
}

bool NearlyEqual(float a, float b, float tolerance) {
    float d = a - b;
    return (d < 0.0f ? -d : d) <= tolerance;
}

bool Vec3NearlyEqual(const float* v, float x, float y, float z, float tolerance) {
    return NearlyEqual(v[0], x, tolerance) && NearlyEqual(v[1], y, tolerance) &&
           NearlyEqual(v[2], z, tolerance);
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

    // --- Case 3: the camera is taken AT the first entity push, so a depth-0 modelview edit
    // afterwards - a viewmodel or an alpha pass setting up its own transform - cannot replace
    // it. Asserting only that a balanced push/pop leaves the camera alone would prove nothing:
    // the pop restores it either way, so that passes with no gate at all.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 10.0f, 20.0f, 30.0f);

        float cameraFromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, cameraFromDriver);

        DrawEntity(500.0f, 600.0f, 700.0f, 45.0f);   // the camera is latched here

        pTranslatef(1000.0f, 0.0f, 0.0f);            // depth 0, after the latch
        NotifyMatrixEdited();
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, cameraFromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("camera  ", cameraFromDriver);
        }
        Check(got && MatricesEqual(camera.m, cameraFromDriver),
              "camera is latched at the first entity push, not at swap");

        AdvanceCameraHistory();
    }

    // --- Case 4: the HUD pass must not become the camera. It runs LAST in a real frame, so
    // "the most recent modelview at swap time" is precisely the wrong answer.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 180.0f, 0.0f, -40.0f, 15.0f, 8.0f);

        float cameraFromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, cameraFromDriver);

        BeginHudPass();
        pTranslatef(32.0f, 32.0f, 0.0f);
        NotifyMatrixEdited();
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, cameraFromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("camera  ", cameraFromDriver);
        }
        Check(got && MatricesEqual(camera.m, cameraFromDriver),
              "HUD ortho pass does not overwrite the camera");

        AdvanceCameraHistory();
    }

    // --- Case 5: a push/pop pair BEFORE the camera sequence must not latch early. Nothing is
    // pending at that point, so there is nothing to take, and the camera established afterwards
    // is still the one captured.
    {
        BeginWorldPass();
        pMatrixMode(GL_MODELVIEW);
        NotifyMatrixMode(GL_MODELVIEW);
        DrawEntity(1.0f, 2.0f, 3.0f, 90.0f);

        SetWorldCamera(5.0f, 270.0f, 0.0f, 77.0f, 88.0f, 99.0f);

        float cameraFromDriver[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, cameraFromDriver);

        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, cameraFromDriver)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("camera  ", cameraFromDriver);
        }
        Check(got && MatricesEqual(camera.m, cameraFromDriver),
              "a push/pop pair before the camera sequence does not latch early");

        AdvanceCameraHistory();
    }

    // --- Case 6: the disarm itself. A frame whose world pass is armed but where the HUD begins
    // before any camera is set - the engine drew no world geometry that frame - must not let the
    // HUD's modelview become the camera. Nothing is pending at the glOrtho, so the latch there is
    // a no-op, and g_armed = false is the only thing standing between the HUD and the capture.
    {
        // A normal frame first, so there is a known camera to preserve.
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 7.0f, 8.0f, 9.0f);
        float knownCamera[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, knownCamera);
        FinalizeCameraForFrame();
        AdvanceCameraHistory();

        // Now a frame that arms a world pass and goes straight to the HUD.
        BeginWorldPass();
        BeginHudPass();
        pTranslatef(64.0f, 64.0f, 0.0f);
        NotifyMatrixEdited();
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, knownCamera)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("expected", knownCamera);
        }
        Check(got && MatricesEqual(camera.m, knownCamera),
              "glOrtho disarms: a HUD pass with no camera pending cannot become the camera");

        AdvanceCameraHistory();
    }

    // --- Case 7: history. During a frame, "previous" must be the LAST frame's camera - which
    // is the whole point of keeping it, and what reprojection will consume.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f);
        float frameOne[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, frameOne);
        FinalizeCameraForFrame();
        AdvanceCameraHistory();

        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 11.0f, 22.0f, 33.0f);
        float frameTwo[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, frameTwo);
        FinalizeCameraForFrame();

        CameraMatrix current, previous;
        bool gotCurrent = GetCapturedCamera(current);
        bool gotPrevious = GetPreviousCamera(previous);
        Check(gotCurrent && MatricesEqual(current.m, frameTwo),
              "GetCapturedCamera() returns this frame's camera");
        Check(gotPrevious && MatricesEqual(previous.m, frameOne),
              "GetPreviousCamera() returns the previous frame's camera, not this one's");

        AdvanceCameraHistory();
    }

    // --- Case 8: a second glFrustum within one frame - a viewmodel at its own FOV, a scope -
    // is a sub-pass of the same camera and must not replace what was already latched.
    {
        BeginWorldPass();
        SetWorldCamera(0.0f, 0.0f, 0.0f, 4.0f, 5.0f, 6.0f);
        float worldCamera[16] = {};
        pGetFloatv(GL_MODELVIEW_MATRIX, worldCamera);
        DrawEntity(0.0f, 0.0f, 0.0f, 0.0f);   // latches here

        BeginWorldPass();                      // a second frustum, same frame
        SetWorldCamera(0.0f, 90.0f, 0.0f, 900.0f, 900.0f, 900.0f);
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        if (got && !MatricesEqual(camera.m, worldCamera)) {
            PrintMatrix("captured", camera.m);
            PrintMatrix("expected", worldCamera);
        }
        Check(got && MatricesEqual(camera.m, worldCamera),
              "a second glFrustum in the same frame does not replace the latched camera");

        AdvanceCameraHistory();
    }

    // --- Case 9: decomposition. The identity view matrix is the unambiguous case, and the
    // Quake II sequence is the real one: whatever rotations precede it, the recovered world
    // position must be the eye the engine translated by.
    {
        CameraMatrix identity;
        CameraPose pose;
        DecomposeCamera(identity, pose);
        Check(Vec3NearlyEqual(pose.position, 0.0f, 0.0f, 0.0f, 1e-4f),
              "DecomposeCamera: identity view matrix is at the world origin");
        Check(Vec3NearlyEqual(pose.right, 1.0f, 0.0f, 0.0f, 1e-4f) &&
              Vec3NearlyEqual(pose.up, 0.0f, 1.0f, 0.0f, 1e-4f) &&
              Vec3NearlyEqual(pose.forward, 0.0f, 0.0f, -1.0f, 1e-4f),
              "DecomposeCamera: identity view matrix looks down -Z with +Y up");

        BeginWorldPass();
        SetWorldCamera(12.0f, 34.0f, 0.0f, 128.0f, -64.0f, 48.0f);
        FinalizeCameraForFrame();

        CameraMatrix camera;
        bool got = GetCapturedCamera(camera);
        DecomposeCamera(camera, pose);
        if (got) {
            printf("  recovered eye = %.3f/%.3f/%.3f (expected 128.000/-64.000/48.000)\n",
                   pose.position[0], pose.position[1], pose.position[2]);
        }
        Check(got && Vec3NearlyEqual(pose.position, 128.0f, -64.0f, 48.0f, 1e-2f),
              "DecomposeCamera: recovers the eye position from a real R_SetupGL matrix");

        // The eye check above pins only the translation half, and the identity check pins nothing
        // a transposed basis would fail - the identity matrix is symmetric. So cross-check the
        // basis against the captured matrix itself, on an asymmetric camera: transforming a world
        // point BY the matrix must agree with projecting that point onto the decomposed axes. A
        // row/column transposition, or a sign error in `forward`, breaks this and nothing else in
        // the suite would notice.
        if (got) {
            const float px = 200.0f, py = 50.0f, pz = 10.0f;
            const float* m = camera.m;
            const float viaMatrixX = m[0] * px + m[4] * py + m[8]  * pz + m[12];
            const float viaMatrixY = m[1] * px + m[5] * py + m[9]  * pz + m[13];
            const float viaMatrixZ = m[2] * px + m[6] * py + m[10] * pz + m[14];

            const float dx = px - pose.position[0];
            const float dy = py - pose.position[1];
            const float dz = pz - pose.position[2];
            const float viaPoseX =   pose.right[0]   * dx + pose.right[1]   * dy + pose.right[2]   * dz;
            const float viaPoseY =   pose.up[0]      * dx + pose.up[1]      * dy + pose.up[2]      * dz;
            const float viaPoseZ = -(pose.forward[0] * dx + pose.forward[1] * dy + pose.forward[2] * dz);

            printf("  world point via matrix = %.3f/%.3f/%.3f, via pose = %.3f/%.3f/%.3f\n",
                   viaMatrixX, viaMatrixY, viaMatrixZ, viaPoseX, viaPoseY, viaPoseZ);
            Check(NearlyEqual(viaMatrixX, viaPoseX, 5e-2f) &&
                  NearlyEqual(viaMatrixY, viaPoseY, 5e-2f) &&
                  NearlyEqual(viaMatrixZ, viaPoseZ, 5e-2f),
                  "DecomposeCamera: the recovered basis agrees with the matrix on an asymmetric camera");
        }

        AdvanceCameraHistory();
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    printf(g_failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
