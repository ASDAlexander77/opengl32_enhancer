cmake_minimum_required(VERSION 3.20)

# Make CMake find ts-language modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

# Enable TS-language
enable_language(TSLANG)

# Include folders
include_directories(${CMAKE_TSLANG_DIR}/defaultlib)

# The compiled default lib is split into per-build subfolders (debug/release) and then per
# memory model (gc/rc/none); pick the pair matching this build, so that the CRT, the allocator
# and the default-lib binaries all agree. A library built for one model cannot be linked into
# a program built for another.
if (CMAKE_BUILD_TYPE STREQUAL "Release")
	set(TSLANG_DEFAULTLIB_BUILD "release")
else()
	set(TSLANG_DEFAULTLIB_BUILD "debug")
endif()

set(TSLANG_MEMORY_MODEL "gc" CACHE STRING "Memory model of compiled code: gc, rc or none")
set_property(CACHE TSLANG_MEMORY_MODEL PROPERTY STRINGS gc rc none)

# Lib folders
link_directories(${CMAKE_TSLANG_DIR} ${CMAKE_TSLANG_DIR}/defaultlib/lib/${TSLANG_DEFAULTLIB_BUILD}/${TSLANG_MEMORY_MODEL})

# set options
if (CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_TSLANG_FLAGS "--opt --opt_level=3") # global
else()
	set(CMAKE_TSLANG_FLAGS "--di --opt_level=0") # global
endif()

# The same variable that picked the link directory has to reach the compiler as well, or the
# program is compiled for one model and linked against another model's default lib.
set(CMAKE_TSLANG_FLAGS "${CMAKE_TSLANG_FLAGS} -mm=${TSLANG_MEMORY_MODEL}") # global

if(WIN32)
else()
    set(CMAKE_TSLANG_FLAGS "${CMAKE_TSLANG_FLAGS} -relocation-model=pic") # global
endif()

set (WRAPPER_NAME "opengl32_enh")

# .ts files compile with TSLANG command; .cpp with the C++ compiler.
add_executable(${PROJECT_NAME}
    dummy.cpp
    main.ts
)

add_library(${WRAPPER_NAME} SHARED
    dummy.cpp
    wrapper.ts
)

# Exercises the opengl32 proxy built from wrapper.ts: resolves every intercepted
# function by name and calls a representative subset through it, to check the
# interception is actually catching the calls rather than just compiling. Needs
# the "opengl32" target's DLL to sit next to it at runtime (same output directory,
# which is the default for a single-config generator like Ninja).
add_executable("test"
    dummy.cpp
    test.ts
)

# required libs
set(TSLANG_LINK_LIBS "TypeScriptDefaultLib" "TypeScriptAsyncRuntime" "LLVMSupport")

# Boehm is only referenced by the gc default lib; the rc and none builds allocate through the
# CRT and must not drag a collector in.
if (TSLANG_MEMORY_MODEL STREQUAL "gc")
    list(APPEND TSLANG_LINK_LIBS "gcdll/gc")
endif()

# ntdll provides RtlGetLastNtStatus (pulled in by LLVMSupport) on Windows
if(WIN32)
    list(APPEND TSLANG_LINK_LIBS "ntdll")
else()
    list(APPEND TSLANG_LINK_LIBS "LLVMDemangle" "stdc++" "m" "pthread" "tinfo" "dl" "rt")
endif()

target_link_libraries(${PROJECT_NAME} ${TSLANG_LINK_LIBS})
target_link_libraries(${WRAPPER_NAME} ${TSLANG_LINK_LIBS})
target_link_libraries("test" ${TSLANG_LINK_LIBS})

# The GC-model default lib depends on gc.dll at runtime; copy it next to the
# built binary so the program can find it without relying on PATH.
if (TSLANG_MEMORY_MODEL STREQUAL "gc")
    add_custom_command(TARGET ${WRAPPER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_TSLANG_DIR}/gcdll/gc.dll"
            "$<TARGET_FILE_DIR:${WRAPPER_NAME}>"
    )
endif()

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
add_library(${WRAPPER_NAME_CXX} SHARED wrapper.cpp pixel_invert.cpp)
set_target_properties(${WRAPPER_NAME_CXX} PROPERTIES
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/wrapper.def"
)

# Loads opengl32_enh32.dll by name (same output directory) and calls a couple of
# functions through it, the same sanity check test.ts does for the x64 build.
add_executable(${WRAPPER_NAME_CXX}_test wrapper_test.cpp)
add_dependencies(${WRAPPER_NAME_CXX}_test ${WRAPPER_NAME_CXX})
