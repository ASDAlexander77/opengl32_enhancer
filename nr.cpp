// See nr.h. Two compute programs: a 5x5 bilateral filter (dispatched `passes` times in a row,
// ping-ponging between two owned work textures) and a combine pass.
//
// The combine pass is why this isn't just "run a bilateral filter and call it done": a plain
// bilateral filter smooths luma and chroma by the same amount, but chroma (color) noise reads
// as far uglier than luma (brightness) noise at the same magnitude - it's the reason real
// video/photo denoisers almost always hit chroma harder than luma by default. So the combine
// pass reads BOTH the original srcTexture pixel and the fully-filtered result, splits each
// into a luma value (the standard Rec. 709 luma dot product) and a chroma residual (color
// minus luma, spread across the three channels), and re-mixes the two independently:
// `tonePreservation` pulls the mixed luma back toward the original (denoising color without
// touching brightness/detail when set to 1), `colorStrength` scales how much of the filtered
// chroma survives, and `grainPreservation` re-adds a fraction of the REAL luma detail the
// filter removed (original luma minus filtered luma - not synthetic noise, unlike FSR's
// fsrFilmGrain). All three read directly off values already computed for the luma/chroma
// split, so none of them cost an extra pass.
//
// No capture/blit/state-save of its own - see post_effects.cpp for the shared pipeline that
// owns srcTexture/dstTexture and the app's GL state around the whole chain of stages this is
// one link in. Only the filter intermediates (own, resized with width/height) belong here.
#include <cstdio>

#include "nr.h"
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

// 5x5 bilateral filter: a fixed spatial Gaussian (sigma 1.5, computed inline - a precomputed
// weight table would save a handful of exp() calls but 25 taps is already cheap next to e.g.
// FSR's 12-tap EASU or NIS's much larger neighborhood) times a range weight from the luma
// difference between the center texel and each tap. params.x (rangeSigma) is derived from
// `intensity` on the C++ side - see ApplyNr.
const char* kFilterShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D srcTex;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D dstImage;\n"
    "layout(std140, binding = 0) uniform NrFilterConfigBlock {\n"
    "    vec4 params;\n"   // .x = rangeSigma, .yzw unused (std140 padding)
    "};\n"
    "vec3 nrLoadRgb(ivec2 p, ivec2 maxCoord) {\n"
    "    return texelFetch(srcTex, clamp(p, ivec2(0), maxCoord), 0).rgb;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(dstImage);\n"
    "    ivec2 sp = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (sp.x >= outSize.x || sp.y >= outSize.y) { return; }\n"
    "    ivec2 maxCoord = outSize - ivec2(1);\n"
    "    vec4 center = texelFetch(srcTex, clamp(sp, ivec2(0), maxCoord), 0);\n"
    "    float centerLuma = dot(center.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
    "    float rangeSigma = max(params.x, 0.001);\n"
    "    float rangeDenom = 2.0 * rangeSigma * rangeSigma;\n"
    "    vec3 sum = vec3(0.0);\n"
    "    float weightSum = 0.0;\n"
    "    for (int dy = -2; dy <= 2; ++dy) {\n"
    "        for (int dx = -2; dx <= 2; ++dx) {\n"
    "            vec3 tapColor = nrLoadRgb(sp + ivec2(dx, dy), maxCoord);\n"
    "            float tapLuma = dot(tapColor, vec3(0.2126, 0.7152, 0.0722));\n"
    "            float spatialWeight = exp(-float(dx * dx + dy * dy) / 4.5);\n"
    "            float rangeDist = tapLuma - centerLuma;\n"
    "            float rangeWeight = exp(-(rangeDist * rangeDist) / rangeDenom);\n"
    "            float w = spatialWeight * rangeWeight;\n"
    "            sum += tapColor * w;\n"
    "            weightSum += w;\n"
    "        }\n"
    "    }\n"
    "    vec3 filtered = sum / max(weightSum, 0.0001);\n"
    "    imageStore(dstImage, sp, vec4(filtered, center.a));\n"
    "}\n";

const char* kCombineShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D origTex;\n"
    "layout(binding = 1) uniform sampler2D filteredTex;\n"
    "layout(rgba16f, binding = 2) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform NrCombineConfigBlock {\n"
    "    vec4 params;\n"  // .x = colorStrength, .y = tonePreservation, .z = grainPreservation
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 sp = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (sp.x >= outSize.x || sp.y >= outSize.y) { return; }\n"
    "    vec4 orig = texelFetch(origTex, sp, 0);\n"
    "    vec4 filtered = texelFetch(filteredTex, sp, 0);\n"
    "    vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);\n"
    "    float lumaOrig = dot(orig.rgb, lumaWeights);\n"
    "    float lumaFilt = dot(filtered.rgb, lumaWeights);\n"
    "    vec3 chromaOrig = orig.rgb - lumaOrig;\n"
    "    vec3 chromaFilt = filtered.rgb - lumaFilt;\n"
    "    float colorStrength = clamp(params.x, 0.0, 1.0);\n"
    "    float tonePreservation = clamp(params.y, 0.0, 1.0);\n"
    "    float grainPreservation = clamp(params.z, 0.0, 1.0);\n"
    "    float newLuma = mix(lumaOrig, lumaFilt, 1.0 - tonePreservation);\n"
    "    vec3 newChroma = mix(chromaOrig, chromaFilt, colorStrength);\n"
    "    float residual = lumaOrig - lumaFilt;\n"
    "    newLuma += residual * grainPreservation;\n"
    "    vec3 result = vec3(newLuma) + newChroma;\n"
    "    imageStore(outputImage, sp, vec4(clamp(result, 0.0, 1.0), orig.a));\n"
    "}\n";

struct NrFilterConfigData {
    float rangeSigma;
    float pad[3];
};

struct NrCombineConfigData {
    float colorStrength;
    float tonePreservation;
    float grainPreservation;
    float pad;
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int filterProgram = 0;
    unsigned int combineProgram = 0;
    unsigned int filterUbo = 0;
    unsigned int combineUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    // Ping-pong pair the filter pass iterates over; whichever one holds the last pass's output
    // is read by the combine pass alongside the untouched srcTexture.
    unsigned int filterTex[2] = {0, 0};
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
        printf("[opengl32_enh_cpp] nr: FAILED to compile '%s' compute shader: %s\n", label, log);
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
        printf("[opengl32_enh_cpp] nr: FAILED to link '%s' compute program: %s\n", label, log);
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
        gl.glDeleteTextures(2, g_state.filterTex);
        g_state.texturesValid = false;
    }

    g_state.filterTex[0] = CreateWorkTexture(gl, width, height);
    g_state.filterTex[1] = CreateWorkTexture(gl, width, height);

    g_state.width = width;
    g_state.height = height;
    g_state.texturesValid = true;
}

void EnsureUbos(const GlComputeApi& gl) {
    if (g_state.filterUbo == 0) {
        gl.glGenBuffers(1, &g_state.filterUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.filterUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(NrFilterConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
    if (g_state.combineUbo == 0) {
        gl.glGenBuffers(1, &g_state.combineUbo);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.combineUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(NrCombineConfigData), nullptr, GL_DYNAMIC_DRAW);
    }
}

unsigned int GroupCount(int extent) {
    return (unsigned int)((extent + 7) / 8);
}

}  // namespace

bool ApplyNr(unsigned int srcTexture, unsigned int dstTexture, int width, int height,
             float intensity, int passes, float colorStrength, float tonePreservation,
             float grainPreservation) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] nr: GL 4.3 compute support unavailable on this context, "
                   "effect disabled\n");
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
        ok &= CompileAndLink(gl, kFilterShaderSource, "filter", g_state.filterProgram);
        ok &= CompileAndLink(gl, kCombineShaderSource, "combine", g_state.combineProgram);
        g_state.initOk = ok;
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] nr: shader init failed, effect disabled until the GL "
                   "context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }
    if (passes < 1) { passes = 1; }
    if (passes > 4) { passes = 4; }

    EnsureTextures(gl, width, height);
    EnsureUbos(gl);

    unsigned int groupsX = GroupCount(width);
    unsigned int groupsY = GroupCount(height);

    // rangeSigma grows with `intensity`: near 0 it is small enough that only near-exact-luma
    // neighbors (or the center tap itself) get meaningful weight, so the filter is an
    // effective no-op; the ceiling (0.35, roughly a third of the full 0..1 luma range) is
    // already a strong denoise on real content.
    float clampedIntensity = intensity;
    if (clampedIntensity < 0.0f) { clampedIntensity = 0.0f; }
    if (clampedIntensity > 1.0f) { clampedIntensity = 1.0f; }
    NrFilterConfigData filterData{};
    filterData.rangeSigma = 0.001f + clampedIntensity * 0.35f;

    // Pass loop: filter reads srcTexture on the first iteration, then its own previous output.
    // filterTex ping-pongs so a pass never reads and writes the same texture.
    unsigned int readTex = srcTexture;
    int writeSlot = 0;
    for (int i = 0; i < passes; ++i) {
        gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.filterUbo);
        gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(NrFilterConfigData), &filterData, GL_DYNAMIC_DRAW);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.filterUbo);

        gl.glUseProgram(g_state.filterProgram);
        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(GL_TEXTURE_2D, readTex);
        gl.glBindImageTexture(1, g_state.filterTex[writeSlot], 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
        gl.glDispatchCompute(groupsX, groupsY, 1);
        gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        readTex = g_state.filterTex[writeSlot];
        writeSlot = 1 - writeSlot;
    }

    // Combine: srcTexture (original) + the last pass's output -> dstTexture.
    NrCombineConfigData combineData{};
    combineData.colorStrength = colorStrength;
    combineData.tonePreservation = tonePreservation;
    combineData.grainPreservation = grainPreservation;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.combineUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(NrCombineConfigData), &combineData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.combineUbo);

    gl.glUseProgram(g_state.combineProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, readTex);
    gl.glBindImageTexture(2, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] nr: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
