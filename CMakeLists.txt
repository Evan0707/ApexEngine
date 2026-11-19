cmake_minimum_required(VERSION 3.28)

project(ApexEngine VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset: cmake --preset windows-debug")
endif()

option(APEX_BUILD_TESTS "Build the automated tests" ON)
option(APEX_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
# Needs the vcpkg manifest feature "profiling" (VCPKG_MANIFEST_FEATURES), as set by the windows-profile preset.
option(APEX_ENABLE_PROFILING "Instrument the engine with Tracy" OFF)

# C++23 for every target of the project. CMake still maps it to /std:c++latest for MSVC, while the toolset
# required by cmake/ApexCompileOptions.cmake has /std:c++23: tell CMake so. Declared here rather than as
# a raw flag: dependencies that ask for C++17 or C++20 (vk-bootstrap, EnTT) then add no second /std flag.
if(MSVC)
    set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "-std:c++23")
    set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "-std:c++23")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The project has no C++ module: no dependency scan before each compilation.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
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
