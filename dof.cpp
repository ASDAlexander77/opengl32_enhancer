// See dof.h. Single compute dispatch plus a small UBO - structurally identical to
// depth_vignette.cpp, but instead of darkening by depth it gathers neighbouring texels over a
// disc whose radius grows with the pixel's distance from the focus plane. No capture/blit/
// state-save of its own - see post_effects.cpp for the shared pipeline that owns
// srcTexture/dstTexture/depthTexture and the app's GL state around the whole chain.
#include <cstdio>

#include "dof.h"
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
// outputImage's image-unit binding=0 and DofConfigBlock's uniform-buffer binding=0 are different
// GL namespaces again from both - see depth_vignette.cpp's equivalent comment. None collide.
//
// Two deliberate choices in here:
//
//   - The early-out when radius is zero returns the centre texel VERBATIM. uv lands exactly on
//     the texel centre, so this is bit-exact rather than merely close, which is what makes
//     blurStrength=0 an exact no-op and keeps a perfectly focused subject untouched instead of
//     very slightly resampled.
//   - Each tap is weighted by its OWN circle of confusion, squared. Without that, a sharp
//     foreground object bleeds outward onto the blurred background behind it - the classic
//     gather-DoF halo - because its texels would contribute to their blurred neighbours as
//     strongly as anything else. An in-focus tap has coc ~0, so it contributes almost nothing to
//     a blurred pixel, while a blurred tap contributes fully. This is not a true scatter-as-
//     gather: it cannot make a blurred foreground correctly spill over a sharp background. That
//     costs a depth pre-pass and a second dispatch, and is not worth it here.
const char* kDofShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(binding = 1) uniform sampler2D depthTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform DofConfigBlock {\n"
    "    float focusDistance;\n"
    "    float focusRange;\n"
    "    float blurStrength;\n"
    "    float zNear;\n"
    "    float zFar;\n"
    "};\n"
    // Taps and maximum radius are fixed rather than configurable: 24 samples on a Vogel disc is
    // enough that the disc reads as a soft circle instead of a ring at this radius, and 16 px is
    // a strong blur without turning the frame to soup. Both are cheap to revisit; neither is a
    // knob a player would know how to set.
    "const int kTaps = 24;\n"
    "const float kMaxRadiusPixels = 16.0;\n"
    "const float kGoldenAngle = 2.39996323;\n"
    // Raw hardware depth -> eye-space distance in world units. Same conversion as depth_view.h's
    // LinearizeDepth and ssao.cpp's unprojection.
    "float Linearize(float raw) {\n"
    "    float ndc = 2.0 * raw - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    // 0 inside the sharp band, ramping to 1 one full focusRange beyond it.
    "float Coc01(float dist, float focus) {\n"
    "    return clamp((abs(dist - focus) - focusRange * 0.5) / focusRange, 0.0, 1.0);\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 texel = 1.0 / vec2(outSize);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) * texel;\n"
    "    vec4 center = texture(inputTex, uv);\n"
    // focusDistance <= 0 means "focus on the centre of the screen" - one extra fetch per
    // invocation, which is cheaper and simpler than a readback or a separate reduction pass.
    "    float focus = focusDistance;\n"
    "    if (focus <= 0.0) {\n"
    "        focus = Linearize(texture(depthTex, vec2(0.5)).r);\n"
    "    }\n"
    "    float centerCoc = Coc01(Linearize(texture(depthTex, uv).r), focus);\n"
    "    float radius = centerCoc * blurStrength * kMaxRadiusPixels;\n"
    "    if (radius <= 0.0) {\n"
    "        imageStore(outputImage, outCoord, center);\n"
    "        return;\n"
    "    }\n"
    "    vec4 sum = center * (centerCoc * centerCoc + 0.001);\n"
    "    float weight = centerCoc * centerCoc + 0.001;\n"
    "    for (int i = 0; i < kTaps; ++i) {\n"
    // sqrt() spreads the taps by equal AREA rather than equal radius, so the disc samples
    // uniformly instead of clumping at the centre.
    "        float t = (float(i) + 0.5) / float(kTaps);\n"
    "        float r = sqrt(t) * radius;\n"
    "        float a = float(i) * kGoldenAngle;\n"
    "        vec2 sampleUv = uv + vec2(cos(a), sin(a)) * r * texel;\n"
    "        float sampleCoc = Coc01(Linearize(texture(depthTex, sampleUv).r), focus);\n"
    "        float w = sampleCoc * sampleCoc + 0.001;\n"
    "        sum += texture(inputTex, sampleUv) * w;\n"
    "        weight += w;\n"
    "    }\n"
    "    imageStore(outputImage, outCoord, sum / weight);\n"
    "}\n";

struct DofConfigData {
    float focusDistance;
    float focusRange;
    float blurStrength;
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
    gl.glShaderSource(shader, 1, &kDofShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] dof: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] dof: FAILED to link compute program: %s\n", log);
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(DofConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplyDof(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
               int width, int height, const ProjectionParams& projection,
               float focusDistance, float focusRange, float blurStrength) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] dof: GL 4.3 compute support unavailable on this context, "
                   "effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (depthTexture == 0) {
        return false;
    }

    // No usable frustum means raw depth cannot be turned into the world units focusDistance is
    // denominated in. Blurring against a made-up distance would be worse than not blurring, so
    // no-op and let the caller keep the unmodified source - see projection_capture.h.
    if (projection.zNear <= 0.0f || projection.zFar <= projection.zNear) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] dof: no projection captured yet, effect idle until the "
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
            printf("[opengl32_enh_cpp] dof: shader init failed, effect disabled until the GL "
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

    DofConfigData configData{};
    configData.focusDistance = focusDistance;
    // focusRange divides the circle-of-confusion ramp, so a zero would produce an infinity even
    // though the config clamp already keeps it positive - belt and braces, the same way
    // depth_vignette.cpp guards smoothstep's edge0 < edge1 requirement.
    configData.focusRange = focusRange > 0.001f ? focusRange : 0.001f;
    configData.blurStrength = blurStrength;
    configData.zNear = projection.zNear;
    configData.zFar = projection.zFar;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(DofConfigData), &configData, GL_DYNAMIC_DRAW);
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
        printf("[opengl32_enh_cpp] dof: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
