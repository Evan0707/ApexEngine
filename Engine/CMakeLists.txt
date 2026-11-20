find_package(EnTT CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(APEX_CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
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
    Graphics/ColorTarget.cpp
    Graphics/ColorTarget.h
    Graphics/DeferredDeleter.cpp
    Graphics/DeferredDeleter.h
    Graphics/DepthTarget.cpp
    Graphics/DepthTarget.h
    Graphics/DrawList.cpp
    Graphics/DrawList.h
    Graphics/Device.cpp
    Graphics/Device.h
    Graphics/EnvironmentLighting.cpp
    Graphics/EnvironmentLighting.h
    Graphics/EnvironmentPrefilter.cpp
    Graphics/EnvironmentPrefilter.h
    Graphics/FrameBuffers.cpp
    Graphics/FrameBuffers.h
    Graphics/FrameCounters.h
    Graphics/FrameData.cpp
    Graphics/FrameData.h
    Graphics/FrameSync.cpp
    Graphics/FrameSync.h
    Graphics/Frustum.cpp
    Graphics/Frustum.h
    Graphics/GpuAllocator.cpp
    Graphics/GpuAllocator.h
    Graphics/GpuMemoryStats.h
    Graphics/GpuMesh.cpp
    Graphics/GpuEnvironment.cpp
    Graphics/GpuEnvironment.h
    Graphics/GpuMesh.h
    Graphics/GpuTimer.cpp
    Graphics/GpuTimer.h
    Graphics/GpuTimestamps.cpp
    Graphics/GpuTimestamps.h
    Graphics/GraphicsPipeline.cpp
    Graphics/GraphicsPipeline.h
    Graphics/GpuTexture.cpp
    Graphics/GpuTexture.h
    Graphics/ImageInputSet.cpp
    Graphics/ImageInputSet.h
    Graphics/LightingSets.cpp
    Graphics/LightingSets.h
    Graphics/MaterialSets.cpp
    Graphics/MaterialSets.h
    Graphics/MaterialTable.cpp
    Graphics/MaterialTable.h
    Graphics/MipLevels.h
    Graphics/Projection.cpp
    Graphics/Projection.h
    Graphics/RenderOverlay.h
    Graphics/RenderScene.h
    Graphics/Renderer.cpp
    Graphics/RendererRecording.cpp
    Graphics/RendererResources.cpp
    Graphics/RendererState.h
    Graphics/RenderTargetDesc.h
    Graphics/Renderer.h
    Graphics/ResourceMirror.h
    Graphics/ShaderBinaries.cpp
    Graphics/ShaderBinaries.h
    Graphics/ShadowMap.cpp
    Graphics/ShadowMap.h
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
    Physics/JoltInclude.h
    Physics/PhysicsBodies.cpp
    Physics/PhysicsCharacters.cpp
    Physics/PhysicsQueries.cpp
    Physics/PhysicsSettings.h
    Physics/PhysicsTypes.h
    Physics/PhysicsWorld.cpp
    Physics/PhysicsWorld.h
    Physics/PhysicsWorldState.h
    Physics/Runtime.cpp
    Physics/Runtime.h
    Platform/Event.h
    Platform/Key.cpp
    Platform/Key.h
    Platform/Paths.cpp
    Platform/Paths.h
    Platform/Window.cpp
    Platform/Window.h
    Resources/AssetPath.cpp
    Resources/AssetPath.h
    Resources/EnvironmentData.cpp
    Resources/EnvironmentData.h
    Resources/FallbackAssets.cpp
    Resources/FallbackAssets.h
    Resources/FileReader.cpp
    Resources/FileReader.h
    Resources/GltfImporter.cpp
    Resources/GltfImporter.h
    Resources/Handles.h
    Resources/ImageDecoder.cpp
    Resources/ImageDecoder.h
    Resources/MaterialData.h
    Resources/MeshData.cpp
    Resources/MeshData.h
    Resources/ModelData.h
    Resources/Primitives.cpp
    Resources/Primitives.h
    Resources/ResourceCache.cpp
    Resources/ResourceCache.h
    Resources/ResourcePool.h
    Resources/TextureData.h
    Serialization/ConfigJson.cpp
    Serialization/EngineConfig.h
    Serialization/InputConfig.h
    Serialization/Json.cpp
    Serialization/Json.h
    Serialization/SceneDescription.h
    Serialization/SceneJson.cpp
    Serialization/SceneJson.h
    Serialization/SceneLoader.cpp
    Serialization/SceneLoader.h
    World/ColliderLines.cpp
    World/ColliderLines.h
    World/Components.h
    World/PhysicsSync.cpp
    World/PhysicsSync.h
    World/RenderExtraction.cpp
    World/RenderExtraction.h
    World/Rotation.cpp
    World/Rotation.h
    World/SceneQueries.cpp
    World/SceneQueries.h
    World/ShadowFocus.cpp
    World/ShadowFocus.h
)
add_library(Apex::Engine ALIAS ApexEngine)

apex_add_shaders(ApexEngine
    Graphics/Shaders/Fullscreen.vert
    Graphics/Shaders/Prefilter.frag
    Graphics/Shaders/Prefilter.vert
    Graphics/Shaders/Scene.frag
    Graphics/Shaders/Scene.vert
    Graphics/Shaders/Shadow.vert
    Graphics/Shaders/Sky.frag
    Graphics/Shaders/Sky.vert
    Graphics/Shaders/Tonemap.frag
)
# Included by the shaders above: tracked through glslc's depfiles, listed here for the IDE.
target_sources(ApexEngine PRIVATE
    Graphics/Shaders/Environment.glsl
    Graphics/Shaders/Frame.glsl
    Graphics/Shaders/Material.glsl
    Graphics/Shaders/Pbr.glsl
    Graphics/Shaders/Shadow.glsl
)

# Include root: #include "Core/Version.h"
target_include_directories(ApexEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ApexEngine PUBLIC Apex::Options)
# PUBLIC: GLM types are part of the public interfaces; included through Core/Math.h only (decision 0010).
target_link_libraries(ApexEngine PUBLIC glm::glm)
# PUBLIC: World and Game use entt::registry directly (decision 0015). Never included by Graphics.
target_link_libraries(ApexEngine PUBLIC EnTT::EnTT)
# SDL is included only by Platform/*.cpp (decision 0002).
target_link_libraries(ApexEngine PRIVATE SDL3::SDL3)
# Vulkan headers are included only by Graphics (decision 0006); no public header of Graphics needs them.
target_link_libraries(ApexEngine PRIVATE Vulkan::Headers Vulkan::Loader vk-bootstrap::vk-bootstrap
                                          GPUOpen::VulkanMemoryAllocator)
# stb_image is included only by Resources/ImageDecoder.cpp (decision 0013). SYSTEM: its warnings are not ours.
target_include_directories(ApexEngine SYSTEM PRIVATE "${Stb_INCLUDE_DIR}")
# cgltf is included only by Resources/GltfImporter.cpp (decision 0012).
target_include_directories(ApexEngine SYSTEM PRIVATE "${APEX_CGLTF_INCLUDE_DIR}")
# nlohmann/json is included only by Serialization/Json.h, an internal header (decision 0016).
target_link_libraries(ApexEngine PRIVATE nlohmann_json::nlohmann_json)
# Jolt is included only by Physics, through its internal header Physics/JoltInclude.h (decision 0018). Its
# compile definitions must come from this target: Jolt checks at run time that both sides agree.
target_link_libraries(ApexEngine PRIVATE Jolt::Jolt)
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
