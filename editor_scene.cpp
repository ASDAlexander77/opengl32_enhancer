// See editor_scene.h.
#include <windows.h>
#include <GL/gl.h>

#include <cmath>
#include <cstdio>

#include "editor_scene.h"
#include "frame_dump.h"
#include "gl_loader.h"
#include "projection_capture.h"

namespace {

const unsigned int GL_DEPTH_COMPONENT24_ = 0x81A6;
const unsigned int GL_FRAMEBUFFER_       = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER_  = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER_  = 0x8CA9;
const unsigned int GL_COLOR_ATTACHMENT0_ = 0x8CE0;
const unsigned int GL_DEPTH_ATTACHMENT_  = 0x8D00;
const unsigned int GL_CLAMP_TO_EDGE_     = 0x812F;
const unsigned int GL_RGBA8_             = 0x8058;

struct LoadedFrame {
    bool valid = false;
    int width = 0;
    int height = 0;
    bool hasProjection = false;
    ProjectionParams projection;
    unsigned int colorTexture = 0;
    unsigned int depthTexture = 0;
    unsigned int fbo = 0;
};

LoadedFrame g_frame;

// Registers a frustum with BOTH OpenGL and projection_capture.h. In the game the wrapper's
// glFrustum hook keeps those two in sync automatically; the editor links the real opengl32
// directly, so nothing intercepts its calls and it has to tell projection_capture itself.
void SetFrustum(double left, double right, double bottom, double top, double zNear, double zFar) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(left, right, bottom, top, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    CaptureProjectionFrustum(left, right, bottom, top, zNear, zFar);
}

void DrawQuad(float shade,
               float x0, float y0, float z0, float x1, float y1, float z1,
               float x2, float y2, float z2, float x3, float y3, float z3) {
    glColor3f(shade, shade, shade);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x2, y2, z2);
    glVertex3f(x3, y3, z3);
    glEnd();
}

// An axis-aligned box, each face given a different flat shade so the scene reads as lit without
// any actual lighting. The box's contact with the floor and its proximity to the walls are what
// give SSAO creases to find.
void DrawBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, float shade) {
    DrawQuad(shade * 1.00f, minX, maxY, minZ, maxX, maxY, minZ, maxX, maxY, maxZ, minX, maxY, maxZ);  // top
    DrawQuad(shade * 0.75f, minX, minY, maxZ, maxX, minY, maxZ, maxX, maxY, maxZ, minX, maxY, maxZ);  // front
    DrawQuad(shade * 0.60f, minX, minY, minZ, minX, minY, maxZ, minX, maxY, maxZ, minX, maxY, minZ);  // left
    DrawQuad(shade * 0.60f, maxX, minY, minZ, maxX, maxY, minZ, maxX, maxY, maxZ, maxX, minY, maxZ);  // right
    DrawQuad(shade * 0.45f, minX, minY, minZ, maxX, minY, minZ, maxX, maxY, minZ, minX, maxY, minZ);  // back
}

}  // namespace

void RenderSyntheticScene(int windowWidth, int windowHeight) {
    if (windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_TEXTURE_2D);
    glShadeModel(GL_FLAT);

    const double zNear = 4.0, zFar = 4096.0;
    const double halfHeight = zNear * 0.57735;  // tan(30 degrees) - a 60-degree vertical FOV
    const double halfWidth = halfHeight * ((double)windowWidth / (double)windowHeight);
    SetFrustum(-halfWidth, halfWidth, -halfHeight, halfHeight, zNear, zFar);

    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Room extents, in Quake II-ish units (~1 inch each): a corridor 320 wide and 160 tall
    // running away from the camera, which sits at the origin looking down -z.
    const float floorY = -64.0f, ceilY = 96.0f;
    const float leftX = -160.0f, rightX = 160.0f;
    const float backZ = -512.0f, frontZ = -32.0f;

    // Checkerboard floor: real high-frequency detail for the sharpeners and the AA stages to
    // act on. A flat floor would let a sharpen stage look like a no-op.
    const float tile = 32.0f;
    for (float z = frontZ; z > backZ; z -= tile) {
        for (float x = leftX; x < rightX; x += tile) {
            int ix = (int)floorf((x - leftX) / tile);
            int iz = (int)floorf((frontZ - z) / tile);
            float shade = ((ix + iz) % 2 == 0) ? 0.62f : 0.30f;
            DrawQuad(shade, x, floorY, z, x + tile, floorY, z, x + tile, floorY, z - tile, x, floorY, z - tile);
        }
    }

    DrawQuad(0.22f, leftX, ceilY, backZ, rightX, ceilY, backZ, rightX, ceilY, frontZ, leftX, ceilY, frontZ);
    DrawQuad(0.40f, leftX, floorY, backZ, leftX, floorY, frontZ, leftX, ceilY, frontZ, leftX, ceilY, backZ);
    DrawQuad(0.40f, rightX, floorY, backZ, rightX, ceilY, backZ, rightX, ceilY, frontZ, rightX, floorY, frontZ);
    DrawQuad(0.50f, leftX, floorY, backZ, rightX, floorY, backZ, rightX, ceilY, backZ, leftX, ceilY, backZ);

    // Boxes at a range of depths, several deliberately touching a wall or each other so there
    // are real contact points and inside corners for SSAO rather than only isolated silhouettes.
    DrawBox(-150.0f, floorY, -180.0f, -80.0f, floorY + 72.0f, -110.0f, 0.70f);
    DrawBox(-150.0f, floorY, -110.0f, -110.0f, floorY + 36.0f, -70.0f, 0.62f);
    DrawBox(40.0f, floorY, -260.0f, 150.0f, floorY + 120.0f, -150.0f, 0.66f);
    DrawBox(-40.0f, floorY, -420.0f, 30.0f, floorY + 48.0f, -350.0f, 0.58f);
    DrawBox(70.0f, floorY, -460.0f, 150.0f, floorY + 200.0f, -380.0f, 0.54f);

    // A near-white panel well above the default bloomThreshold of 0.8, so bloom has a genuine
    // highlight to bloom rather than nothing at all.
    DrawQuad(0.97f, -60.0f, 20.0f, backZ + 1.0f, 10.0f, 20.0f, backZ + 1.0f,
              10.0f, 70.0f, backZ + 1.0f, -60.0f, 70.0f, backZ + 1.0f);
}

bool LoadFrameDump(const char* path) {
    FrameDumpHeader header{};
    unsigned char* color = nullptr;
    float* depth = nullptr;
    if (!ReadFrameDump(path, header, &color, &depth)) {
        return false;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        printf("editor_scene: no GL 4.3 compute support, cannot upload a frame dump\n");
        FreeFrameDump(color, depth);
        return false;
    }

    // A reload at a different size needs fresh immutable storage - glTexStorage2D cannot be
    // re-specified, so the old textures go rather than being reused.
    if (g_frame.valid) {
        unsigned int textures[2] = {g_frame.colorTexture, g_frame.depthTexture};
        gl.glDeleteTextures(2, textures);
        g_frame.colorTexture = 0;
        g_frame.depthTexture = 0;
        g_frame.valid = false;
    }

    gl.glGenTextures(1, &g_frame.colorTexture);
    gl.glBindTexture(GL_TEXTURE_2D, g_frame.colorTexture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8_, header.width, header.height);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, header.width, header.height, GL_RGBA, GL_UNSIGNED_BYTE, color);

    gl.glGenTextures(1, &g_frame.depthTexture);
    gl.glBindTexture(GL_TEXTURE_2D, g_frame.depthTexture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24_, header.width, header.height);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, header.width, header.height, GL_DEPTH_COMPONENT, GL_FLOAT, depth);

    if (g_frame.fbo == 0) {
        gl.glGenFramebuffers(1, &g_frame.fbo);
    }
    gl.glBindFramebuffer(GL_FRAMEBUFFER_, g_frame.fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_TEXTURE_2D, g_frame.colorTexture, 0);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER_, GL_DEPTH_ATTACHMENT_, GL_TEXTURE_2D, g_frame.depthTexture, 0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER_, 0);

    FreeFrameDump(color, depth);

    unsigned int err = gl.glGetError();
    if (err != 0) {
        printf("editor_scene: glGetError() = 0x%04X uploading the frame dump\n", err);
    }

    g_frame.width = header.width;
    g_frame.height = header.height;
    g_frame.hasProjection = header.hasProjection != 0;
    g_frame.projection = header.projection;
    g_frame.valid = true;
    printf("editor_scene: loaded '%s' (%dx%d, projection=%s)\n",
           path, header.width, header.height, g_frame.hasProjection ? "yes" : "no");
    return true;
}

bool HasLoadedFrame() {
    return g_frame.valid;
}

bool LoadedFrameHasProjection() {
    return g_frame.valid && g_frame.hasProjection;
}

void LoadedFrameSize(int& outWidth, int& outHeight) {
    outWidth = g_frame.valid ? g_frame.width : 0;
    outHeight = g_frame.valid ? g_frame.height : 0;
}

void RenderLoadedFrame(int windowWidth, int windowHeight) {
    if (!g_frame.valid || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        return;
    }

    float scale = (float)windowWidth / (float)g_frame.width;
    float vScale = (float)windowHeight / (float)g_frame.height;
    if (vScale < scale) {
        scale = vScale;
    }
    int previewWidth = (int)((float)g_frame.width * scale);
    int previewHeight = (int)((float)g_frame.height * scale);
    if (previewWidth <= 0 || previewHeight <= 0) {
        return;
    }

    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClearDepth(1.0);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Color and depth together in one blit. NEAREST is mandatory once GL_DEPTH_BUFFER_BIT is in
    // the mask, and is what we want for color here anyway - this is a 1:1-ish preview of a real
    // frame, so filtering it would misrepresent what the stages actually see.
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER_, g_frame.fbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER_, 0);
    gl.glBlitFramebuffer(0, 0, g_frame.width, g_frame.height,
                          0, 0, previewWidth, previewHeight,
                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER_, 0);

    // The viewport has to match the region the frame actually occupies, because that is what
    // ApplySelectedEffect() reads to decide how much of the back buffer to capture - and it
    // always captures from the origin.
    glViewport(0, 0, previewWidth, previewHeight);

    if (g_frame.hasProjection) {
        CaptureProjectionFrustum(g_frame.projection.left, g_frame.projection.right,
                                  g_frame.projection.bottom, g_frame.projection.top,
                                  g_frame.projection.zNear, g_frame.projection.zFar);
    }
}
