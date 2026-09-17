// See depth_vignette.h. Single compute dispatch plus a small UBO for `intensity`/`threshold` -
// structurally identical to vignette.cpp, but the falloff term comes from a sampled depth
// texture instead of screen-space distance from center. No capture/blit/state-save of its own -
// see post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture/depthTexture and
// the app's GL state around the whole chain of stages this is one link in.
#include <cstdio>

#include "depth_vignette.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_TEXTURE1                  = 0x84C1;
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

// inputTex (texture unit 0) and depthTex (texture unit 1) are separate texture-unit bindings;
// outputImage's image-unit binding=0 and DepthVignetteConfigBlock's uniform-buffer binding=0
// are different GL namespaces again from both - see vignette.cpp's equivalent comment. None of
// these collide with each other.
const char* kDepthVignetteShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(binding = 1) uniform sampler2D depthTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform DepthVignetteConfigBlock {\n"
    "    float intensity;\n"
    "    float threshold;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    float depth = texture(depthTex, uv).r;\n"
    // smoothstep is 0 below `threshold` (nearby geometry left bit-exact) and ramps to 1 at
    // depth=1.0 (the far plane). At intensity=0 the scale is always exactly 1.0, making
    // intensity=0 an exact no-op for every pixel regardless of depth.
    "    float falloff = smoothstep(threshold, 1.0, depth);\n"
    "    float scale = 1.0 - intensity * falloff;\n"
    "    imageStore(outputImage, outCoord, vec4(color.rgb * scale, color.a));\n"
    "}\n";

struct DepthVignetteConfigData {
    float intensity;
    float threshold;
    float pad[2];
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
    gl.glShaderSource(shader, 1, &kDepthVignetteShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] depthvignette: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] depthvignette: FAILED to link compute program: %s\n", log);
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(DepthVignetteConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplyDepthVignette(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
                         int width, int height, float intensity, float threshold) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] depthvignette: GL 4.3 compute support unavailable on "
                   "this context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (depthTexture == 0) {
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
            printf("[opengl32_enh_cpp] depthvignette: shader init failed, effect disabled "
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

    DepthVignetteConfigData configData{};
    configData.intensity = intensity;
    // smoothstep()'s result is undefined when edge0 >= edge1, so keep threshold strictly below
    // the 1.0 far-plane edge even if the caller passes exactly 1.0 (the config clamp's upper
    // bound, which would otherwise be a legal value that produces garbage).
    configData.threshold = threshold < 0.999f ? threshold : 0.999f;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(DepthVignetteConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE1);
    gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] depthvignette: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
