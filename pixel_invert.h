// Captures the current back buffer's pixels via glReadPixels and inverts their RGB
// channels in place before it goes to screen. See pixel_invert.cpp for the implementation
// and generators/gen_wrapper_cpp.py for where wglSwapBuffers calls this before forwarding
// to the real wglSwapBuffers.
#pragma once

void InvertBackBufferColors();
