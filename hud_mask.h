#pragma once

// Puts the HUD back after a scene-space stage.
//
// ssao, dof, fog, ssr, lightshafts and depthvignette act on the 3D world, but they run on the
// finished frame, after the game has drawn its HUD, menus and text over that world - and a HUD
// pixel still has the world's depth (or the sky's) under it. Left alone, fog tints the HUD
// toward the fog colour, ssr mirrors floor reflections into it, lightshafts streak across it and
// dof blurs it. Rather than teach six shaders about overlays, this one pass runs after any of
// them and copies every HUD pixel back from the stage's own INPUT, so the stage's change to that
// pixel is simply undone.
//
// "HUD" is decided the same way taa.cpp and motion_blur.cpp decide it: wherever the finished
// frame (captureTex) differs from the pre-HUD world-only frame (worldTex, see world_capture.h)
// by more than 8-bit quantisation, the game drew an overlay there.
//
// `stageOutput` is modified in place (image load/store). `stageInput` is what the stage read.
// All four textures must be width x height. Returns false, leaving stageOutput as the stage
// wrote it, when GL 4.3 compute is unavailable or the shader failed to build.
bool ApplyHudRestore(unsigned int stageInput, unsigned int stageOutput, unsigned int worldTex,
                     unsigned int captureTex, int width, int height);
