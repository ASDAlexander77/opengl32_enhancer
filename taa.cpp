// See taa.h. Single compute dispatch writing to TWO images at once - dstTexture (this
// pipeline stage's caller-owned output) and pingPong[writeIndex] (this effect's own
// persistent history texture, read back as `historyTex` next call) - both get the same
// computed result. That avoids an extra copy pass: TAA's history buffer can't simply *be*
// dstTexture, because dstTexture is a slot in the shared pipeline's own ping-pong pair (see
// post_effects.cpp), and which physical texture object plays "the shared pipeline's dst" on
// any given frame depends on how many other stages ran before TAA that frame - not something
// TAA can treat as consistently "last frame's output" the way its own private history buffer
// is. No capture/blit/state-save of its own beyond that - the caller owns the app's GL state
// around the whole chain.
#include <cstdint>
#include <cstdio>

#include "taa.h"
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

// currentTex=binding 0 (texture unit 0), historyTex=binding 1 (texture unit 1), outputImage=
// binding 2 and historyImage=binding 3 (image units - a separate namespace from texture
// units, no collision with either sampler binding), TaaConfigBlock=binding 0 (uniform buffer
// binding point 0 - a third separate namespace, no collision with currentTex's texture-unit
// 0). See this plan's Global Constraints "Binding-number discipline" note; the C++ side below
// must bind to these exact same units/points.
const char* kTaaShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D currentTex;\n"
    "layout(binding = 1) uniform sampler2D historyTex;\n"
    "layout(rgba16f, binding = 2) uniform writeonly image2D outputImage;\n"
    "layout(rgba16f, binding = 3) uniform writeonly image2D historyImage;\n"
    "layout(std140, binding = 0) uniform TaaConfigBlock {\n"
    "    float blend;\n"
    "    int historyValid;\n"
    "    float shimmerSuppression;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 texelSize = 1.0 / vec2(outSize);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) * texelSize;\n"
    "    vec4 currentColor = texture(currentTex, uv);\n"
    "    if (historyValid == 0) {\n"
    "        imageStore(outputImage, outCoord, currentColor);\n"
    "        imageStore(historyImage, outCoord, currentColor);\n"
    "        return;\n"
    "    }\n"
    "    vec3 neighborMin = currentColor.rgb;\n"
    "    vec3 neighborMax = currentColor.rgb;\n"
    "    for (int dy = -1; dy <= 1; ++dy) {\n"
    "        for (int dx = -1; dx <= 1; ++dx) {\n"
    "            if (dx == 0 && dy == 0) { continue; }\n"
    "            vec3 s = texture(currentTex, uv + vec2(dx, dy) * texelSize).rgb;\n"
    "            neighborMin = min(neighborMin, s);\n"
    "            neighborMax = max(neighborMax, s);\n"
    "        }\n"
    "    }\n"
    "    vec4 historyColor = texture(historyTex, uv);\n"
    "    vec3 clampedHistory = clamp(historyColor.rgb, neighborMin, neighborMax);\n"
    // History rejection: the neighborhood clamp above only bounds history to the CURRENT
    // frame's own local color range at this pixel - it has no idea whether that range still
    // describes the same on-screen content, or a completely different thing that happens to
    // land in a similar numeric range (busy/detailed regions have wide clamp boxes, so this
    // happens easily). That let stale history survive the clamp mostly unrejected on a fast
    // scene change, then take ~10 frames to exponentially decay away at a high `blend` -
    // visible as persisting ghosts of the previous frame. This is a SEPARATE, more direct
    // signal: how far the RAW (pre-clamp) history is from the current frame at this exact
    // pixel. A small raw difference is ordinary temporal noise the clamp is meant to smooth;
    // a large one means this pixel's content itself just changed, so history should be
    // rejected regardless of what the clamp box would have allowed.
    "    float rawDiff = length(currentColor.rgb - historyColor.rgb);\n"
    "    float rejection = smoothstep(0.1, 0.4, rawDiff);\n"
    "    float baseBlend = blend * (1.0 - rejection);\n"
    // Shimmer suppression: only kicks in where the current frame and the (already-clamped)
    // history nearly agree - i.e. nothing is actually moving here - so it can't add ghosting
    // to real motion or edges, only quiet flicker in already-static content. Layered on top of
    // rejection (not instead of it): a big rawDiff already drove baseBlend near 0, so this has
    // nothing left to amplify on a genuine scene change.
    "    float diff = length(currentColor.rgb - clampedHistory);\n"
    "    float stillness = 1.0 - smoothstep(0.0, 0.05, diff);\n"
    "    float effectiveBlend = mix(baseBlend, max(baseBlend, 0.95), stillness * shimmerSuppression);\n"
    "    vec3 result = mix(currentColor.rgb, clampedHistory, effectiveBlend);\n"
    "    vec4 out4 = vec4(result, currentColor.a);\n"
    "    imageStore(outputImage, outCoord, out4);\n"
    "    imageStore(historyImage, outCoord, out4);\n"
    "}\n";

struct TaaConfigData {
    float blend;
    int32_t historyValid;
    float shimmerSuppression;
    int32_t pad;
};

// The real path (ApplyTaaReal below). Binding numbers: five texture units (0-4), two image
// units (0-1), one uniform buffer binding point (0). Image units, texture units and uniform
// buffer binding points are three separate namespaces, so image binding 0 does not collide with
// currentTex at texture binding 0 - the same discipline the lite shader above follows, just
// with different numbers because this program declares its own.
//
// LinearEyeDistance/ViewPosFor/UvForViewPos are lifted from motion_blur.cpp - which lifted
// ViewPosFor/UvForViewPos from ssr.cpp, which lifted them from ssao.cpp - so every stage in
// this project agrees about where a pixel is in space. The one change is that they take the
// frustum as a parameter instead of reading a single global: this stage is the only one that
// needs TWO frusta at once, the jittered current and the unjittered previous.
const char* kTaaRealShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D currentTex;\n"
    "layout(binding = 1) uniform sampler2D historyTex;\n"
    "layout(binding = 2) uniform sampler2D depthTex;\n"
    "layout(binding = 3) uniform sampler2D worldTex;\n"
    "layout(binding = 4) uniform sampler2D captureTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(rgba16f, binding = 1) uniform writeonly image2D historyImage;\n"
    "layout(std140, binding = 0) uniform TaaRealConfigBlock {\n"
    "    vec4 curFrustum;\n"       // left, right, bottom, top - JITTERED, matches the depth buffer
    "    vec4 prevFrustum;\n"      // left, right, bottom, top - UNJITTERED, matches resolved history
    "    vec4 planes;\n"           // curNear, curFar, prevNear, blend
    "    int historyValid;\n"
    "    vec2 curJitterUv;\n"      // this frame's jitter, in UV, removed from the fetch below
    "    mat4 reprojection;\n"     // previous * inverse(current)
    "};\n"
    "float LinearEyeDistance(float rawDepth, float zNear, float zFar) {\n"
    "    float ndc = rawDepth * 2.0 - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    "vec3 ViewPosFor(vec4 f, vec2 uv, float eyeDist, float zNear) {\n"
    "    float x = (f.x + (f.y - f.x) * uv.x) * (eyeDist / zNear);\n"
    "    float y = (f.z + (f.w - f.z) * uv.y) * (eyeDist / zNear);\n"
    "    return vec3(x, y, -eyeDist);\n"
    "}\n"
    "vec2 UvForViewPos(vec4 f, vec3 p, float zNear) {\n"
    "    float dist = -p.z;\n"
    "    float xn = p.x * zNear / dist;\n"
    "    float yn = p.y * zNear / dist;\n"
    "    return vec2((xn - f.x) / (f.y - f.x), (yn - f.z) / (f.w - f.z));\n"
    "}\n"
    // Where the finished frame differs from the pre-HUD frame, the game drew an overlay. The
    // threshold sits just clear of 8-bit quantisation (1/255), which makes this "did the game
    // draw here" rather than a tolerance to tune.
    "bool IsHud(ivec2 c) {\n"
    "    vec3 finished = texelFetch(captureTex, c, 0).rgb;\n"
    "    vec3 world = texelFetch(worldTex, c, 0).rgb;\n"
    "    return any(greaterThan(abs(finished - world), vec3(1.0 / 128.0)));\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (coord.x >= size.x || coord.y >= size.y) { return; }\n"
    "    vec4 src = texelFetch(currentTex, coord, 0);\n"
    // Every early-out writes src to BOTH targets, so a declined pixel is bit-identical to one
    // this stage never saw AND seeds history correctly for the next frame. Writing only the
    // output here would leave a stale history texel that the next frame would clip against.
    "    if (historyValid == 0 || IsHud(coord)) {\n"
    "        imageStore(outputImage, coord, src);\n"
    "        imageStore(historyImage, coord, src);\n"
    "        return;\n"
    "    }\n"
    "    vec2 uv = (vec2(coord) + vec2(0.5)) / vec2(size);\n"
    "    float raw = texelFetch(depthTex, coord, 0).r;\n"
    "    vec3 here = ViewPosFor(curFrustum, uv, LinearEyeDistance(raw, planes.x, planes.y), planes.x);\n"
    "    vec3 before = (reprojection * vec4(here, 1.0)).xyz;\n"
    // Behind the previous frame's near plane there is no screen position to measure against,
    // and projecting it anyway would divide by a vanishing distance.
    "    if (before.z > -planes.z) {\n"
    "        imageStore(outputImage, coord, src);\n"
    "        imageStore(historyImage, coord, src);\n"
    "        return;\n"
    "    }\n"
    // Unprojecting with the JITTERED current frustum and projecting into the UNJITTERED
    // previous one answers "where was this scene point last frame" - a scene-space motion
    // vector. But history is indexed on the PIXEL GRID, and the scene point sampled at this
    // pixel sits curJitterUv away from the pixel centre, so that answer is off by exactly the
    // current frame's jitter. Taking it back off is what makes a static camera fetch each
    // pixel's own history: without it the fetch lands at uv + jx/width every frame, and the
    // recursion D <- blend * (D + j) turns that into a standing sub-pixel displacement - the
    // permanent wobble taa.h warns about, reached by single-counting the offset rather than
    // double-counting it. Standard TAA computes its velocity with both ends unjittered and
    // gets zero for a static camera; this subtraction is the same thing, applied after the
    // unprojection because the depth being unprojected really was rendered jittered.
    "    vec2 histUv = UvForViewPos(prevFrustum, before, planes.z) - curJitterUv;\n"
    // Off screen last frame means there is no history for this pixel - not history worth
    // clamping. Sampling anyway would drag an edge texel across the whole border.
    "    if (histUv.x < 0.0 || histUv.x > 1.0 || histUv.y < 0.0 || histUv.y > 1.0) {\n"
    "        imageStore(outputImage, coord, src);\n"
    "        imageStore(historyImage, coord, src);\n"
    "        return;\n"
    "    }\n"
    // Variance clipping rather than the neighbourhood min/max the fallback path uses. The
    // min/max box is the looser of the two, and taa.h already documents stale history
    // surviving inside it on busy content, where a wide local colour range admits almost
    // anything. One standard deviation is the conventional tightening.
    "    vec3 m1 = vec3(0.0);\n"
    "    vec3 m2 = vec3(0.0);\n"
    "    for (int dy = -1; dy <= 1; ++dy) {\n"
    "        for (int dx = -1; dx <= 1; ++dx) {\n"
    "            ivec2 c = clamp(coord + ivec2(dx, dy), ivec2(0), size - ivec2(1));\n"
    "            vec3 s = texelFetch(currentTex, c, 0).rgb;\n"
    "            m1 += s;\n"
    "            m2 += s * s;\n"
    "        }\n"
    "    }\n"
    "    vec3 mean = m1 / 9.0;\n"
    "    vec3 sigma = sqrt(max(m2 / 9.0 - mean * mean, vec3(0.0)));\n"
    "    vec3 history = texture(historyTex, histUv).rgb;\n"
    "    vec3 clipped = clamp(history, mean - sigma, mean + sigma);\n"
    "    vec4 out4 = vec4(mix(src.rgb, clipped, planes.w), src.a);\n"
    "    imageStore(outputImage, coord, out4);\n"
    "    imageStore(historyImage, coord, out4);\n"
    "}\n";

// std140 layout for TaaRealConfigBlock above, offsets in bytes: the three vec4s occupy 0..47,
// the int sits at 48, the vec2 is 8-byte aligned so it starts at 56 (one word of padding), and
// the mat4 is 16-byte aligned so it starts at 64. The vec2 therefore lands exactly in the hole
// the int left behind and costs nothing. Get the padding wrong and the matrix starts twelve
// bytes early, at 52.
struct TaaRealConfigData {
    float curFrustum[4];    // 0
    float prevFrustum[4];   // 16
    float planes[4];        // 32 - curNear, curFar, prevNear, blend
    int32_t historyValid;   // 48
    int32_t pad;            // 52
    float curJitterUv[2];   // 56
    float reprojection[16]; // 64
};

struct TaaState {
    bool initTried = false;
    bool initOk = false;
    // The GL context these cached objects belong to - see GetGlContextGeneration().
    unsigned int generation = 0;
    unsigned int program = 0;
    unsigned int configUbo = 0;

    // The real path's own program, UBO and init flags. Separate flags rather than reusing
    // initTried/initOk so that a failed real-path compile neither retries every frame nor
    // disables the lite path, which is a different shader and may well still build.
    bool realInitTried = false;
    bool realInitOk = false;
    unsigned int realProgram = 0;
    unsigned int realConfigUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int pingPong[2] = {0, 0};
    int activeHistory = 0;     // pingPong[activeHistory] holds the last successful frame's output
    bool historyValid = false;
};

TaaState g_state;

bool CompileAndLink(const GlComputeApi& gl, const char* source, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &source, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] taa: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] taa: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

void EnsureTextures(const GlComputeApi& gl, TaaState& state, int width, int height) {
    if (state.texturesValid && state.width == width && state.height == height) {
        return;
    }

    if (state.texturesValid) {
        gl.glDeleteTextures(2, state.pingPong);
        state.texturesValid = false;
    }

    unsigned int textures[2] = {0, 0};
    gl.glGenTextures(2, textures);
    state.pingPong[0] = textures[0];
    state.pingPong[1] = textures[1];

    for (int i = 0; i < 2; ++i) {
        gl.glBindTexture(GL_TEXTURE_2D, state.pingPong[i]);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
    }

    state.width = width;
    state.height = height;
    state.texturesValid = true;
    // A fresh/resized history buffer holds no meaningful data yet - reset both the ping-pong
    // role and the valid flag so the next call takes the historyValid=0 passthrough path
    // instead of blending against garbage.
    state.activeHistory = 0;
    state.historyValid = false;
}

void EnsureUbo(const GlComputeApi& gl, TaaState& state) {
    if (state.configUbo != 0) {
        return;
    }
    gl.glGenBuffers(1, &state.configUbo);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaConfigData), nullptr, GL_DYNAMIC_DRAW);
}

// The real path's block is a different size from the lite path's, so it gets its own buffer
// rather than one path resizing the other's behind its back.
void EnsureRealUbo(const GlComputeApi& gl, TaaState& state) {
    if (state.realConfigUbo != 0) {
        return;
    }
    gl.glGenBuffers(1, &state.realConfigUbo);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, state.realConfigUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaRealConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplyTaa(unsigned int srcTexture, unsigned int dstTexture, int width, int height, float blend,
              float shimmerSuppression) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] taa: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    // Cached programs/textures belong to the GL context that built them. If that context is
    // gone, drop the handles rather than deleting them (the owning context freed them already,
    // and glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = TaaState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, kTaaShaderSource, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] taa: shader init failed, effect disabled for the rest "
                   "of this process\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    if (width <= 0 || height <= 0) {
        return false;
    }

    EnsureTextures(gl, g_state, width, height);
    EnsureUbo(gl, g_state);

    int readIndex = g_state.activeHistory;
    int writeIndex = 1 - g_state.activeHistory;

    TaaConfigData configData{};
    configData.blend = blend;
    configData.historyValid = g_state.historyValid ? 1 : 0;
    configData.shimmerSuppression = shimmerSuppression;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.pingPong[readIndex]);
    gl.glBindImageTexture(2, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glBindImageTexture(3, g_state.pingPong[writeIndex], 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] taa: glGetError() = 0x%04X after dispatch\n", err);
        // Don't flip roles on a dispatch error - pingPong[writeIndex] may hold garbage, so
        // keep treating the same (valid) texture as history next call instead of promoting a
        // possibly-bad frame. dstTexture may also be garbage, so report failure to the caller.
        return false;
    }

    g_state.activeHistory = writeIndex;
    g_state.historyValid = true;
    return true;
}

bool ApplyTaaReal(unsigned int srcTexture, unsigned int dstTexture,
                  unsigned int depthTexture, unsigned int worldTexture,
                  unsigned int captureTexture,
                  int width, int height,
                  const ProjectionParams& currentProjection,
                  const ProjectionParams& previousProjection,
                  const float reprojection[16],
                  float currentJitterDx, float currentJitterDy,
                  float blend) {
    // Same refusals as every other stage here: no inputs invented, no guessing. A caller that
    // gets false must leave dstTexture alone rather than treating it as the new source.
    if (srcTexture == 0 || dstTexture == 0 || depthTexture == 0 ||
        worldTexture == 0 || captureTexture == 0 || width <= 0 || height <= 0) {
        return false;
    }
    // A degenerate frustum on either side divides by zero somewhere in the unprojection, so it
    // is refused rather than dispatched and hoped about.
    if (currentProjection.zNear <= 0.0f || currentProjection.zFar <= currentProjection.zNear ||
        previousProjection.zNear <= 0.0f ||
        currentProjection.right == currentProjection.left ||
        currentProjection.top == currentProjection.bottom ||
        previousProjection.right == previousProjection.left ||
        previousProjection.top == previousProjection.bottom) {
        return false;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] taa: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return false;
    }

    // Cached programs/textures belong to the GL context that built them. If that context is
    // gone, drop the handles rather than deleting them (the owning context freed them already,
    // and glDelete* now would hit unrelated objects) and rebuild against the current one. One
    // assignment, so every member - both paths' programs and UBOs included - resets together.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = TaaState{};
        g_state.generation = GetGlContextGeneration();
    }

    // The real path's own try-once flags: a real-path shader that fails to build must not be
    // recompiled every frame, and must not disable the lite path, which is a different program.
    if (!g_state.realInitTried) {
        g_state.realInitTried = true;
        g_state.realInitOk = CompileAndLink(gl, kTaaRealShaderSource, g_state.realProgram);
        if (!g_state.realInitOk) {
            printf("[opengl32_enh_cpp] taa: real-path shader init failed, falling back to the "
                   "motion-vector-free path for the rest of this process\n");
        }
    }
    if (!g_state.realInitOk) {
        return false;
    }

    EnsureTextures(gl, g_state, width, height);
    EnsureRealUbo(gl, g_state);

    int readIndex = g_state.activeHistory;
    int writeIndex = 1 - g_state.activeHistory;

    TaaRealConfigData configData{};
    configData.curFrustum[0] = currentProjection.left;
    configData.curFrustum[1] = currentProjection.right;
    configData.curFrustum[2] = currentProjection.bottom;
    configData.curFrustum[3] = currentProjection.top;
    configData.prevFrustum[0] = previousProjection.left;
    configData.prevFrustum[1] = previousProjection.right;
    configData.prevFrustum[2] = previousProjection.bottom;
    configData.prevFrustum[3] = previousProjection.top;
    configData.planes[0] = currentProjection.zNear;
    configData.planes[1] = currentProjection.zFar;
    configData.planes[2] = previousProjection.zNear;
    configData.planes[3] = blend;
    configData.historyValid = g_state.historyValid ? 1 : 0;
    // Frustum units into UV: the jittered and unjittered frusta have the same extent (jitter
    // translates the window, it does not resize it), so either one divides correctly here.
    configData.curJitterUv[0] =
        currentJitterDx / (currentProjection.right - currentProjection.left);
    configData.curJitterUv[1] =
        currentJitterDy / (currentProjection.top - currentProjection.bottom);
    for (int i = 0; i < 16; ++i) {
        configData.reprojection[i] = reprojection[i];
    }

    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.realConfigUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(TaaRealConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.realConfigUbo);

    gl.glUseProgram(g_state.realProgram);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, srcTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.pingPong[readIndex]);
    gl.glActiveTexture(GL_TEXTURE0 + 2);
    gl.glBindTexture(GL_TEXTURE_2D, depthTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 3);
    gl.glBindTexture(GL_TEXTURE_2D, worldTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 4);
    gl.glBindTexture(GL_TEXTURE_2D, captureTexture);
    gl.glBindImageTexture(0, dstTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);
    gl.glBindImageTexture(1, g_state.pingPong[writeIndex], 0, 0, 0, GL_WRITE_ONLY, GL_RGBA16F);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    gl.glActiveTexture(GL_TEXTURE0);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] taa: glGetError() = 0x%04X after real-path dispatch\n", err);
        // Don't flip roles on a dispatch error - pingPong[writeIndex] may hold garbage, and
        // both paths share this one ping-pong pair, so promoting it would make every later
        // frame (lite path included) clip against rubbish. dstTexture may also be garbage, so
        // report failure to the caller.
        return false;
    }

    g_state.activeHistory = writeIndex;
    g_state.historyValid = true;
    return true;
}
