// See pixel_invert.h. Single compute dispatch, no capture/blit/state-save of its own - see
// post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture and the app's GL
// state around the whole chain of stages this is one link in.
#include <cstdio>

#include "pixel_invert.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D              = 0x0DE1;
const unsigned int GL_TEXTURE0                = 0x84C0;
const unsigned int GL_RGBA16F                 = 0x881A;
const unsigned int GL_WRITE_ONLY              = 0x88B9;
const unsigned int GL_COMPUTE_SHADER          = 0x91B9;
const unsigned int GL_COMPILE_STATUS          = 0x8B81;
const unsigned int GL_LINK_STATUS             = 0x8B82;
const unsigned int GL_TEXTURE_FETCH_BARRIER_BIT = 0x00000008;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT = 0x00000400;
const unsigned int GL_NO_ERROR                = 0;

const char* kInvertShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    imageStore(outputImage, outCoord, vec4(1.0 - color.rgb, color.a));\n"
    "}\n";

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kInvertShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] pixel_invert: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] pixel_invert: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

}  // namespace

bool ApplyInvert(unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] pixel_invert: GL 4.3 compute support unavailable on "
                   "this context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] pixel_invert: shader init failed, effect disabled for "
                   "the rest of this process\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glBindImageTexture(1, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    // dstTexture may next be read by another stage's sampler (GL_TEXTURE_FETCH_BARRIER_BIT)
    // or blitted from as the pipeline's final output (GL_FRAMEBUFFER_BARRIER_BIT) - this
    // effect doesn't know which, so it covers both.
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] pixel_invert: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
