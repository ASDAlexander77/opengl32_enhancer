// See frame_dump.h.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "frame_dump.h"

namespace {

const char kMagic[8] = {'A', 'N', 'A', 'X', 'F', 'R', 'M', '1'};

// A dump is written straight from a swap hook with whatever the viewport reported, so a wild
// width/height here means the header is garbage rather than that someone is running at an
// exotic resolution. Bounding it keeps a corrupt file from being turned into a multi-gigabyte
// allocation on load.
const int kMaxDimension = 16384;

}  // namespace

bool WriteFrameDump(const char* path, int width, int height, bool hasProjection,
                     const ProjectionParams& projection,
                     const unsigned char* color, const float* depth) {
    if (width <= 0 || height <= 0 || color == nullptr || depth == nullptr) {
        return false;
    }

    FILE* f = fopen(path, "wb");
    if (f == nullptr) {
        printf("[opengl32_enh_cpp] frame_dump: could not open '%s' for writing\n", path);
        return false;
    }

    FrameDumpHeader header{};
    memcpy(header.magic, kMagic, sizeof(kMagic));
    header.width = width;
    header.height = height;
    header.hasProjection = hasProjection ? 1 : 0;
    header.projection = projection;

    size_t colorBytes = (size_t)width * (size_t)height * 4;
    size_t depthCount = (size_t)width * (size_t)height;

    bool ok = fwrite(&header, sizeof(header), 1, f) == 1 &&
              fwrite(color, 1, colorBytes, f) == colorBytes &&
              fwrite(depth, sizeof(float), depthCount, f) == depthCount;
    fclose(f);

    if (!ok) {
        printf("[opengl32_enh_cpp] frame_dump: FAILED writing '%s' (disk full?)\n", path);
        return false;
    }
    printf("[opengl32_enh_cpp] frame_dump: wrote '%s' (%dx%d, projection=%s)\n",
           path, width, height, hasProjection ? "yes" : "no");
    return true;
}

bool ReadFrameDump(const char* path, FrameDumpHeader& outHeader,
                    unsigned char** outColor, float** outDepth) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        printf("frame_dump: no file at '%s'\n", path);
        return false;
    }

    FrameDumpHeader header{};
    if (fread(&header, sizeof(header), 1, f) != 1) {
        printf("frame_dump: '%s' is too short to hold a header\n", path);
        fclose(f);
        return false;
    }
    if (memcmp(header.magic, kMagic, sizeof(kMagic)) != 0) {
        printf("frame_dump: '%s' is not a frame dump (bad magic)\n", path);
        fclose(f);
        return false;
    }
    if (header.width <= 0 || header.height <= 0 ||
        header.width > kMaxDimension || header.height > kMaxDimension) {
        printf("frame_dump: '%s' has implausible dimensions %dx%d\n", path, header.width, header.height);
        fclose(f);
        return false;
    }

    size_t texelCount = (size_t)header.width * (size_t)header.height;
    unsigned char* color = (unsigned char*)malloc(texelCount * 4);
    float* depth = (float*)malloc(texelCount * sizeof(float));
    if (color == nullptr || depth == nullptr) {
        printf("frame_dump: out of memory loading '%s'\n", path);
        free(color);
        free(depth);
        fclose(f);
        return false;
    }

    bool ok = fread(color, 1, texelCount * 4, f) == texelCount * 4 &&
              fread(depth, sizeof(float), texelCount, f) == texelCount;
    fclose(f);

    if (!ok) {
        printf("frame_dump: '%s' is truncated - header says %dx%d but the body is short\n",
               path, header.width, header.height);
        free(color);
        free(depth);
        return false;
    }

    outHeader = header;
    *outColor = color;
    *outDepth = depth;
    return true;
}

void FreeFrameDump(unsigned char* color, float* depth) {
    free(color);
    free(depth);
}
