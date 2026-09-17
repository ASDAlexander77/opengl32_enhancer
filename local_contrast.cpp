// See local_contrast.h. One separable 9-tap Gaussian blur program (reused for both radii,
// distinguished at dispatch time by a `radiusScale` uniform that multiplies the tap spacing -
// so the SAME kernel weights produce a tight blur at scale 1 and a wide one at scale 4, no
// second shader needed) plus a composite pass.
//
// Sequence: H-blur(src, smallScale) -> temp -> V-blur(temp, smallScale) -> structureFinal;
// H-blur(src, largeScale) -> temp (reused) -> V-blur(temp, largeScale) -> toneFinal; composite
// reads src + both finals. Both blurs read directly from the ORIGINAL srcTexture, not chained
// off each other, so structureStrength and toneStrength are independent - raising one doesn't
// change what the other measures (see local_contrast.h).
//
// No capture/blit/state-save of its own - see post_effects.cpp for the shared pipeline that
// owns srcTexture/dstTexture and the app's GL state around the whole chain of stages this is
// one link in. Only the blur intermediates (own, resized with width/height) belong here.
#include <cstdio>

#include "local_contrast.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                = 0x0DE1;
const unsigned int GL_TEXTURE0                  = 0x84C0;
const unsigned int GL_TEXTURE_MIN_FILTER        = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER        = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S            = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T            = 0x2803;
const unsigned int GL_LINEAR                    = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE             = 0x812F;
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

// Fixed radius multipliers for the two blur passes - not config-exposed, same precedent as
// bloom.cpp's Gaussian sigma/weights (only the effect's STRENGTH is a tunable, not its
// underlying blur shape). 2 texels apart is tight enough to isolate fine texture; 8 is wide
// enough to separate broad midtone regions for a real "clarity" feel.
const float kStructureRadiusScale = 2.0f;
const float kToneRadiusScale = 8.0f;

// Same normalized 9-tap Gaussian weights as bloom.cpp's blur (w0 + 2*(w1+w2+w3+w4) == 1.0).
// params.x doubles as the blur direction flag (>0.5 = horizontal, else vertical), params.y is
// the radius scale that stretches the tap spacing - see this file's header comment.
const char* kBlurShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D dstImage;\n"
    "layout(std140, binding = 0) uniform LocalContrastBlurBlock {\n"
    "    vec4 params;\n"  // .x = direction, .y = radiusScale, .zw unused
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(dstImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 texelSize = 1.0 / vec2(outSize);\n"
    "    vec2 dir = params.x > 0.5 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) * texelSize;\n"
    "    float w[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);\n"
    "    vec3 sum = texture(srcTex, uv).rgb * w[0];\n"
    "    for (int i = 1; i < 5; ++i) {\n"
    "        vec2 offset = dir * texelSize * float(i) * params.y;\n"
    "        sum += texture(srcTex, uv + offset).rgb * w[i];\n"
    "        sum += texture(srcTex, uv - offset).rgb * w[i];\n"
    "    }\n"
    "    imageStore(dstImage, outCoord, vec4(sum, 1.0));\n"
    "}\n";

const char* kCompositeShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D origTex;\n"
    "layout(binding = 1) uniform sampler2D structureBlurTex;\n"
    "layout(binding = 2) uniform sampler2D toneBlurTex;\n"
    "layout(rgba16f, binding = 3) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform LocalContrastCompositeBlock {\n"
    "    vec4 params;\n"  // .x = structureStrength, .y = toneStrength, .zw unused
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 orig = texture(origTex, uv);\n"
    "    vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);\n"
    "    float lumaOrig = dot(orig.rgb, lumaWeights);\n"
    "    float lumaStructureBlur = dot(texture(structureBlurTex, uv).rgb, lumaWeights);\n"
    "    float lumaToneBlur = dot(texture(toneBlurTex, uv).rgb, lumaWeights);\n"
    "    float structureDetail = lumaOrig - lumaStructureBlur;\n"
    "    float toneDetail = lumaOrig - lumaToneBlur;\n"
    "    float delta = structureDetail * params.x + toneDetail * params.y;\n"
    "    vec3 result = clamp(orig.rgb + vec3(delta), 0.0, 1.0);\n"
    "    imageStore(outputImage, outCoord, vec4(result, orig.a));\n"
    "}\n";

struct BlurConfigData {
    float direction;
    float radiusScale;
    float pad[2];
};

struct CompositeConfigData {
    float structureStrength;
    float toneStrength;
    float pad[2];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int blurProgram = 0;
    unsigned int compositeProgram = 0;
    unsigned int blurUbo = 0;
    unsigned int compositeUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int blurTempTexture = 0;
    unsigned int structureFinalTexture = 0;
    unsigned int toneFinalTexture = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* source, const char* label, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &source, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] local_contrast: FAILED to compile '%s' compute shader: %s\n", label, log);
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
        printf("[opengl32_enh_cpp] local_contrast: FAILED to link '%s' compute program: %s\n", label, log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

unsigned int CreateWorkTexture(const GlComputeApi& gl, int width, int height) {
    unsigned int texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    return texture;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        unsigned int textures[3] = {g_state.blurTempTexture, g_state.structureFinalTexture, g_state.toneFinalTexture};
        gl.glDeleteTextures(3, textures);
        g_state.texturesValid = false;
    }

    g_state.blurTempTexture = CreateWorkTexture(gl, width, height);
    g_state.structureFinalTexture = CreateWorkTexture(gl, width, height);
    g_state.toneFinalTexture = CreateWorkTexture(gl, width, height);

    g_state.width = width;
    g_state.height = height;
    g_state.texturesValid = true;
}

void EnsureUbos(const GlComputeApi& gl) {
    if (g_state.blurUbo == 0) {
        gl.glGenBuffers(1, &g_state.blurUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.blurUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BlurConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
    if (g_state.compositeUbo == 0) {
        gl.glGenBuffers(1, &g_state.compositeUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.compositeUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CompositeConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
}

unsigned int GroupCount(int extent) {
    return (unsigned int)((extent + 7) / 8);
}

void RunBlurPass(const GlComputeApi& gl, unsigned int srcTex, unsigned int dstTex,
                  float direction, float radiusScale, unsigned int groupsX, unsigned int groupsY) {
    BlurConfigData data{};
    data.direction = direction;
    data.radiusScale = radiusScale;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.blurUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(BlurConfigData), &data, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.blurUbo);

    gl.glUseProgram(g_state.blurProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTex);
    gl.glBindImageTexture(1, dstTex, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

}  // namespace

bool ApplyLocalContrast(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
                         float structureStrength, float toneStrength) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] local_contrast: GL 4.3 compute support unavailable on "
                   "this context, effect disabled\n");
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
        bool ok = true;
        ok &= CompileAndLink(gl, kBlurShaderSource, "blur", g_state.blurProgram);
        ok &= CompileAndLink(gl, kCompositeShaderSource, "composite", g_state.compositeProgram);
        g_state.initOk = ok;
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] local_contrast: shader init failed, effect disabled "
                   "until the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureTextures(gl, width, height);
    EnsureUbos(gl);

    unsigned int groupsX = GroupCount(width);
    unsigned int groupsY = GroupCount(height);

    // Structure layer: small-radius blur of the ORIGINAL srcTexture, both directions.
    RunBlurPass(gl, srcTexture, g_state.blurTempTexture, 1.0f, kStructureRadiusScale, groupsX, groupsY);
    RunBlurPass(gl, g_state.blurTempTexture, g_state.structureFinalTexture, 0.0f, kStructureRadiusScale, groupsX, groupsY);

    // Tone layer: large-radius blur of the ORIGINAL srcTexture (not chained off the structure
    // blur above), both directions. blurTempTexture is safely reused - the structure pass's
    // vertical dispatch already finished reading it before this overwrites it.
    RunBlurPass(gl, srcTexture, g_state.blurTempTexture, 1.0f, kToneRadiusScale, groupsX, groupsY);
    RunBlurPass(gl, g_state.blurTempTexture, g_state.toneFinalTexture, 0.0f, kToneRadiusScale, groupsX, groupsY);

    // Composite: srcTexture + both detail layers * their strengths -> dstTexture.
    CompositeConfigData compositeData{};
    compositeData.structureStrength = structureStrength;
    compositeData.toneStrength = toneStrength;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.compositeUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(CompositeConfigData), &compositeData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.compositeUbo);

    gl.glUseProgram(g_state.compositeProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.structureFinalTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.toneFinalTexture);
    gl.glBindImageTexture(3, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] local_contrast: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
