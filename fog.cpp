// See fog.h. Single compute dispatch plus a small UBO - structurally identical to
// depth_vignette.cpp and dof.cpp. No capture/blit/state-save of its own - see post_effects.cpp
// for the shared pipeline that owns srcTexture/dstTexture/depthTexture and the app's GL state
// around the whole chain of stages this is one link in.
#include <cstdio>

#include "fog.h"
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
// outputImage's image-unit binding=0 and FogConfigBlock's uniform-buffer binding=0 are different
// GL namespaces again from both - see depth_vignette.cpp's equivalent comment. None collide.
const char* kFogShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(binding = 1) uniform sampler2D depthTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform FogConfigBlock {\n"
    "    vec4 fogColor;\n"
    "    float fogStart;\n"
    "    float fogSpan;\n"
    "    float intensity;\n"
    "    float zNear;\n"
    "    float zFar;\n"
    "};\n"
    // Raw hardware depth -> eye-space distance in world units. Same conversion as depth_view.h's
    // LinearizeDepth and dof.cpp's.
    "float Linearize(float raw) {\n"
    "    float ndc = 2.0 * raw - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    float dist = Linearize(texture(depthTex, uv).r);\n"
    // fogSpan is precomputed on the CPU and guaranteed positive, so this cannot divide by zero.
    "    float t = clamp((dist - fogStart) / fogSpan, 0.0, 1.0) * intensity;\n"
    // At t == 0 mix() returns color.rgb bit-exact, which is what makes both intensity=0 and
    // anything nearer than fogStart an exact no-op rather than a near-miss.
    "    imageStore(outputImage, outCoord, vec4(mix(color.rgb, fogColor.rgb, t), color.a));\n"
    "}\n";

struct FogConfigData {
    // vec4 first: std140 aligns a vec4 to 16 bytes, so putting it after the scalars would leave
    // the layout depending on padding nobody can see. This ordering needs no padding at all
    // between the color and the scalars.
    float colorR, colorG, colorB, colorA;
    float fogStart;
    float fogSpan;
    float intensity;
    float zNear;
    float zFar;
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
    gl.glShaderSource(shader, 1, &kFogShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] fog: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] fog: FAILED to link compute program: %s\n", log);
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(FogConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplyFog(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
               int width, int height, const ProjectionParams& projection,
               float fogStart, float fogEnd, float intensity,
               float colorR, float colorG, float colorB) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] fog: GL 4.3 compute support unavailable on this context, "
                   "effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (depthTexture == 0) {
        return false;
    }

    // No usable frustum means raw depth cannot be turned into the world units fogStart/fogEnd are
    // denominated in - see projection_capture.h and dof.cpp's equivalent guard.
    if (projection.zNear <= 0.0f || projection.zFar <= projection.zNear) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] fog: no projection captured yet, effect idle until the "
                   "game establishes a 3D view\n");
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
            printf("[opengl32_enh_cpp] fog: shader init failed, effect disabled until the GL "
                   "context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureUbo(gl);

    FogConfigData configData{};
    configData.colorR = colorR;
    configData.colorG = colorG;
    configData.colorB = colorB;
    configData.colorA = 1.0f;
    configData.fogStart = fogStart;
    // A fogEnd at or below fogStart is a misconfiguration, not a reason to divide by zero. The
    // smallest positive span makes everything past fogStart fully fogged, which is the honest
    // reading of "the ramp has no width" and is visibly wrong rather than silently NaN.
    configData.fogSpan = (fogEnd > fogStart) ? (fogEnd - fogStart) : 0.0001f;
    configData.intensity = intensity;
    configData.zNear = projection.zNear;
    configData.zFar = projection.zFar;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(FogConfigData), &configData, GL_DYNAMIC_DRAW);
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
        printf("[opengl32_enh_cpp] fog: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
