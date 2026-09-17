cmake_minimum_required(VERSION 3.20)

project(opengl32_enhancer CXX)

# Ninja (this project's only generator - see CMakePresets.json) is single-config: whatever
# CMAKE_BUILD_TYPE is at configure time is the only thing that build directory can ever
# produce, unlike a Visual Studio/multi-config generator where it's chosen per-build. Default
# to Debug when the preset/command line didn't set one, so a bare `cmake .` still works the
# way it always has; pick "x86-release" (see CMakePresets.json) or pass
# -DCMAKE_BUILD_TYPE=Release yourself for an optimized build.
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# The tslang toolchain's runtime libraries (TypeScriptAsyncRuntime.lib, LLVMSupport.lib -
# these link into every tslang-built program, not just the compiler) are only built for
# x64 right now; a genuine wrapper.ts x86 build needs an x86 LLVM/MLIR rebuild first (see
# generators/gen_wrapper_cpp.py's header comment for the full story). So the two
# architectures build entirely different things here:
#   - x64 (the "default" preset): the full TSLANG-based project - opengl32_enhancer.exe,
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
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp config.cpp gl_loader.cpp post_effects.cpp bilinear_upscale.cpp nis_effect.cpp hdr_look.cpp bloom.cpp lut_grading.cpp vignette.cpp chromatic_aberration.cpp taa.cpp dither.cpp texture_effect.cpp texture_filter.cpp smaa.cpp fsr.cpp cas.cpp nr.cpp local_contrast.cpp fx_indicator.cpp)
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

# Creates a real GL context and checks ApplyCas() reproduces a flat image and sharpens a
# mid-contrast edge - see cas_test.cpp.
add_executable(cas_test cas_test.cpp cas.cpp gl_loader.cpp)
target_link_libraries(cas_test opengl32 gdi32 user32)

# Creates a real GL context and checks ApplyFsr()'s two passes (EASU then RCAS) reproduce a
# flat image, respond near a hard edge, and survive the reduced-scale upscale path - see
# fsr_test.cpp.
add_executable(fsr_test fsr_test.cpp fsr.cpp gl_loader.cpp)
target_link_libraries(fsr_test opengl32 gdi32 user32)

# Runs an effect, destroys the GL context, creates a new one and runs the effect again -
# checks cached GL objects are rebuilt rather than reused across contexts (the failure a
# vid_restart / video mode change would otherwise cause) - see gl_context_change_test.cpp.
add_executable(gl_context_change_test gl_context_change_test.cpp pixel_invert.cpp gl_loader.cpp)
target_link_libraries(gl_context_change_test opengl32 gdi32 user32)

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

# Creates a real GL context, clears to a known color, and checks ApplyDither()'s GPU pipeline
# at strength=0 (must reproduce the input exactly) and strength=1.0 (must run with no GL
# error, and stay within +-1 LSB of the input since the dither offset is bounded to +-0.5 of
# one 8-bit step) - see dither_test.cpp.
add_executable(dither_test dither_test.cpp dither.cpp gl_loader.cpp)
target_link_libraries(dither_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyVignette()'s GPU pipeline
# at intensity=0 (must reproduce the input exactly everywhere, corners included) and at a strong
# intensity (center stays bit-exact inside `radius`, near-corner pixel comes back much darker) -
# see vignette_test.cpp.
add_executable(vignette_test vignette_test.cpp vignette.cpp gl_loader.cpp)
target_link_libraries(vignette_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyChromaticAberration()'s
# GPU pipeline at strength=0 (exact passthrough) and strength=1 (flat frame must survive the
# radial channel offsets unchanged - catches channel swaps and out-of-bounds sampling) - see
# chromatic_aberration_test.cpp.
add_executable(chromatic_aberration_test chromatic_aberration_test.cpp chromatic_aberration.cpp gl_loader.cpp)
target_link_libraries(chromatic_aberration_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplySmaa()'s three-pass GPU
# pipeline (edge detection -> blending weight calculation -> neighborhood blending) runs with
# no GL error and reproduces a flat/edgeless image exactly - see smaa_test.cpp.
add_executable(smaa_test smaa_test.cpp smaa.cpp gl_loader.cpp)
target_link_libraries(smaa_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyNr()'s filter+combine GPU
# pipeline reproduces a flat/edgeless image exactly regardless of intensity/passes (a bilateral
# filter of a uniform field is a no-op by construction), and that a checkerboard pattern is
# measurably smoothed at a nonzero intensity - see nr_test.cpp.
add_executable(nr_test nr_test.cpp nr.cpp gl_loader.cpp)
target_link_libraries(nr_test opengl32 gdi32 user32)

# Creates a real GL context, clears to a known color, and checks ApplyLocalContrast()'s 5-pass
# GPU pipeline reproduces a flat/edgeless image exactly (both detail layers are zero on a
# uniform field) regardless of strength, and that a hard edge gains measurable local contrast
# at a nonzero strength - see local_contrast_test.cpp.
add_executable(local_contrast_test local_contrast_test.cpp local_contrast.cpp gl_loader.cpp)
target_link_libraries(local_contrast_test opengl32 gdi32 user32)

# Creates a real GL context, fills a texture with a known flat color, and checks
# DrawFxIndicator() draws its badge only within its own corner footprint (a point far from it
# stays exactly the fill color; a known "on" bit of the "F" glyph comes back white; the plate
# darkens a non-glyph point inside its footprint) and leaves a too-small texture untouched -
# see fx_indicator_test.cpp.
add_executable(fx_indicator_test fx_indicator_test.cpp fx_indicator.cpp gl_loader.cpp)
target_link_libraries(fx_indicator_test opengl32 gdi32 user32)

# Creates a real GL context, a real texture, and checks ApplyTextureFilterOverride[F]() only
# upgrades GL_TEXTURE_MIN_FILTER calls that request a mipmap-capable mode on GL_TEXTURE_2D
# (both the int and float entry points), forces trilinear + the configured anisotropy on those,
# leaves every other glTexParameter call byte-for-byte unchanged (wrong pname, non-mipmap
# filter, wrong target), and is a true no-op end to end when anisotropy=0 - see
# texture_filter_test.cpp.
add_executable(texture_filter_test texture_filter_test.cpp texture_filter.cpp config.cpp gl_loader.cpp)
target_link_libraries(texture_filter_test opengl32 gdi32 user32)
# texture_filter_test resolves opengl32_enhancer.ini next to its own .exe, same as
# post_effects_test - but this needs a fixed, known anisotropy value rather than whatever the
# project's real shipped ini currently says, so it copies its own small fixture instead (see
# texture_filter_test.ini) renamed to the filename config.cpp actually looks for. That file
# gets its own RUNTIME_OUTPUT_DIRECTORY, separate from every other target's shared build
# directory: two targets both writing a same-named opengl32_enhancer.ini into one directory is
# a real Ninja hazard (POST_BUILD steps between unrelated targets aren't ordered), not a
# theoretical one - post_effects_test's own copy of the REAL shipped ini silently won that race
# in a release build during development here, and this test read the wrong config as a result.
set_target_properties(texture_filter_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/texture_filter_test_dir
)
add_custom_command(TARGET texture_filter_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/texture_filter_test.ini
        $<TARGET_FILE_DIR:texture_filter_test>/opengl32_enhancer.ini
)

# Integration test for the CURRENT opengl32_enhancer.ini: builds a synthetic test image, blits it
# onto a real back buffer, calls the actual ApplySelectedEffect() entry point (the same one
# wrapper.cpp calls from wglSwapBuffers), and checks it runs with no GL error and visibly
# changes the image - then dumps input/output .ppm files for a human to judge whether today's
# config actually looks right (an aesthetic call no automated assertion can make) - see
# post_effects_test.cpp's header comment. Links the same stage sources as ${WRAPPER_NAME_CXX}
# (everything except wrapper.cpp itself, which this doesn't need).
add_executable(post_effects_test post_effects_test.cpp post_effects.cpp config.cpp gl_loader.cpp
    pixel_invert.cpp bilinear_upscale.cpp nis_effect.cpp hdr_look.cpp bloom.cpp lut_grading.cpp
    vignette.cpp chromatic_aberration.cpp taa.cpp dither.cpp smaa.cpp fsr.cpp cas.cpp nr.cpp
    local_contrast.cpp fx_indicator.cpp)
target_link_libraries(post_effects_test opengl32 gdi32 user32)
# post_effects_test resolves opengl32_enhancer.ini (and any lutPath it names) next to its own .exe
# - see config.cpp's GetIniPathNextToThisModule. Copy the real files there after each build so
# the test reads the actual shipped config, not a fixture. With the Ninja generator this project
# uses, that copy is chained after the link step (see the generated build.ninja), so it only
# re-runs when post_effects_test itself actually relinks - editing ONLY opengl32_enhancer.ini
# and re-running the already-built .exe reads a stale copy. `cmake --build ... --target
# post_effects_test` after touching any of its own source files (or a full reconfigure) is what
# actually refreshes it.
add_custom_command(TARGET post_effects_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/opengl32_enhancer.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/cyberpunk.cube
        $<TARGET_FILE_DIR:post_effects_test>
)

# Register every test target above with CTest, so `ctest` in the build directory is the one
# authoritative way to run them. Without this, ctest reports "No tests were found!!!" and the
# only way to run the suite is to glob *_test.exe in the build directory - which also picks up
# stale executables left behind by targets that have since been removed, making deleted tests
# look like failing ones.
#
# Tests are split by what they need to run, via the "gpu" CTest label:
#
#   - Unlabeled tests need nothing but Windows. They run anywhere, CI included.
#   - Tests labeled "gpu" create a real GL context and dispatch compute shaders, so they need a
#     GPU/driver exposing GL 4.3 compute. They still create their own windows and run
#     unattended, but on a machine without that support (a GitHub Actions runner falls back to
#     Microsoft's GDI Generic OpenGL 1.1) they fail rather than skip - GetGlComputeApi() cannot
#     resolve the entry points, which is exactly the failure they exist to detect on real
#     hardware. Select them with `ctest -L gpu`, or exclude them with `ctest -LE gpu`.
enable_testing()

# No GPU required: config_test is pure INI parsing, and ${WRAPPER_NAME_CXX}_test only loads the
# proxy DLL and calls through it (its GL calls are made with no current context on purpose).
foreach(test_target
    ${WRAPPER_NAME_CXX}_test
    config_test)
    add_test(NAME ${test_target} COMMAND ${test_target}
             WORKING_DIRECTORY $<TARGET_FILE_DIR:${test_target}>)
endforeach()

foreach(test_target
    gl_loader_test
    gl_context_change_test
    cas_test
    fsr_test
    bilinear_upscale_test
    nis_effect_test
    hdr_look_test
    taa_test
    lut_grading_test
    bloom_test
    dither_test
    vignette_test
    chromatic_aberration_test
    smaa_test
    nr_test
    local_contrast_test
    fx_indicator_test
    texture_filter_test
    post_effects_test)
    add_test(NAME ${test_target} COMMAND ${test_target}
             WORKING_DIRECTORY $<TARGET_FILE_DIR:${test_target}>)
    set_tests_properties(${test_target} PROPERTIES LABELS gpu)
endforeach()
