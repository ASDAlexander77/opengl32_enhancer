// See motion_blur.h.
#include <cstdio>

#include "motion_blur.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_TEXTURE1                  = 0x84C1;
const unsigned int GL_TEXTURE2                  = 0x84C2;
const unsigned int GL_TEXTURE3                  = 0x84C3;
const unsigned int GL_RGBA16F                   = 0x881A;
const unsigned int GL_WRITE_ONLY                = 0x88B9;
const unsigned int GL_COMPUTE_SHADER            = 0x91B9;
const unsigned int GL_COMPILE_STATUS            = 0x8B81;
const unsigned int GL_LINK_STATUS               = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT   = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER            = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW              = 0x88E8;
const unsigned int GL_NO_ERROR                  = 0;

// Every numeric constant inside the shader is spelled out as a GLSL literal rather than pasted
// in from a C++ constant - a name that only exists on the host side compiles to nothing here and
// fails at shader-compile time (see ssr.cpp's header comment for the debugging round that cost).
const char* kMotionBlurShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D colorTex;\n"
    "layout(binding = 1) uniform sampler2D depthTex;\n"
    "layout(binding = 2) uniform sampler2D worldTex;\n"
    "layout(binding = 3) uniform sampler2D captureTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform MotionBlurConfigBlock {\n"
    "    vec4 frustum;\n"        // left, right, bottom, top
    "    vec4 params;\n"         // zNear, zFar, strength, maxRadius
    "    mat4 reprojection;\n"   // previous * inverse(current)
    "};\n"
    "float LinearEyeDistance(float rawDepth, float zNear, float zFar) {\n"
    "    float ndc = rawDepth * 2.0 - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    // Lifted verbatim from ssr.cpp, which lifted it from ssao.cpp, so all three stages agree
    // about where a pixel is in space. Keeping left/right/bottom/top separate rather than
    // assuming a symmetric field of view means an off-centre frustum unprojects correctly.
    "vec3 ViewPosFor(vec2 uv, float eyeDist, float zNear) {\n"
    "    float x = (frustum.x + (frustum.y - frustum.x) * uv.x) * (eyeDist / zNear);\n"
    "    float y = (frustum.z + (frustum.w - frustum.z) * uv.y) * (eyeDist / zNear);\n"
    "    return vec3(x, y, -eyeDist);\n"
    "}\n"
    "vec2 UvForViewPos(vec3 p, float zNear) {\n"
    "    float dist = -p.z;\n"
    "    float xn = p.x * zNear / dist;\n"
    "    float yn = p.y * zNear / dist;\n"
    "    return vec2((xn - frustum.x) / (frustum.y - frustum.x),\n"
    "                 (yn - frustum.z) / (frustum.w - frustum.z));\n"
    "}\n"
    // Where the finished frame differs from the pre-HUD frame, the game drew an overlay. The
    // threshold sits just clear of 8-bit quantisation (1/255), which makes this "did the game
    // draw here" rather than a tolerance to tune.
    "bool IsHud(ivec2 c) {\n"
    "    vec3 finished = texelFetch(captureTex, c, 0).rgb;\n"
    "    vec3 world = texelFetch(worldTex, c, 0).rgb;\n"
    "    return any(greaterThan(abs(finished - world), vec3(1.0 / 128.0)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (coord.x >= size.x || coord.y >= size.y) { return; }\n"
    "    vec4 src = texelFetch(colorTex, coord, 0);\n"
    // Every early-out writes src untouched, so a pixel this stage declined to blur is
    // bit-identical to one it never saw.
    "    if (IsHud(coord)) { imageStore(outputImage, coord, src); return; }\n"
    "    float raw = texelFetch(depthTex, coord, 0).r;\n"
    "    vec2 uv = (vec2(coord) + vec2(0.5)) / vec2(size);\n"
    "    vec3 here = ViewPosFor(uv, LinearEyeDistance(raw, params.x, params.y), params.x);\n"
    "    vec3 before = (reprojection * vec4(here, 1.0)).xyz;\n"
    // Behind the previous frame's near plane there is no screen position to measure against,
    // and projecting it anyway would divide by a vanishing distance.
    "    if (before.z > -params.x) { imageStore(outputImage, coord, src); return; }\n"
    "    vec2 velocity = (uv - UvForViewPos(before, params.x)) * params.z;\n"
    "    float len = length(velocity);\n"
    // A cost saving, not a correctness guarantee: a still camera - the common case across a
    // static scene - makes velocity IEEE-754-exact zero, which sends every one of the 7
    // taps below to the same texel as this pixel's own centre regardless of whether this early-out
    // runs, so deleting it changes no output pixel. The same holds for any len just under 1e-6,
    // not only exactly zero: 1e-6 in uv space is roughly 1e-4 px at this file's 128px test width
    // and roughly 2e-3 px even at 1920 wide - far too small to move a tap off the centre texel
    // starting from coord+0.5. That bit-exactness is for an ORDINARY static-scene world pixel:
    // not HUD, and in front of the previous near plane, so neither early-out above this one fires
    // - the tap collapse this comment describes is what actually delivers it, and it IS covered,
    // by motion_blur_test.cpp's "a stationary camera leaves the pixel bit-exact" case. Don't go
    // looking for a test that kills this line instead; there isn't one to find.
    "    if (len < 1e-6) { imageStore(outputImage, coord, src); return; }\n"
    "    if (len > params.w) { velocity *= params.w / len; }\n"
    "    vec3 sum = src.rgb;\n"
    "    float count = 1.0;\n"
    "    for (int i = 1; i < 8; ++i) {\n"
    "        vec2 tapUv = uv - velocity * (float(i) / 7.0);\n"
    "        ivec2 tap = ivec2(clamp(tapUv, vec2(0.0), vec2(1.0)) * vec2(size));\n"
    "        tap = clamp(tap, ivec2(0), size - ivec2(1));\n"
    "        if (IsHud(tap)) { continue; }\n"
    "        sum += texelFetch(colorTex, tap, 0).rgb;\n"
    "        count += 1.0;\n"
    "    }\n"
    "    imageStore(outputImage, coord, vec4(sum / count, src.a));\n"
    "}\n";

struct MotionBlurConfigData {
    float frustum[4];
    float params[4];
    float reprojection[16];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int program = 0;
    unsigned int configUbo = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kMotionBlurShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] motion_blur: FAILED to compile compute shader: %s\n", log);
        gl.glDeleteShader(shader);
        return false;
    }

    unsigned int program = gl.glCreateProgram();
    gl.glAttachShader(program, shader);
    gl.glLinkProgram(program);
    gl.glDeleteShader(shader);

    int linked = 0;
    gl.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048];
        int logLen = 0;
        gl.glGetProgramInfoLog(program, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] motion_blur: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

void EnsureUbo(const GlComputeApi& gl) {
    if (g_state.configUbo != 0) {
        return;
    }
    gl.glGenBuffers(1, &g_state.configUbo);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(MotionBlurConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

void MotionBlurReprojection(const CameraMatrix& current, const CameraMatrix& previous,
                            float out[16]) {
    // Column-major throughout: element (row, col) is m[col * 4 + row]. A view matrix is
    // [R | t] with R the world->view rotation, so R[row][col] == current.m[col * 4 + row] and
    // t[row] == current.m[12 + row].
    //
    // inverse([R | t]) == [R-transpose | -R-transpose * t] because R is orthonormal. Doing it
    // this way rather than with a general 4x4 inversion is not only cheaper, it cannot produce
    // a near-singular result on a matrix that is rigid by construction.
    float inv[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // (R-transpose)[row][col] == R[col][row] == current.m[row * 4 + col]
            inv[col * 4 + row] = current.m[row * 4 + col];
        }
    }
    for (int row = 0; row < 3; ++row) {
        float acc = 0.0f;
        for (int k = 0; k < 3; ++k) {
            acc += current.m[row * 4 + k] * current.m[12 + k];
        }
        inv[12 + row] = -acc;
    }

    // out = previous * inv
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float acc = 0.0f;
            for (int k = 0; k < 4; ++k) {
                acc += previous.m[k * 4 + row] * inv[col * 4 + k];
            }
            out[col * 4 + row] = acc;
        }
    }
}

bool ApplyMotionBlur(unsigned int srcTexture, unsigned int dstTexture,
                     unsigned int depthTexture, unsigned int worldTexture,
                     unsigned int captureTexture,
                     int width, int height, const ProjectionParams& projection,
                     const float reprojection[16],
                     float strength, float maxRadius) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] motion_blur: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (depthTexture == 0 || worldTexture == 0 || captureTexture == 0) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    // A guard clause, not merely a beneficial side effect of the shader's own math: strength=0
    // makes every tap collapse onto the source pixel and the dispatch happens to come out
    // bit-exact, but running it anyway still costs a full compute dispatch every frame for a
    // user who has the stage listed but turned all the way down. Refusing here instead makes
    // motion_blur.h's contract, the design doc's "no-op conditions: all return false", and this
    // code agree - see motion_blur_test.cpp's "strength 0" case, which now checks the return
    // value and that dst is untouched rather than reading a dst this no longer writes.
    if (strength <= 0.0f) {
        return false;
    }

    // Without a captured frustum raw depth cannot be turned into view-space positions - see
    // projection_capture.h. Refusing here rather than guessing near/far keeps a wrong-looking
    // blur from being mistaken for a tuning problem.
    if (projection.zNear <= 0.0f || projection.zFar <= projection.zNear) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] motion_blur: no projection captured yet, effect idle "
                   "until the game establishes a 3D view\n");
            warned = true;
        }
        return false;
    }

    // Cached programs/buffers belong to the GL context that built them. If that context is gone,
    // drop the handles rather than deleting them (the owning context freed them already, and
    // glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = PipelineState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] motion_blur: shader init failed, effect disabled until "
                   "the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    EnsureUbo(gl);

    MotionBlurConfigData configData{};
    configData.frustum[0] = projection.left;
    configData.frustum[1] = projection.right;
    configData.frustum[2] = projection.bottom;
    configData.frustum[3] = projection.top;
    configData.params[0] = projection.zNear;
    configData.params[1] = projection.zFar;
    configData.params[2] = strength;
    configData.params[3] = maxRadius;
    for (int i = 0; i < 16; ++i) {
        configData.reprojection[i] = reprojection[i];
    }
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(MotionBlurConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);     gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE1);     gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glActiveTexture(GL_TEXTURE2);     gl.glBindTexture(GL_TEXTURE_2D, worldTexture);
    gl.glActiveTexture(GL_TEXTURE3);     gl.glBindTexture(GL_TEXTURE_2D, captureTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] motion_blur: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
