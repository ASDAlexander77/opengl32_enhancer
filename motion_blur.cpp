// See motion_blur.h.
#include <cstdio>

#include "motion_blur.h"
#include "gl_loader.h"

void MotionBlurReprojection(const CameraMatrix& current, const CameraMatrix& previous,
                            float out[16]) {
    // Column-major throughout: element (row, col) is m[col * 4 + row]. A view matrix is
    // [R | t] with R the world->view rotation, so R[row][col] == current.m[col * 4 + row] and
    // t[row] == current.m[12 + row].
    //
    // inverse([R | t]) == [R-transpose | -R-transpose * t] because R is orthonormal. Doing it
    // this way rather than with a general 4x4 inversion is not only cheaper, it cannot produce
    // a near-singular result on a matrix that is rigid by construction.
    float inv[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // (R-transpose)[row][col] == R[col][row] == current.m[row * 4 + col]
            inv[col * 4 + row] = current.m[row * 4 + col];
        }
    }
    for (int row = 0; row < 3; ++row) {
        float acc = 0.0f;
        for (int k = 0; k < 3; ++k) {
            acc += current.m[row * 4 + k] * current.m[12 + k];
        }
        inv[12 + row] = -acc;
    }

    // out = previous * inv
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float acc = 0.0f;
            for (int k = 0; k < 4; ++k) {
                acc += previous.m[k * 4 + row] * inv[col * 4 + k];
            }
            out[col * 4 + row] = acc;
        }
    }
}
