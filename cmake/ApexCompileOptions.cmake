# Shared compile options for engine, game and tests. Third-party code never links this target.

add_library(apex_options INTERFACE)
add_library(Apex::Options ALIAS apex_options)

if(MSVC)
    # /std:c++23 requires MSVC Build Tools 14.52 (compiler 19.52).
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.52)
        message(FATAL_ERROR
            "MSVC ${CMAKE_CXX_COMPILER_VERSION} detected. Apex Engine requires MSVC Build Tools 14.52 "
            "(compiler 19.52) or newer: update Visual Studio 2026 to 18.12 or later.")
    endif()

    target_compile_options(apex_options INTERFACE
        /std:c++23
        /W4
        /permissive-
        /utf-8
        /Zc:__cplusplus
    )
    if(APEX_WARNINGS_AS_ERRORS)
        target_compile_options(apex_options INTERFACE /WX)
    endif()
else()
    # Not a supported target; kept so a port stays possible (G-10).
    target_compile_features(apex_options INTERFACE cxx_std_23)
    target_compile_options(apex_options INTERFACE -Wall -Wextra -Wpedantic)
    if(APEX_WARNINGS_AS_ERRORS)
        target_compile_options(apex_options INTERFACE -Werror)
    endif()
endif()
