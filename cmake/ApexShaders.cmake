# GLSL to SPIR-V at build time, with glslc from the Vulkan SDK (spec 3.1, decision 0006).
# A shader error fails the build. The SPIR-V is embedded in the binary: nothing to find at launch.

find_program(APEX_GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" NO_CACHE)
if(NOT APEX_GLSLC)
    message(FATAL_ERROR
        "glslc not found. Install the LunarG Vulkan SDK 1.4.363.1 (https://vulkan.lunarg.com/sdk/home), "
        "then open a new terminal so that VULKAN_SDK is set.")
endif()
message(STATUS "glslc: ${APEX_GLSLC}")

# apex_add_shaders(<target> <shader>...)
# Compiles each shader to "<build dir>/Shaders/<name>.spv.inc", a C initializer list of 32-bit words:
#   constexpr std::uint32_t kWords[] =
#   #include "Shaders/Triangle.vert.spv.inc"
#   ;
function(apex_add_shaders target)
    set(outputDirectory "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
    file(MAKE_DIRECTORY "${outputDirectory}")
    set(outputs "")
    foreach(shader IN LISTS ARGN)
        cmake_path(ABSOLUTE_PATH shader BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE source)
        cmake_path(GET source FILENAME name)
        set(output "${outputDirectory}/${name}.spv.inc")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${APEX_GLSLC}" --target-env=vulkan1.3 -Werror -mfmt=c
                    "$<IF:$<CONFIG:Debug>,-g,-O>"
                    -MD -MF "${output}.d" -o "${output}" "${source}"
            MAIN_DEPENDENCY "${source}"
            DEPFILE "${output}.d"
            COMMENT "Compiling shader ${name}"
            VERBATIM)
        list(APPEND outputs "${output}")
    endforeach()
    target_sources(${target} PRIVATE ${outputs})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()
