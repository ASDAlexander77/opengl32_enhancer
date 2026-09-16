// See lut_grading.h. GL pipeline mirrors taa.cpp's two-texture-unit setup (inputTex on unit 0,
// lutTex on unit 1, outputImage on image unit 2, TaaConfigBlock-style UBO on point 0), plus a
// GL_TEXTURE_3D built from a parsed .cube file instead of a second 2D texture.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "lut_grading.h"
#include "gl_loader.h"

namespace {

const unsigned int GL_VIEWPORT                 = 0x0BA2;
const unsigned int GL_BACK                     = 0x0405;
const unsigned int GL_TEXTURE_2D               = 0x0DE1;
const unsigned int GL_TEXTURE_3D               = 0x806F;
const unsigned int GL_TEXTURE0                 = 0x84C0;
const unsigned int GL_ACTIVE_TEXTURE           = 0x84E0;
const unsigned int GL_TEXTURE_BINDING_2D       = 0x8069;
const unsigned int GL_TEXTURE_BINDING_3D       = 0x806A;
const unsigned int GL_TEXTURE_MIN_FILTER       = 0x2801;
const unsigned int GL_TEXTURE_MAG_FILTER       = 0x2800;
const unsigned int GL_TEXTURE_WRAP_S           = 0x2802;
const unsigned int GL_TEXTURE_WRAP_T           = 0x2803;
const unsigned int GL_TEXTURE_WRAP_R           = 0x8072;
const unsigned int GL_LINEAR                   = 0x2601;
const unsigned int GL_CLAMP_TO_EDGE            = 0x812F;
const unsigned int GL_RGBA8                    = 0x8058;
const unsigned int GL_RGBA16F                  = 0x881A;
const unsigned int GL_RGBA                     = 0x1908;
const unsigned int GL_FLOAT                    = 0x1406;
const unsigned int GL_WRITE_ONLY               = 0x88B9;
const unsigned int GL_COMPUTE_SHADER           = 0x91B9;
const unsigned int GL_COMPILE_STATUS           = 0x8B81;
const unsigned int GL_LINK_STATUS              = 0x8B82;
const unsigned int GL_CURRENT_PROGRAM          = 0x8B8D;
const unsigned int GL_FRAMEBUFFER              = 0x8D40;
const unsigned int GL_READ_FRAMEBUFFER         = 0x8CA8;
const unsigned int GL_DRAW_FRAMEBUFFER         = 0x8CA9;
const unsigned int GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const unsigned int GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const unsigned int GL_COLOR_ATTACHMENT0        = 0x8CE0;
const unsigned int GL_COLOR_BUFFER_BIT         = 0x00004000;
const unsigned int GL_NEAREST                  = 0x2600;
const unsigned int GL_FRAMEBUFFER_BARRIER_BIT  = 0x00000400;
const unsigned int GL_UNIFORM_BUFFER           = 0x8A11;
const unsigned int GL_UNIFORM_BUFFER_BINDING   = 0x8A28;
const unsigned int GL_DYNAMIC_DRAW             = 0x88E8;
const unsigned int GL_NO_ERROR                 = 0;

const int kMinLutSize = 2;
const int kMaxLutSize = 128;

// inputTex=binding 0 (texture unit 0), lutTex=binding 1 (texture unit 1), outputImage=binding
// 2 (image unit 2 - separate namespace, no collision with either sampler binding),
// LutConfigBlock=binding 0 (uniform buffer binding point 0 - a third separate namespace, no
// collision with inputTex's texture-unit 0). See taa.cpp's equivalent comment.
const char* kLutGradingShaderSource =
    "#version 430\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(binding = 1) uniform sampler3D lutTex;\n"
    "layout(rgba8, binding = 2) uniform writeonly image2D outputImage;\n"
    "layout(std140, binding = 0) uniform LutConfigBlock {\n"
    "    float strength;\n"
    "    float lutSize;\n"
    "};\n"
    "void main() {\n"
    "    ivec2 outSize = imageSize(outputImage);\n"
    "    ivec2 outCoord = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (outCoord.x >= outSize.x || outCoord.y >= outSize.y) { return; }\n"
    "    vec2 uv = (vec2(outCoord) + vec2(0.5)) / vec2(outSize);\n"
    "    vec4 color = texture(inputTex, uv);\n"
    "    vec3 c = clamp(color.rgb, 0.0, 1.0);\n"
    // Domain edges 0 and 1 are the FIRST and LAST texel centers in a .cube LUT, not the
    // texture's outer edges - remap so normalized coordinate 0/1 lands exactly on those
    // centers (the standard LUT half-texel correction) instead of on the texture border.
    "    vec3 lutCoord = (c * (lutSize - 1.0) + 0.5) / lutSize;\n"
    "    vec3 graded = texture(lutTex, lutCoord).rgb;\n"
    "    vec3 result = mix(c, graded, strength);\n"
    "    imageStore(outputImage, outCoord, vec4(result, color.a));\n"
    "}\n";

struct LutConfigData {
    float strength;
    float lutSize;
    float pad[2];
};

struct PipelineState {
    bool initTried = false;
    bool initOk = false;
    unsigned int program = 0;
    unsigned int configUbo = 0;

    bool texturesValid = false;
    int width = 0;
    int height = 0;
    unsigned int inputTexture = 0;
    unsigned int outputTexture = 0;
    unsigned int outputFbo = 0;

    // LUT state: reparsed only when lutPath changes from the previous call, and a failed
    // parse is cached too (lutLoaded stays false) so a missing/bad file isn't re-logged and
    // re-attempted every single frame.
    char lutPathLoaded[256] = "";
    bool lutPathTried = false;
    bool lutLoaded = false;
    unsigned int lutTexture = 0;
    int lutSize = 0;
};

PipelineState g_state;

bool CompileAndLink(const GlComputeApi& gl, unsigned int& outProgram) {
    unsigned int shader = gl.glCreateShader(GL_COMPUTE_SHADER);
    gl.glShaderSource(shader, 1, &kLutGradingShaderSource, nullptr);
    gl.glCompileShader(shader);

    int compiled = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048];
        int logLen = 0;
        gl.glGetShaderInfoLog(shader, sizeof(log), &logLen, log);
        printf("[opengl32_enh_cpp] lut_grading: FAILED to compile compute shader: %s\n", log);
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
        printf("[opengl32_enh_cpp] lut_grading: FAILED to link compute program: %s\n", log);
        gl.glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

void EnsureTextures(const GlComputeApi& gl, int width, int height) {
    if (g_state.texturesValid && g_state.width == width && g_state.height == height) {
        return;
    }

    if (g_state.texturesValid) {
        unsigned int textures[2] = {g_state.inputTexture, g_state.outputTexture};
        gl.glDeleteTextures(2, textures);
        gl.glDeleteFramebuffers(1, &g_state.outputFbo);
        g_state.texturesValid = false;
    }

    unsigned int textures[2] = {0, 0};
    gl.glGenTextures(2, textures);
    unsigned int inputTexture = textures[0];
    unsigned int outputTexture = textures[1];

    gl.glBindTexture(GL_TEXTURE_2D, inputTexture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    gl.glBindTexture(GL_TEXTURE_2D, outputTexture);
    gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    unsigned int fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    g_state.inputTexture = inputTexture;
    g_state.outputTexture = outputTexture;
    g_state.outputFbo = fbo;
    g_state.width = width;
    g_state.height = height;
    g_state.texturesValid = true;
}

void EnsureUbo(const GlComputeApi& gl) {
    if (g_state.configUbo != 0) {
        return;
    }
    gl.glGenBuffers(1, &g_state.configUbo);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(LutConfigData), nullptr, GL_DYNAMIC_DRAW);
}

// Trim/tokenize helpers, same style as config.cpp's Trim() (duplicated locally rather than
// shared - config.cpp keeps it in its own anonymous namespace, and this parser's needs -
// numeric tokenizing - are different enough that sharing isn't a clean fit).
void Trim(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') {
        ++start;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

// Parses an Adobe/Iridas ".cube" 3D LUT: a LUT_3D_SIZE N header line, then N^3 "R G B" float
// triples in the standard order (R fastest, then G, then B - the same x-fastest order a
// GL_TEXTURE_3D upload expects, so no reordering is needed). TITLE/DOMAIN_MIN/DOMAIN_MAX and
// blank/comment ('#') lines are skipped. Returns false (leaving outSize/outData untouched) on
// any I/O or format error - never partially fills outData.
bool ParseCubeFile(const char* path, int& outSize, std::vector<float>& outData) {
    FILE* f = fopen(path, "r");
    if (f == nullptr) {
        printf("[opengl32_enh_cpp] lut_grading: no LUT file at '%s'\n", path);
        return false;
    }

    int size = 0;
    std::vector<float> data;
    char line[256];
    while (fgets(line, sizeof(line), f) != nullptr) {
        Trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (strncmp(line, "LUT_3D_SIZE", 11) == 0) {
            size = atoi(line + 11);
            continue;
        }
        // Any other alphabetic keyword line (TITLE, DOMAIN_MIN, DOMAIN_MAX, LUT_1D_SIZE, ...)
        // - skip; only numeric data lines are consumed below.
        if ((line[0] >= 'A' && line[0] <= 'Z') || (line[0] >= 'a' && line[0] <= 'z')) {
            continue;
        }
        float r = 0.0f, g = 0.0f, b = 0.0f;
        if (sscanf(line, "%f %f %f", &r, &g, &b) != 3) {
            printf("[opengl32_enh_cpp] lut_grading: '%s': malformed data line '%s'\n", path, line);
            fclose(f);
            return false;
        }
        // Stored as RGBA (alpha padded to 1.0) rather than RGB - a 4-component texture
        // upload sidesteps 3-component row-packing pitfalls entirely, matching every other
        // texture in this codebase (all GL_RGBA8).
        data.push_back(r);
        data.push_back(g);
        data.push_back(b);
        data.push_back(1.0f);
    }
    fclose(f);

    if (size < kMinLutSize || size > kMaxLutSize) {
        printf("[opengl32_enh_cpp] lut_grading: '%s': LUT_3D_SIZE %d missing or out of range [%d, %d]\n",
               path, size, kMinLutSize, kMaxLutSize);
        return false;
    }
    size_t expected = static_cast<size_t>(size) * size * size * 4;
    if (data.size() != expected) {
        printf("[opengl32_enh_cpp] lut_grading: '%s': expected %zu RGBA values for size %d, got %zu\n",
               path, expected, size, data.size());
        return false;
    }

    outSize = size;
    outData = std::move(data);
    return true;
}

// Reparses lutPath into g_state.lutTexture only when it differs from the last call (or on the
// very first call). A failed parse is cached (lutLoaded stays false) rather than retried every
// frame; it's only retried if lutPath itself changes again.
void EnsureLut(const GlComputeApi& gl, const char* lutPath) {
    if (g_state.lutPathTried && strcmp(g_state.lutPathLoaded, lutPath) == 0) {
        return;
    }
    g_state.lutPathTried = true;
    strncpy(g_state.lutPathLoaded, lutPath, sizeof(g_state.lutPathLoaded) - 1);
    g_state.lutPathLoaded[sizeof(g_state.lutPathLoaded) - 1] = '\0';
    g_state.lutLoaded = false;

    if (lutPath[0] == '\0') {
        return;
    }

    int size = 0;
    std::vector<float> data;
    if (!ParseCubeFile(lutPath, size, data)) {
        return;
    }

    if (g_state.lutTexture == 0) {
        gl.glGenTextures(1, &g_state.lutTexture);
    }
    gl.glBindTexture(GL_TEXTURE_3D, g_state.lutTexture);
    gl.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl.glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, size, size, size);
    gl.glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, size, size, size, GL_RGBA, GL_FLOAT, data.data());

    printf("[opengl32_enh_cpp] lut_grading: loaded '%s' (%dx%dx%d)\n", lutPath, size, size, size);
    g_state.lutSize = size;
    g_state.lutLoaded = true;
}

}  // namespace

void ApplyLutGrading(const char* lutPath, float strength) {
    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        static bool warned = false;
        if (!warned) {
            printf("[opengl32_enh_cpp] lut_grading: GL 4.3 compute support unavailable on this "
                   "context, effect disabled\n");
            warned = true;
        }
        return;
    }

    if (!g_state.initTried) {
        g_state.initTried = true;
        g_state.initOk = CompileAndLink(gl, g_state.program);
        if (!g_state.initOk) {
            printf("[opengl32_enh_cpp] lut_grading: shader init failed, effect disabled for "
                   "the rest of this process\n");
        }
    }
    if (!g_state.initOk) {
        return;
    }

    EnsureLut(gl, lutPath);
    if (!g_state.lutLoaded) {
        return;
    }

    int viewport[4] = {0, 0, 0, 0};
    gl.glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    if (width <= 0 || height <= 0) {
        return;
    }

    int savedActiveTexture = 0;
    gl.glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedTextureBinding0 = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTextureBinding0);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    int savedTextureBinding1 = 0;
    gl.glGetIntegerv(GL_TEXTURE_BINDING_3D, &savedTextureBinding1);
    gl.glActiveTexture(GL_TEXTURE0);
    int savedProgram = 0;
    gl.glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
    int savedReadFbo = 0;
    gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    int savedDrawFbo = 0;
    gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFbo);
    int savedUniformBuffer = 0;
    gl.glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &savedUniformBuffer);

    EnsureTextures(gl, width, height);
    EnsureUbo(gl);

    auto restoreState = [&]() {
        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, (unsigned int)savedReadFbo);
        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (unsigned int)savedDrawFbo);
        gl.glUseProgram((unsigned int)savedProgram);
        gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, (unsigned int)savedUniformBuffer);
        gl.glBindBuffer(GL_UNIFORM_BUFFER, (unsigned int)savedUniformBuffer);
        gl.glActiveTexture(GL_TEXTURE0 + 1);
        gl.glBindTexture(GL_TEXTURE_3D, (unsigned int)savedTextureBinding1);
        gl.glActiveTexture(GL_TEXTURE0);
        gl.glBindTexture(GL_TEXTURE_2D, (unsigned int)savedTextureBinding0);
        gl.glActiveTexture((unsigned int)savedActiveTexture);
    };

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.glReadBuffer(GL_BACK);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.inputTexture);
    gl.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    unsigned int captureErr = gl.glGetError();
    if (captureErr != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] lut_grading: glGetError() = 0x%04X after capture, skipping "
               "this frame\n", captureErr);
        restoreState();
        return;
    }

    LutConfigData configData{};
    configData.strength = strength;
    configData.lutSize = static_cast<float>(g_state.lutSize);
    gl.glBindBuffer(GL_UNIFORM_BUFFER, g_state.configUbo);
    gl.glBufferData(GL_UNIFORM_BUFFER, sizeof(LutConfigData), &configData, GL_DYNAMIC_DRAW);
    gl.glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_state.configUbo);

    gl.glUseProgram(g_state.program);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, g_state.inputTexture);
    gl.glActiveTexture(GL_TEXTURE0 + 1);
    gl.glBindTexture(GL_TEXTURE_3D, g_state.lutTexture);
    gl.glBindImageTexture(2, g_state.outputTexture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);

    unsigned int groupsX = (unsigned int)((width + 7) / 8);
    unsigned int groupsY = (unsigned int)((height + 7) / 8);
    gl.glDispatchCompute(groupsX, groupsY, 1);
    gl.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, g_state.outputFbo);
    gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    unsigned int err = gl.glGetError();
    if (err != GL_NO_ERROR) {
        printf("[opengl32_enh_cpp] lut_grading: glGetError() = 0x%04X after dispatch\n", err);
    }

    restoreState();
}
