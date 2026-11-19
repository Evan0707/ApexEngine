find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
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
    Core/Math.h
    Core/Profiling.h
    Core/Version.cpp
    Core/Version.h
    Core/WorldPosition.h
    Debug/BenchmarkRecorder.cpp
    Debug/BenchmarkRecorder.h
    Debug/ColorSpace.cpp
    Debug/ColorSpace.h
    Debug/DebugUi.cpp
    Debug/DebugUi.h
    Debug/FrameTimeStats.cpp
    Debug/FrameTimeStats.h
    Debug/ImGuiBackend.cpp
    Debug/ImGuiBackend.h
    Debug/InputCapture.cpp
    Debug/InputCapture.h
    Debug/LogHistory.cpp
    Debug/LogHistory.h
    Debug/LogHistorySink.cpp
    Debug/LogHistorySink.h
    Debug/StatsPanel.cpp
    Debug/StatsPanel.h
    Graphics/DeferredDeleter.cpp
    Graphics/DeferredDeleter.h
    Graphics/DepthTarget.cpp
    Graphics/DepthTarget.h
    Graphics/DrawList.cpp
    Graphics/DrawList.h
    Graphics/Device.cpp
    Graphics/Device.h
    Graphics/FrameBuffers.cpp
    Graphics/FrameBuffers.h
    Graphics/FrameCounters.h
    Graphics/FrameData.cpp
    Graphics/FrameData.h
    Graphics/FrameSync.cpp
    Graphics/FrameSync.h
    Graphics/GpuAllocator.cpp
    Graphics/GpuAllocator.h
    Graphics/GpuHandles.h
    Graphics/GpuMemoryStats.h
    Graphics/GpuMesh.cpp
    Graphics/GpuMesh.h
    Graphics/HandlePool.h
    Graphics/MaterialTable.cpp
    Graphics/MaterialTable.h
    Graphics/Projection.cpp
    Graphics/Projection.h
    Graphics/RenderOverlay.h
    Graphics/RenderScene.h
    Graphics/Renderer.cpp
    Graphics/RenderTargetDesc.h
    Graphics/Renderer.h
    Graphics/ScenePipeline.cpp
    Graphics/ScenePipeline.h
    Graphics/ShaderBinaries.cpp
    Graphics/ShaderBinaries.h
    Graphics/Swapchain.cpp
    Graphics/Swapchain.h
    Graphics/SwapchainState.cpp
    Graphics/SwapchainState.h
    Graphics/UploadContext.cpp
    Graphics/UploadContext.h
    Graphics/ValidationLog.cpp
    Graphics/ValidationLog.h
    Graphics/VmaImplementation.cpp
    Graphics/VulkanBackendAccess.h
    Input/ActionMap.cpp
    Input/ActionMap.h
    Input/ActionState.cpp
    Input/ActionState.h
    Platform/Event.h
    Platform/Key.cpp
    Platform/Key.h
    Platform/Window.cpp
    Platform/Window.h
    Resources/MaterialData.h
    Resources/MeshData.cpp
    Resources/MeshData.h
    Resources/Primitives.cpp
    Resources/Primitives.h
)
add_library(Apex::Engine ALIAS ApexEngine)

apex_add_shaders(ApexEngine
    Graphics/Shaders/Scene.frag
    Graphics/Shaders/Scene.vert
)
# Included by the shaders above: tracked through glslc's depfiles, listed here for the IDE.
target_sources(ApexEngine PRIVATE
    Graphics/Shaders/Frame.glsl
    Graphics/Shaders/Material.glsl
    Graphics/Shaders/Pbr.glsl
)

# Include root: #include "Core/Version.h"
target_include_directories(ApexEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ApexEngine PUBLIC Apex::Options)
# PUBLIC: GLM types are part of the public interfaces; included through Core/Math.h only (decision 0010).
target_link_libraries(ApexEngine PUBLIC glm::glm)
# SDL is included only by Platform/*.cpp (decision 0002).
target_link_libraries(ApexEngine PRIVATE SDL3::SDL3)
# Vulkan headers are included only by Graphics (decision 0006); no public header of Graphics needs them.
target_link_libraries(ApexEngine PRIVATE Vulkan::Headers Vulkan::Loader vk-bootstrap::vk-bootstrap
                                          GPUOpen::VulkanMemoryAllocator)
# Dear ImGui is included only by Debug (decision 0008).
target_link_libraries(ApexEngine PRIVATE imgui::imgui)
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
