#pragma once

#include "projection_capture.h"

// A single captured game frame written to disk: the color the game rendered, its depth buffer,
// and the projection it was rendered with. This is what lets the standalone config editor (see
// config_editor.cpp) tune settings against REAL game content instead of a synthetic stand-in -
// which matters because the settings that are hardest to get right are exactly the ones a
// synthetic scene cannot model honestly. ssaoRadius is denominated in the game's own world
// units, and bloomThreshold depends on the actual brightness distribution of the game's art;
// both are guesses until they are tuned against a real frame.
//
// Deliberately a dumb uncompressed format. This project has no image-decoding dependency (see
// post_effects_test.cpp's note on the same subject) and these files are transient developer
// artifacts, not something shipped or kept - so the format optimizes for being trivial to write
// from inside a swap hook and trivial to read back, not for size.
//
// Layout: FrameDumpHeader, then width*height RGBA8 texels of color, then width*height floats of
// raw (non-linear, hardware) depth in 0..1. Both are stored in OpenGL's row order - row 0 is the
// BOTTOM of the image - so they round-trip through glReadPixels/glTexSubImage2D unflipped.

struct FrameDumpHeader {
    char magic[8];        // "ANAXFRM1", checked on load
    int width;
    int height;
    int hasProjection;    // 0 when the game had not set up a 3D view yet - see projection_capture.h
    ProjectionParams projection;
};

// Writes a dump to `path`. `color` is width*height RGBA8 texels and `depth` is width*height
// floats, both as glReadPixels returned them. Returns false (and logs) if the file could not be
// written; never throws and never partially reports success.
bool WriteFrameDump(const char* path, int width, int height, bool hasProjection,
                     const ProjectionParams& projection,
                     const unsigned char* color, const float* depth);

// Reads a dump written by WriteFrameDump. On success, `outColor` is resized to width*height*4
// bytes and `outDepth` to width*height floats. Returns false (and logs) on a missing file, a bad
// magic, or a truncated/implausible body - callers must treat the out-params as untouched then.
bool ReadFrameDump(const char* path, FrameDumpHeader& outHeader,
                    unsigned char** outColor, float** outDepth);

// Frees the buffers ReadFrameDump allocated. Safe to call with nullptr.
void FreeFrameDump(unsigned char* color, float* depth);
