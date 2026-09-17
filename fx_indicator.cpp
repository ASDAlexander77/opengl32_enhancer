// See fx_indicator.h. One compute dispatch, sized to just the badge's own small footprint (not
// the whole frame) and positioned via an origin uniform - the shader draws a flat semi-
// transparent plate with a 1px border, then a hardcoded 5x7 bitmap "F" and "X" glyph scaled up,
// blended on top. Unlike every other stage in this pipeline, this READS AND WRITES THE SAME
// texture (image2D, not a separate writeonly output) - safe here because every invocation only
// ever touches its own texel and the plate/glyph math needs no neighbor samples, so there's no
// intra-dispatch race to worry about.
#include <cstdio>

#include "fx_indicator.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_TEXTURE_2D                    = 0x0DE1;
const unsigned int GL_RGBA16F                       = 0x881A;
const unsigned int GL_READ_WRITE                    = 0x88BA;
const unsigned int GL_COMPUTE_SHADER                = 0x91B9;
const unsigned int GL_COMPILE_STATUS                = 0x8B81;
const unsigned int GL_LINK_STATUS                   = 0x8B82;
const unsigned int GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x00000020;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT       = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER                = 0x8A11;
const unsigned int GL_DYNAMIC_DRAW                  = 0x88E8;
const unsigned int GL_NO_ERROR                      = 0;

// Badge geometry - kept in sync with the identical constants baked into kShaderSource below
// (GLSL has no way to share a #define with the C++ side here, so these two copies must agree).
// PLATE_W/H is the whole badge footprint the shader dispatches over; kMargin is its distance
// from the texture's top-right corner.
const int kPlateWidth = 45;
const int kPlateHeight = 33;
const int kMargin = 8;

const char* kShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(rgba16f, binding = 0) uniform image2D targetImage;\n"
    "layout(std140, binding = 0) uniform FxIndicatorConfigBlock {\n"
    "    vec4 params;\n"  // .x = originX, .y = originY, .zw unused
    "};\n"
    "const int PLATE_W = 45;\n"
    "const int PLATE_H = 33;\n"
    "const int SCALE = 3;\n"
    "const int GLYPH_W = 5;\n"
    "const int GLYPH_H = 7;\n"
    // 5x7 bitmap font, one row per array entry, 5 bits used (MSB = leftmost column).
    "const int F_ROWS[7] = int[](31, 16, 16, 30, 16, 16, 16);\n"
    "const int X_ROWS[7] = int[](17, 10, 4, 4, 4, 10, 17);\n"
    "bool GlyphBit(bool useX, int col, int row) {\n"
    "    int bits = useX ? X_ROWS[row] : F_ROWS[row];\n"
    "    return ((bits >> (GLYPH_W - 1 - col)) & 1) != 0;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 local = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (local.x >= PLATE_W || local.y >= PLATE_H) { return; }\n"
    "    ivec2 origin = ivec2(int(params.x), int(params.y));\n"
    "    ivec2 coord = origin + local;\n"
    "    ivec2 imgSize = imageSize(targetImage);\n"
    "    if (coord.x < 0 || coord.y < 0 || coord.x >= imgSize.x || coord.y >= imgSize.y) { return; }\n"
    "    bool isBorder = local.x == 0 || local.y == 0 || local.x == PLATE_W - 1 || local.y == PLATE_H - 1;\n"
    "    int textOriginX = 6;\n"
    "    int textOriginY = (PLATE_H - GLYPH_H * SCALE) / 2;\n"
    "    int glyphOriginF = textOriginX;\n"
    "    int glyphOriginX = textOriginX + GLYPH_W * SCALE + SCALE;\n"
    "    bool isGlyph = false;\n"
    "    if (!isBorder) {\n"
    "        int ly = local.y - textOriginY;\n"
    "        if (ly >= 0 && ly < GLYPH_H * SCALE) {\n"
    // GL image/texture y increases UPWARD on screen (texel (0,0) is the bottom-left - see
    // fx_indicator.cpp's header comment), but F_ROWS/X_ROWS are written with row 0 as the
    // glyph's visual TOP. Without this flip, increasing ly (moving up-screen within the
    // glyph) would walk the rows top-to-bottom in ARRAY order but bottom-to-top on SCREEN,
    // drawing every glyph upside down.
    "            int row = GLYPH_H - 1 - (ly / SCALE);\n"
    "            int lxF = local.x - glyphOriginF;\n"
    "            int lxX = local.x - glyphOriginX;\n"
    "            if (lxF >= 0 && lxF < GLYPH_W * SCALE) {\n"
    "                isGlyph = GlyphBit(false, lxF / SCALE, row);\n"
    "            } else if (lxX >= 0 && lxX < GLYPH_W * SCALE) {\n"
    "                isGlyph = GlyphBit(true, lxX / SCALE, row);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    vec4 bg = imageLoad(targetImage, coord);\n"
    "    vec3 overlayColor;\n"
    "    float overlayAlpha;\n"
    "    if (isGlyph) {\n"
    "        overlayColor = vec3(1.0);\n"
    "        overlayAlpha = 1.0;\n"
    "    } else if (isBorder) {\n"
    "        overlayColor = vec3(0.85);\n"
    "        overlayAlpha = 0.9;\n"
    "    } else {\n"
    "        overlayColor = vec3(0.05);\n"
    "        overlayAlpha = 0.55;\n"
    "    }\n"
    "    vec3 result = mix(bg.rgb, overlayColor, overlayAlpha);\n"
    "    imageStore(targetImage, coord, vec4(result, bg.a));\n"
    "}\n";

struct FxIndicatorConfigData {
    float originX;
    float originY;
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
    gl.glShaderSource(shader, 1, &kShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] fx_indicator: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] fx_indicator: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

}  // namespace

void DrawFxIndicator(unsigned int texture, int width, int height) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        // No warning log here, unlike every other stage - this runs on every frame the
        // pipeline is active, and if GL 4.3 compute were really unavailable, every actual
        // effect stage already printed that warning once before this ever gets called.
        return;
    }

    // Cached programs belong to the GL context that built them. If that context is gone, drop
    // the handle rather than deleting it (the owning context freed it already, and glDelete*
    // now would hit an unrelated object) and rebuild against the current one.
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
            gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(FxIndicatorConfigData), nullptr, GL_DYNAMIC_DRAW);
        }
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] fx_indicator: shader init failed, badge disabled until "
                   "the GL context changes\n");
        }
    }
    if (!g_state.initOk) {
        return;
    }

    // Too small to fit the badge with its corner margin - leave the frame alone rather than
    // drawing a clipped/garbled badge.
    if (width < kPlateWidth + 2 * kMargin || height < kPlateHeight + 2 * kMargin) {
        return;
    }

    // GL image/texture y increases UPWARD on screen (texel (0,0) is the bottom-left), so
    // "kMargin from the TOP" means the plate's origin (its bottom-left texel) sits kMargin
    // below the texture's top edge, not at kMargin itself - that would be the BOTTOM margin.
    FxIndicatorConfigData configData{};
    configData.originX = (float)(width - kMargin - kPlateWidth);
    configData.originY = (float)(height - kMargin - kPlateHeight);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(FxIndicatorConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glBindImageTexture(0, texture, 0, 0, 0, GL_READ_WRITE, GL_RGBA16F);
    unsigned int groupsX = (unsigned int)((kPlateWidth + 7) / 8);
    unsigned int groupsY = (unsigned int)((kPlateHeight + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    // The upcoming blit reads this same texture through the default framebuffer's blit path.
    gl.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] fx_indicator: glGetError() = 0x%04X after dispatch\n", err);
    }
}
