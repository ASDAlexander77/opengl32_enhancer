#pragma once

// The thing the config editor applies effects TO: either a synthetic 3D room rendered live, or a
// real game frame loaded from a dump (see frame_dump.h). Both end up in the same place - the
// default framebuffer's color AND depth, with the projection registered through
// projection_capture.h - because that is exactly what ApplySelectedEffect() reads, so the editor
// exercises the real pipeline rather than a reimplementation of it.
//
// This lives in its own translation unit purely so it can include <GL/gl.h>: ImGui's OpenGL3
// backend ships its own GL loader, and the two conflict if pulled into one file.

// Draws the synthetic room into the default framebuffer and registers its projection. The room
// is built at Quake II's scale (roughly 1 unit = 1 inch) specifically so that world-unit
// settings - ssaoRadius above all - land in the same numeric range they will need in the real
// game, instead of being tuned against a scene whose scale silently differs by an order of
// magnitude. Sets the viewport to the full window.
void RenderSyntheticScene(int windowWidth, int windowHeight);

// Loads a frame dump and uploads its color and depth into textures for RenderLoadedFrame().
// Returns false (and logs) if the file is missing or malformed; any previously loaded frame is
// kept in that case, so a failed load never blanks the editor.
bool LoadFrameDump(const char* path);

// True once LoadFrameDump() has succeeded at least once.
bool HasLoadedFrame();

// Whether the loaded dump carried a projection. A dump taken before the game first set up a 3D
// view has none, which means ssao cannot run against it - see projection_capture.h.
bool LoadedFrameHasProjection();

// Dimensions of the loaded dump, or zeroes if none is loaded.
void LoadedFrameSize(int& outWidth, int& outHeight);

// Blits the loaded frame's color and depth into the default framebuffer and re-registers its
// projection, then sets the viewport to the (aspect-preserving) region it occupies. The scale is
// preserved rather than stretched because the projection's near-plane extents encode the frame's
// aspect ratio: stretching the image while leaving those extents alone would make every
// unprojected position subtly wrong, and SSAO with it.
void RenderLoadedFrame(int windowWidth, int windowHeight);
