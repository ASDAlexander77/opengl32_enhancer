#pragma once

// debugStats=1: where does the frame time go?
//
// Measures each frame without stalling it, and reports every two seconds - to the log and to
// the game's window title - so a slow session can be traced to a specific stage instead of
// guessed at:
//
//   - GPU time per pipeline segment, from GL_TIMESTAMP queries read back several frames later
//     (never waited on - a result that is not ready yet is dropped, not blocked for). Segments
//     are the time between consecutive marks: "game" is everything the game itself drew since
//     the previous swap, then "capture", then one per effect= stage (including that stage's
//     colour-space conversion and HUD restore), then "present" (resolve + blit).
//   - CPU time: the game's own frame, our post-processing, and how long the real SwapBuffers
//     blocked - a large swap wait means the GPU, not the CPU, is the bottleneck.
//   - Texture uploads and how many went through textureEffect (each one is a synchronous
//     readback - see texture_effect.cpp), and draw calls per frame.
//
// Every entry point is a no-op costing one config read when debugStats is off.

// Right after the real wglSwapBuffers returns: opens the next frame.
void PerfFrameBegin();

// Records a GPU timestamp closing the segment named `label` (a string literal or other
// string that outlives the process - only the pointer is kept). Also used for the first
// mark of the post-process pass, "game".
void PerfGpuMark(const char* label);

// Right before the real wglSwapBuffers: closes this frame's CPU accounting and, once due,
// writes the report. `hdc` is the swap's DC, used to find the window for the title.
void PerfFrameEnd(void* hdc);

// Counters.
void PerfCountDraw();
void PerfCountUpload(bool throughTextureEffect, double cpuMs);

// A high-resolution CPU clock in milliseconds, for callers timing their own work.
double PerfNowMs();
