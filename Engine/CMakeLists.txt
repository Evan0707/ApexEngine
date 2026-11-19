find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
if(APEX_ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
endif()

add_library(ApexEngine STATIC
    Core/Assert.cpp
    Core/Assert.h
    Core/ConsoleLogSink.cpp
    Core/ConsoleLogSink.h
    Core/Error.h
    Core/FileLogSink.cpp
    Core/FileLogSink.h
    Core/FixedTimestep.cpp
    Core/FixedTimestep.h
    Core/FramePhases.h
    Core/Logger.cpp
    Core/Logger.h
    Core/LogSink.cpp
    Core/LogSink.h
    Core/MainLoop.cpp
    Core/MainLoop.h
    Core/Profiling.h
    Core/Version.cpp
    Core/Version.h
    Debug/FrameTimeStats.cpp
    Debug/FrameTimeStats.h
    Debug/LogHistory.cpp
    Debug/LogHistory.h
    Debug/LogHistorySink.cpp
    Debug/LogHistorySink.h
    Graphics/DeferredDeleter.cpp
    Graphics/DeferredDeleter.h
    Graphics/Device.cpp
    Graphics/Device.h
    Graphics/FrameSync.cpp
    Graphics/FrameSync.h
    Graphics/Renderer.cpp
    Graphics/Renderer.h
    Graphics/ShaderBinaries.cpp
    Graphics/ShaderBinaries.h
    Graphics/Swapchain.cpp
    Graphics/Swapchain.h
    Graphics/SwapchainState.cpp
    Graphics/SwapchainState.h
    Graphics/ValidationLog.cpp
    Graphics/ValidationLog.h
    Input/ActionMap.cpp
    Input/ActionMap.h
    Input/ActionState.cpp
    Input/ActionState.h
    Platform/Event.h
    Platform/Key.cpp
    Platform/Key.h
    Platform/Window.cpp
    Platform/Window.h
)
add_library(Apex::Engine ALIAS ApexEngine)

apex_add_shaders(ApexEngine
    Graphics/Shaders/Triangle.frag
    Graphics/Shaders/Triangle.vert
)

# Include root: #include "Core/Version.h"
target_include_directories(ApexEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ApexEngine PUBLIC Apex::Options)
# SDL is included only by Platform/*.cpp (decision 0002).
target_link_libraries(ApexEngine PRIVATE SDL3::SDL3)
# Vulkan headers are included only by Graphics (decision 0006); no public header of Graphics needs them.
target_link_libraries(ApexEngine PRIVATE Vulkan::Headers Vulkan::Loader vk-bootstrap::vk-bootstrap)
if(APEX_ENABLE_PROFILING)
    # PUBLIC: the macros of Core/Profiling.h expand in every source that profiles (decision 0003).
    target_link_libraries(ApexEngine PUBLIC Tracy::TracyClient)
    target_compile_definitions(ApexEngine PUBLIC APEX_ENABLE_PROFILING=1)
endif()

target_compile_definitions(ApexEngine PRIVATE
    APEX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    APEX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    APEX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
