#pragma once

// Draws a small "FX" badge into the top-right corner of `texture`, in place - a lightweight
// visual confirmation that the post-process pipeline actually ran this frame. It exists to
// answer one question at a glance: is the proxy even loaded and running, or is "the game looks
// unchanged" because the whole pipeline never fired (bad ini path, DLL not picked up, GL 4.3
// unavailable)? A stage that quietly no-ops (bad lutPath, etc.) still leaves the badge up,
// since that's a different failure the existing printf logging already covers - see
// GetAnaxConfig().fxIndicator, opengl32_enhancer.ini's `fxIndicator=1/0` (default on).
//
// This is NOT a pipeline stage: it doesn't read config.stages, has no src/dst framing, and
// always runs once, directly on the pipeline's own final texture, right after every listed
// stage - so it draws on top of whatever the chain produced, unconditionally. Called from
// post_effects.cpp only when config.fxIndicator is set and the pipeline actually ran (i.e.
// config.stageCount > 0, already guaranteed by the point post_effects.cpp gets here).

// Runs the badge draw, called from post_effects.cpp's ApplySelectedEffect(). `texture` is the
// width x height RGBA16F 2D texture the pipeline is about to present - read-modify-write in
// place, unlike every other stage here (which reads one texture and writes a different one).
// A texture too small to fit the badge with its corner margin is left untouched rather than
// drawing a clipped/garbled badge. No-ops silently if GL 4.3 compute support is unavailable
// (post_effects.cpp already checked this before capturing the frame, so that's expected to
// never actually happen in practice, but this function makes the same check independently
// rather than assuming its caller did).
void DrawFxIndicator(unsigned int texture, int width, int height);
