cmake_minimum_required(VERSION 3.20)

project(anax_enhancer CXX)

# The tslang toolchain's runtime libraries (TypeScriptAsyncRuntime.lib, LLVMSupport.lib -
# these link into every tslang-built program, not just the compiler) are only built for
# x64 right now; a genuine wrapper.ts x86 build needs an x86 LLVM/MLIR rebuild first (see
# generators/gen_wrapper_cpp.py's header comment for the full story). So the two
# architectures build entirely different things here:
#   - x64 (the "default" preset): the full TSLANG-based project - anax_enhancer.exe,
#     opengl32_enh.dll (from wrapper.ts), test.exe - plus the plain-C++ proxy below.
#   - x86 (the "x86" preset): just the plain-C++ proxy below (opengl32_enh32.dll, generated
#     by gen_wrapper_cpp.py into wrapper.cpp). No TSLANG involved on this side at all.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)

message(FATAL_ERROR "64bit: Unsupported for now")

endif() # CMAKE_SIZEOF_VOID_P EQUAL 8 (the TSLANG-based project)

# OpenGL proxy built from plain C++ (wrapper.cpp) - see generators/gen_wrapper_cpp.py.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
set (WRAPPER_NAME_CXX "opengl32_enh64")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
set (WRAPPER_NAME_CXX "opengl32_enh32")
else()
    message(FATAL_ERROR "Unsupported pointer size: ${CMAKE_SIZEOF_VOID_P} (expected 4 or 8)")
endif()

# Plain C++ proxy - see generators/gen_wrapper_cpp.py. Exports under the real function
# names via the .def file (a __declspec(dllexport) alone would get __stdcall's "@N"
# decoration on x86, which wouldn't match what a host app imports by name).
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp hdr_look.cpp bloom.cpp lut_grading.cpp taa.cpp)
set_target_properties(${WRAPPER_NAME_CXX} PROPERTIES
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/wrapper.def"
)

# Loads opengl32_enh32.dll by name (same output directory) and calls a couple of
# functions through it, the same sanity check test.ts does for the x64 build.
add_executable(${WRAPPER_NAME_CXX}_test wrapper_test.cpp)
add_dependencies(${WRAPPER_NAME_CXX}_test ${WRAPPER_NAME_CXX})

# Exercises config.cpp's INI parsing directly (no DLL/GL involved) - see config_test.cpp.
add_executable(config_test config_test.cpp config.cpp)

# Creates a real GL context and checks gl_loader.cpp resolves every GL 4.3 compute-shader
# entry point it needs - see gl_loader_test.cpp.
add_executable(gl_loader_test gl_loader_test.cpp gl_loader.cpp)
target_link_libraries(gl_loader_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyBilinearUpscale()'s
# GPU pipeline (capture -> compute dispatch -> blit) reproduces it without a GL error - see
# bilinear_upscale_test.cpp.
add_executable(bilinear_upscale_test bilinear_upscale_test.cpp bilinear_upscale.cpp gl_loader.cpp)
target_link_libraries(bilinear_upscale_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks both ApplyNVScaler and
# ApplyNVSharpen's GPU pipelines run to completion with no GL error - see nis_effect_test.cpp.
add_executable(nis_effect_test nis_effect_test.cpp nis_effect.cpp gl_loader.cpp)
target_link_libraries(nis_effect_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyHdrLook()'s GPU
# pipeline at strength=0 (must reproduce the input) and strength=0.8 (must run with no GL
# error) - see hdr_look_test.cpp.
add_executable(hdr_look_test hdr_look_test.cpp hdr_look.cpp gl_loader.cpp)
target_link_libraries(hdr_look_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and calls ApplyTaa() four times in a
# row, checking the ping-pong history buffer and neighborhood clamp reproduce a static
# scene's color with no drift and no GL error - see taa_test.cpp.
add_executable(taa_test taa_test.cpp taa.cpp gl_loader.cpp)
target_link_libraries(taa_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, writes an identity .cube LUT fixture,
# and checks ApplyLutGrading()'s GPU pipeline (parse .cube -> 3D texture -> dispatch -> blit)
# at strength=1 (identity LUT reproduces the input), strength=0 (exact passthrough), and a
# missing LUT file (no-op, no GL error) - see lut_grading_test.cpp.
add_executable(lut_grading_test lut_grading_test.cpp lut_grading.cpp gl_loader.cpp)
target_link_libraries(lut_grading_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyBloom()'s four-pass GPU
# pipeline (extract -> blur x2 -> composite -> blit) at intensity=0 (must reproduce the input
# exactly, no bloom contribution) and a nonzero intensity (must run with no GL error) - see
# bloom_test.cpp.
add_executable(bloom_test bloom_test.cpp bloom.cpp gl_loader.cpp)
target_link_libraries(bloom_test opengl32 gdi32 user32)
