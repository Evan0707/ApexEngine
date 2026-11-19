cmake_minimum_required(VERSION 3.28)

project(ApexEngine VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset: cmake --preset windows-debug")
endif()

option(APEX_BUILD_TESTS "Build the automated tests" ON)
option(APEX_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
# Needs the vcpkg manifest feature "profiling" (VCPKG_MANIFEST_FEATURES), as set by the windows-profile preset.
option(APEX_ENABLE_PROFILING "Instrument the engine with Tracy" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/ApexCompileOptions.cmake)
include(cmake/ApexShaders.cmake)

add_subdirectory(Engine)
add_subdirectory(Game)

if(APEX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
