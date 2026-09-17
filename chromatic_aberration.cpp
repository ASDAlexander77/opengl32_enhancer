// See chromatic_aberration.h. Single compute dispatch plus a one-float UBO for `strength`. No
// capture/blit/state-save of its own - see post_effects.cpp for the shared pipeline that owns
// srcTexture/dstTexture and the app's GL state around the whole chain of stages this is one
// link in.
#include <cstdio>

#include "chromatic_aberration.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
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

// binding=0 on inputTex (a texture-unit binding) and binding=0 on CaConfigBlock (a
// uniform-buffer binding point) are different GL namespaces - see hdr_look.cpp's equivalent
// comment. Not a collision.
//
// kMaxShift is the UV-space offset at strength=1 before the radial falloff is applied; the
// corner (where dir is (0.5,0.5) and r2 is 2.0) ends up around 0.85% of the frame width, a
// few pixels at 1080p. It lives in the shader rather than the config so `strength` stays a
// resolution- and taste-independent 0..1 knob.
const char* kChromaticAberrationShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform CaConfigBlock {\n"
    "    float strength;\n"
    "};\n"
    "const float kMaxShift = 0.003;\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    // Radial direction and a quadratic falloff: dot(dir,dir)*4 is 0 at the center and 2 in
    // the corners, so the fringe stays invisible in the middle of the frame and grows toward
    // the edges the way a real lens' lateral aberration does. At strength=0 the offset is
    // exactly vec2(0), so all three channels sample the same texel and the pass is a bit-
    // exact passthrough.
    "    vec2 dir = uv - vec2(0.5);\n"
    "    float r2 = dot(dir, dir) * 4.0;\n"
    "    vec2 offset = dir * (strength * kMaxShift * r2);\n"
    // Green stays put and carries the alpha; red is pulled outward and blue inward, which is
    // what puts complementary red/cyan fringes on opposite sides of a high-contrast edge.
    "    vec4 center = texture(inputTex, uv);\n"
    "    float r = texture(inputTex, uv + offset).r;\n"
    "    float b = texture(inputTex, uv - offset).b;\n"
    "    imageStore(outputImage, outCoord, vec4(r, center.g, b, center.a));\n"
    "}\n";

struct CaConfigData {
    float strength;
    float pad[3];
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
    gl.glShaderSource(shader, 1, &kChromaticAberrationShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] chromatic_aberration: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] chromatic_aberration: FAILED to link compute program: %s\n", log);
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CaConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplyChromaticAberration(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                              float strength) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] chromatic_aberration: GL 4.3 compute support unavailable "
                   "on this context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    // Cached programs/textures belong to the GL context that built them. If that context is
    // gone, drop the handles rather than deleting them (the owning context freed them already,
    // and glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = PipelineState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] chromatic_aberration: shader init failed, effect disabled "
                   "until the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureUbo(gl);

    CaConfigData configData{};
    configData.strength = strength;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CaConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(1, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] chromatic_aberration: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
