// See vignette.h. Single compute dispatch plus a small UBO for `intensity`/`radius`. No
// capture/blit/state-save of its own - see post_effects.cpp for the shared pipeline that owns
// srcTexture/dstTexture and the app's GL state around the whole chain of stages this is one
// link in.
#include <cstdio>

#include "vignette.h"
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

// binding=0 on inputTex (a texture-unit binding) and binding=0 on VignetteConfigBlock (a
// uniform-buffer binding point) are different GL namespaces - see hdr_look.cpp's equivalent
// comment. Not a collision.
const char* kVignetteShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform VignetteConfigBlock {\n"
    "    float intensity;\n"
    "    float radius;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    // Offset from the frame center, with x scaled by the aspect ratio so the falloff stays a
    // circle on a wide frame instead of stretching into an ellipse, then normalized by the
    // center-to-corner distance: dist is 0 at the center and exactly 1 in the corners
    // regardless of resolution, which is what lets `radius` be a resolution-independent 0..1.
    "    float aspect = float(outSize.x) / float(outSize.y);\n"
    "    vec2 d = (uv - vec2(0.5)) * vec2(aspect, 1.0);\n"
    "    float dist = length(d) / length(vec2(0.5 * aspect, 0.5));\n"
    // smoothstep is 0 inside `radius` (so the center is left bit-exact) and ramps to 1 at the
    // corner. At intensity=0 the scale is always exactly 1.0, making intensity=0 an exact
    // no-op for every pixel, not just the center.
    "    float falloff = smoothstep(radius, 1.0, dist);\n"
    "    float scale = 1.0 - intensity * falloff;\n"
    "    imageStore(outputImage, outCoord, vec4(color.rgb * scale, color.a));\n"
    "}\n";

struct VignetteConfigData {
    float intensity;
    float radius;
    float pad[2];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;
    unsigned int configUbo = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kVignetteShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] vignette: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] vignette: FAILED to link compute program: %s\n", log);
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(VignetteConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplyVignette(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                   float intensity, float radius) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] vignette: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] vignette: shader init failed, effect disabled for the "
                   "rest of this process\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureUbo(gl);

    VignetteConfigData configData{};
    configData.intensity = intensity;
    // smoothstep()'s result is undefined when edge0 >= edge1, so keep radius strictly below
    // the 1.0 outer edge even if the caller passes exactly 1.0 (the config clamp's upper
    // bound, which would otherwise be a legal value that produces garbage).
    configData.radius = radius < 0.999f ? radius : 0.999f;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(VignetteConfigData), &configData, GL_DYNAMIC_DRAW);
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
        printf("[opengl32_enh_cpp] vignette: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
