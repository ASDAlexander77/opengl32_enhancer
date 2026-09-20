// See srgb_convert.h. Two single compute dispatches, no capture/blit/state-save of their own -
// see post_effects.cpp for the shared pipeline that owns srcTexture/dstTexture and the app's GL
// state around the whole chain of stages these bracket.
#include <cstdio>

#include "srgb_convert.h"
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

const char* kDecodeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    // The exact piecewise sRGB EOTF. mix() with a bvec selects per channel, so all three
    // channels take their own branch without any actual branching.
    "vec3 SrgbToLinear(vec3 s) {\n"
    "    vec3 lo = s / 12.92;\n"
    "    vec3 hi = pow((s + 0.055) / 1.055, vec3(2.4));\n"
    "    return mix(hi, lo, lessThanEqual(s, vec3(0.04045)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    // max() before the curve: pow() of a negative is NaN, and one NaN here propagates
    // through every later stage and out to the screen.
    "    imageStore(outputImage, outCoord, vec4(SrgbToLinear(max(color.rgb, vec3(0.0))), color.a));\n"
    "}\n";

const char* kEncodeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D outputImage;\n"
    "vec3 LinearToSrgb(vec3 l) {\n"
    "    vec3 lo = l * 12.92;\n"
    "    vec3 hi = 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055;\n"
    "    return mix(hi, lo, lessThanEqual(l, vec3(0.0031308)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    imageStore(outputImage, outCoord, vec4(LinearToSrgb(max(color.rgb, vec3(0.0))), color.a));\n"
    "}\n";

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int program = 0;
};

PipelineState g_decode;
PipelineState g_encode;

bool CompileAndLink(const GlComputeApi& gl, const char* shaderSource, const char* name,
                    unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &shaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] %s: FAILED to compile compute shader: %s\n", name, log);
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
        printf("[opengl32_enh_cpp] %s: FAILED to link compute program: %s\n", name, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

// Shared body for both directions: rebuild against the current context if needed, compile on
// first use, dispatch one compute pass over dstTexture's full extent, and barrier before
// returning. state/shaderSource/name select the direction (decode vs encode); everything else
// is identical to pixel_invert.cpp's single pass.
bool RunConvertPass(PipelineState& state, const char* shaderSource, const char* name,
                    unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] %s: GL 4.3 compute support unavailable on "
                   "this context, effect disabled\n", name);
            warned = true;
        }
        return false;
    }

    // Cached programs/textures belong to the GL context that built them. If that context is
    // gone, drop the handles rather than deleting them (the owning context freed them already,
    // and glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (state.generation != GetGlContextGeneration()) {
        state = PipelineState{};
        state.generation = GetGlContextGeneration();
    }

    if (!state.initTried) {
        state.initTried = true;
        state.initOk = CompileAndLink(gl, shaderSource, name, state.program);
        if (!state.initOk) {
            printf("[opengl32_enh_cpp] %s: shader init failed, effect disabled "
                   "until the GL context changes\n", name);
        }
    }
    if (!state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    gl.glUseProgram(state.program);
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
        printf("[opengl32_enh_cpp] %s: glGetError() = 0x%04X after dispatch\n", name, err);
    }
    return true;
}

}  // namespace

bool ApplySrgbDecode(unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    return RunConvertPass(g_decode, kDecodeShaderSource, "srgb_convert(decode)",
                          srcTexture, dstTexture, width, height);
}

bool ApplySrgbEncode(unsigned int srcTexture, unsigned int dstTexture, int width, int height) {
    return RunConvertPass(g_encode, kEncodeShaderSource, "srgb_convert(encode)",
                          srcTexture, dstTexture, width, height);
}
