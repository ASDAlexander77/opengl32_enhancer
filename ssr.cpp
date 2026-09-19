// See ssr.h. Single compute dispatch plus a small UBO - structurally identical to fog.cpp and
// dof.cpp, and it reuses ssao.cpp's depth-to-view-space reconstruction verbatim. No capture/
// blit/state-save of its own - see post_effects.cpp for the shared pipeline that owns
// srcTexture/dstTexture/depthTexture and the app's GL state around the whole chain of stages
// this is one link in.
#include <cstdio>

#include "ssr.h"
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

// Every numeric constant inside the shader is spelled out as a GLSL literal rather than pasted
// in from a C++ constant - a name that only exists on the host side compiles to nothing here and
// fails at shader-compile time, which cost a debugging round in light_shafts.cpp.
//
// The march is 32 steps. Fixed rather than derived so the cost per pixel is predictable: raising
// ssrMaxDistance lengthens the step instead of adding steps, trading precision (a longer step
// can straddle a thin object and miss it) for reach, which is the honest trade to expose.
const char* kSsrShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D colorTex;\n"
    "layout(binding = 1) uniform sampler2D depthTex;\n"
    "layout(rgba16f, binding = 0) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform SsrConfigBlock {\n"
    "    vec4 frustum;\n"   // left, right, bottom, top
    "    vec4 params;\n"    // zNear, zFar, maxDistance, thickness
    "    vec4 params2;\n"   // intensity, upThreshold, unused, unused
    "};\n"
    "float LinearEyeDistance(float rawDepth, float zNear, float zFar) {\n"
    "    float ndc = rawDepth * 2.0 - 1.0;\n"
    "    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));\n"
    "}\n"
    // Unprojection and its inverse. Keeping left/right/bottom/top separate rather than assuming
    // a symmetric field of view means an off-centre frustum unprojects correctly - see ssao.cpp,
    // which this is lifted from so that the two stages agree about where a pixel is in space.
    "vec3 ViewPosFor(vec2 uv, float eyeDist, float zNear) {\n"
    "    float x = (frustum.x + (frustum.y - frustum.x) * uv.x) * (eyeDist / zNear);\n"
    "    float y = (frustum.z + (frustum.w - frustum.z) * uv.y) * (eyeDist / zNear);\n"
    "    return vec3(x, y, -eyeDist);\n"
    "}\n"
    "vec2 UvForViewPos(vec3 p, float zNear) {\n"
    "    float dist = -p.z;\n"
    "    float xn = p.x * zNear / dist;\n"
    "    float yn = p.y * zNear / dist;\n"
    "    return vec2((xn - frustum.x) / (frustum.y - frustum.x),\n"
    "                 (yn - frustum.z) / (frustum.w - frustum.z));\n"
    "}\n"
    "vec3 ViewPosAt(ivec2 coord, ivec2 size, float zNear, float zFar) {\n"
    "    ivec2 c = clamp(coord, ivec2(0), size - ivec2(1));\n"
    "    float raw = texelFetch(depthTex, c, 0).r;\n"
    "    vec2 uv = (vec2(c) + vec2(0.5)) / vec2(size);\n"
    "    return ViewPosFor(uv, LinearEyeDistance(raw, zNear, zFar), zNear);\n"
    "}\n"
    "void main() {\n"
    "    ivec2 size = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= size.x || outCoord.y >= size.y) { return; }\n"
    "    float zNear = params.x;\n"
    "    float zFar = params.y;\n"
    "    float maxDistance = params.z;\n"
    "    float thickness = params.w;\n"
    "    float intensity = params2.x;\n"
    "    float upThreshold = params2.y;\n"
    // texelFetch, not texture(): every early-out below hands this value straight back, and a
    // filtered fetch would return something a hair off the source. A stage that found nothing
    // should be indistinguishable from one that never ran.
    "    vec4 color = texelFetch(colorTex, outCoord, 0);\n"
    "    float rawDepth = texelFetch(depthTex, outCoord, 0).r;\n"
    // Nothing was drawn here (cleared depth / sky): there is no surface to reflect in, and
    // unprojecting the far plane would put the ray's origin kilometres away.
    "    if (rawDepth >= 1.0) {\n"
    "        imageStore(outputImage, outCoord, color);\n"
    "        return;\n"
    "    }\n"
    "    float eyeDist = LinearEyeDistance(rawDepth, zNear, zFar);\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(size);\n"
    "    vec3 P = ViewPosFor(uv, eyeDist, zNear);\n"
    // Normals from neighbouring positions, taking whichever of the forward/backward difference
    // spans the smaller depth step on each axis so a silhouette's normal stays attached to the
    // surface it belongs to instead of tilting toward whatever sits across the edge.
    "    vec3 Pr = ViewPosAt(outCoord + ivec2(1, 0), size, zNear, zFar);\n"
    "    vec3 Pl = ViewPosAt(outCoord - ivec2(1, 0), size, zNear, zFar);\n"
    "    vec3 Pu = ViewPosAt(outCoord + ivec2(0, 1), size, zNear, zFar);\n"
    "    vec3 Pd = ViewPosAt(outCoord - ivec2(0, 1), size, zNear, zFar);\n"
    "    vec3 dx = (abs(Pr.z - P.z) < abs(P.z - Pl.z)) ? (Pr - P) : (P - Pl);\n"
    "    vec3 dy = (abs(Pu.z - P.z) < abs(P.z - Pd.z)) ? (Pu - P) : (P - Pd);\n"
    "    vec3 N = cross(dx, dy);\n"
    "    float nLen = length(N);\n"
    // Degenerate cross product: the neighbours were collinear, or the surface is exactly
    // edge-on. There is no usable orientation, so there is nothing to decide reflectivity with.
    "    if (nLen < 1e-8) {\n"
    "        imageStore(outputImage, outCoord, color);\n"
    "        return;\n"
    "    }\n"
    "    N /= nLen;\n"
    "    if (dot(N, P) > 0.0) { N = -N; }\n"
    // THE GATE - the whole heuristic, in one line. A GL 1.1 game never says what a surface is
    // made of, so orientation stands in for material: only surfaces facing up reflect. The
    // comparison is <= rather than < so that upThreshold=1 gates off even a perfectly flat
    // floor, which is what makes 1 usable as "disable without unlisting the stage".
    "    if (N.y <= upThreshold) {\n"
    "        imageStore(outputImage, outCoord, color);\n"
    "        return;\n"
    "    }\n"
    "    vec3 R = reflect(normalize(P), N);\n"
    "    float stepLen = maxDistance / 32.0;\n"
    "    bool hit = false;\n"
    "    vec2 hitUv = vec2(0.0);\n"
    "    float travelled = 0.0;\n"
    "    for (int i = 1; i <= 32; ++i) {\n"
    "        vec3 S = P + R * (stepLen * float(i));\n"
    // The ray has come back around in front of the near plane: there is no screen position for
    // it, and projecting anyway would divide by a vanishing distance.
    "        if (S.z > -zNear) { break; }\n"
    "        float sampleDist = -S.z;\n"
    "        vec2 sUv = UvForViewPos(S, zNear);\n"
    // Off the edge of the frame. Nothing beyond it was rendered, so the trace ends here rather
    // than clamping to the border and smearing an edge pixel down the whole reflection.
    "        if (any(lessThan(sUv, vec2(0.0))) || any(greaterThan(sUv, vec2(1.0)))) { break; }\n"
    "        float sceneRaw = texture(depthTex, sUv).r;\n"
    "        if (sceneRaw >= 1.0) { continue; }\n"
    "        float sceneDist = LinearEyeDistance(sceneRaw, zNear, zFar);\n"
    // A hit is the ray having passed BEHIND a surface. The thickness window is what stops a ray
    // that slipped past a thin foreground object from registering against the far wall it is now
    // nominally behind - without it, every ray that leaves the screen's geometry eventually
    // "hits" the background.
    "        if (sceneDist < sampleDist && sceneDist > sampleDist - thickness) {\n"
    "            hit = true;\n"
    "            hitUv = sUv;\n"
    "            travelled = stepLen * float(i);\n"
    "            break;\n"
    "        }\n"
    "    }\n"
    "    if (!hit) {\n"
    "        imageStore(outputImage, outCoord, color);\n"
    "        return;\n"
    "    }\n"
    // Two fades, both hiding the same thing: that this is screen-space, not a real trace. The
    // edge fade tapers a reflection out as its source approaches the border, so walking toward
    // the frame edge dims it instead of cutting it off. The distance fade does the same for a
    // ray about to exhaust maxDistance.
    "    vec2 edgeIn = smoothstep(vec2(0.0), vec2(0.1), hitUv);\n"
    "    vec2 edgeOut = smoothstep(vec2(0.0), vec2(0.1), vec2(1.0) - hitUv);\n"
    "    float fade = edgeIn.x * edgeIn.y * edgeOut.x * edgeOut.y *\n"
    "                  (1.0 - travelled / maxDistance);\n"
    "    vec3 reflected = texture(colorTex, hitUv).rgb;\n"
    // At a blend of 0 mix() returns color.rgb bit-exact, which is what makes intensity=0 an
    // exact no-op rather than a near-miss.
    "    float blend = clamp(intensity * fade, 0.0, 1.0);\n"
    "    imageStore(outputImage, outCoord, vec4(mix(color.rgb, reflected, blend), color.a));\n"
    "}\n";

struct SsrConfigData {
    float frustum[4];
    float params[4];
    float params2[4];
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
    gl.glShaderSource(shader, 1, &kSsrShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] ssr: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] ssr: FAILED to link compute program: %s\n", log);
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
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SsrConfigData), nullptr, GL_DYNAMIC_DRAW);
}

}  // namespace

bool ApplySsr(unsigned int srcTexture, unsigned int dstTexture, unsigned int depthTexture,
              int width, int height, const ProjectionParams& projection,
              float intensity, float maxDistance, float thickness, float upThreshold) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] ssr: GL 4.3 compute support unavailable on this context, "
                   "effect disabled\n");
            warned = true;
        }
        return false;
    }

    if (depthTexture == 0 || width <= 0 || height <= 0) {
        return false;
    }

    // Without a captured frustum raw depth cannot be turned into the view-space positions this
    // stage traces through - see projection_capture.h. Refusing here (rather than substituting a
    // guessed near/far) keeps a wrong-looking reflection from being mistaken for a tuning
    // problem.
    if (projection.zNear <= 0.0f || projection.zFar <= projection.zNear) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] ssr: no projection captured yet, effect idle until the "
                   "game establishes a 3D view\n");
            warned = true;
        }
        return false;
    }

    // Cached programs/buffers belong to the GL context that built them. If that context is gone,
    // drop the handles rather than deleting them (the owning context freed them already, and
    // glDelete* now would hit unrelated objects) and rebuild against the current one.
    if (g_state.generation != GetGlContextGeneration()) {
        g_state = PipelineState{};
        g_state.generation = GetGlContextGeneration();
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] ssr: shader init failed, effect disabled until the GL "
                   "context changes\n");
        }
    }
    if (!g_state.initOk) {
        return false;
    }

    EnsureUbo(gl);

    SsrConfigData configData{};
    configData.frustum[0] = projection.left;
    configData.frustum[1] = projection.right;
    configData.frustum[2] = projection.bottom;
    configData.frustum[3] = projection.top;
    configData.params[0] = projection.zNear;
    configData.params[1] = projection.zFar;
    // A zero or negative march length would make every step land on the ray's own origin and
    // divide the distance fade by zero. The smallest sane distance is one step of nothing, which
    // finds no reflection - visibly inert rather than NaN.
    configData.params[2] = (maxDistance > 0.0f) ? maxDistance : 0.0001f;
    configData.params[3] = thickness;
    configData.params2[0] = intensity;
    configData.params2[1] = upThreshold;
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(SsrConfigData), &configData, GL_DYNAMIC_DRAW);
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
        printf("[opengl32_enh_cpp] ssr: glGetError() = 0x%04X after dispatch\n", err);
    }
    return true;
}
