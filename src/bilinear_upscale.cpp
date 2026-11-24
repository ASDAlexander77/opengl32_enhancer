// See bilinear_upscale.h. Single compute dispatch, no capture/blit/state-save of its own -
// see post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture and the app's
// GL state around the whole chain of stages this is one link in.
#include <cstdio>

#include "bilinear_upscale.h"
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
const unsigned int GL_DYNAMIC_DRAW               = 0x88E8;
const unsigned int GL_NO_ERROR                  = 0;

// Resamples the source as if it had been rendered at `scale` of the current resolution, then
// bilinearly reconstructs full size. `scale` describes a virtual lower-resolution grid laid
// over the (full-resolution) source, the same convention fsr.cpp and nis_effect.cpp use, so
// the three upscalers are directly comparable at a given scale.
//
// The four virtual-grid samples are interpolated explicitly rather than leaning on GL_LINEAR:
// hardware filtering interpolates between SOURCE texels, which at scale < 1 is not the same
// thing as interpolating between the coarser virtual texels we are pretending were rendered.
// At scale == 1.0 the virtual grid IS the source grid and this reduces to an exact
// texel-center copy. binding=0 matches the texture unit ApplyBilinearUpscale() below binds
// srcTexture to; binding=1 matches the image unit dstTexture is bound to.
const char* kComputeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(std140, binding = 0) uniform BilinearConfigBlock {\n"
    "    vec4 params;\n"   // .x = scale, .yzw unused (std140 padding)
    "};\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) {\n"
    "        return;\n"
    "    }\n"
    "    vec2 vSize = max(floor(vec2(outSize) * params.x), vec2(1.0));\n"
    "    vec2 p = ((vec2(outCoord) + vec2(0.5)) / vec2(outSize)) * vSize - vec2(0.5);\n"
    "    vec2 base = floor(p);\n"
    "    vec2 frac = p - base;\n"
    "    vec4 c00 = textureLod(inputTex, (base + vec2(0.5, 0.5)) / vSize, 0.0);\n"
    "    vec4 c10 = textureLod(inputTex, (base + vec2(1.5, 0.5)) / vSize, 0.0);\n"
    "    vec4 c01 = textureLod(inputTex, (base + vec2(0.5, 1.5)) / vSize, 0.0);\n"
    "    vec4 c11 = textureLod(inputTex, (base + vec2(1.5, 1.5)) / vSize, 0.0);\n"
    "    vec4 color = mix(mix(c00, c10, frac.x), mix(c01, c11, frac.x), frac.y);\n"
    "    imageStore(outputImage, outCoord, color);\n"
    "}\n";

// Matches the std140 BilinearConfigBlock above.
struct BilinearConfigData {
    float params[4];
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
    gl.glShaderSource(shader, 1, &kComputeShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[1024];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] bilinear_upscale: FAILED to compile compute shader: %s\n", log);
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
        char log[1024];
        int logLen = 0;
        gl.glGetProgramInfoLog(program, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] bilinear_upscale: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

}  // namespace

bool ApplyBilinearUpscale(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                           float scale) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] bilinear_upscale: GL 4.3 compute support unavailable "
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
        if (g_state.initOk && g_state.configUbo == 0) {
            gl.glGenBuffers(1, &g_state.configUbo);
            gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
            gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BilinearConfigData), nullptr, GL_DYNAMIC_DRAW);
        }
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] bilinear_upscale: shader init failed, effect disabled "
                   "until the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    // Clamped to the documented range: above 1.0 the virtual grid would be finer than the
    // source and invent detail that is not there, and at/below 0 the reciprocals blow up.
    float clampedScale = scale;
    if (clampedScale < 0.05f) { clampedScale = 0.05f; }
    if (clampedScale > 1.0f)  { clampedScale = 1.0f; }

    BilinearConfigData configData{};
    configData.params[0] = clampedScale;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BilinearConfigData), &configData, GL_DYNAMIC_DRAW);
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
        printf("[opengl32_enh_cpp] bilinear_upscale: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
