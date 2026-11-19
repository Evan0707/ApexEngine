#pragma once

#include "Graphics/DeferredDeleter.h"
#include "Graphics/DepthTarget.h"
#include "Graphics/Device.h"
#include "Graphics/DrawList.h"
#include "Graphics/FrameBuffers.h"
#include "Graphics/FrameData.h"
#include "Graphics/FrameSync.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/GpuMesh.h"
#include "Graphics/GpuTexture.h"
#include "Graphics/MaterialSets.h"
#include "Graphics/MaterialTable.h"
#include "Graphics/Renderer.h"
#include "Graphics/ResourceMirror.h"
#include "Graphics/ScenePipeline.h"
#include "Graphics/Swapchain.h"
#include "Graphics/SwapchainState.h"
#include "Graphics/UploadContext.h"
#include "Resources/ResourceCache.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace apex::graphics {

// Internal to Graphics: the private state of the Renderer, shared by its three source files. Renderer.cpp
// holds the device, the swapchain and the frame loop; RendererResources.cpp the GPU mirror of the resource
// cache; RendererRecording.cpp the draw list and the command recording.

// Spec 5.4.
inline constexpr std::uint32_t kFramesInFlight = 2;

// Room in the per-frame material table, default material included.
inline constexpr std::uint32_t kMaxMaterials = 256;
// Material descriptor sets alive at once: every material twice (a set waiting for deferred destruction
// and its replacement), plus the default material's.
inline constexpr std::uint32_t kMaxMaterialSets = 2 * kMaxMaterials + 2;
// Room in the per-frame object buffer; objects beyond are not drawn, with a warning.
inline constexpr std::uint32_t kMaxObjects = 4096;

// Logs "<what> failed: VkResult n" and returns it as a GpuFailure.
[[nodiscard]] std::unexpected<core::Error> vulkanError(core::Logger& logger, std::string_view what,
                                                       VkResult result);

// Members are destroyed in reverse order: swapchain and frames, then the deleter (which may still free
// buffers), then the allocator, then the device.
struct Renderer::State {
    platform::Window* window = nullptr;
    core::Logger* logger = nullptr;
    std::unique_ptr<Device> device;
    std::unique_ptr<GpuAllocator> allocator;
    // Before the deleter: pending destructions free sets of its pool.
    MaterialSets materialSets;
    DeferredDeleter deleter{kFramesInFlight};
    FrameSync frames;
    UploadContext upload;
    ResourceMirror<resources::MeshHandle, resources::MeshData, GpuMesh> meshes;
    resources::MeshHandle fallbackMesh;
    ResourceMirror<resources::TextureHandle, resources::TextureData, GpuTexture> textures;
    // Material sets refer to texture views: all rebuilt when the textures change.
    ResourceMirror<resources::MaterialHandle, resources::MaterialData, VkDescriptorSet> materialSetMirror;
    std::optional<VkDescriptorSet> defaultMaterialSet;
    std::uint64_t textureRevision = std::numeric_limits<std::uint64_t>::max();
    MaterialTable materials{kMaxMaterials};
    FrameBuffers frameBuffers;
    ScenePipeline scenePipeline;
    Swapchain swapchain;
    DepthTarget depth;
    FrameCounters counters;
    RenderOverlay* overlay = nullptr;
    bool vsync = true;
    SwapchainState swapchainState;
    std::uint32_t frameIndex = 0;
    // True once the fence of a slot was waited for and the deleter advanced, until the slot is submitted
    // again.
    std::array<bool, kFramesInFlight> slotWaited{};
    // Set by renderFrame, consumed by presentFrame.
    std::optional<std::uint32_t> acquiredImage;
    // Rebuilt each frame; kept to reuse their memory.
    std::vector<DrawItem> drawItems;
    // The mesh of each draw, in draw order. Valid for the frame being recorded only.
    std::vector<const GpuMesh*> drawMeshes;
    std::vector<VkDescriptorSet> drawMaterialSets;
    std::vector<ObjectGpu> objectData;
    bool warnedTooManyObjects = false;

    ~State() {
        if (device) {
            device->waitIdle();
        }
        if (allocator) {
            meshes.releaseAll([this](GpuMesh&& mesh) { destroyGpuMesh(*allocator, mesh); });
            textures.releaseAll(
                [this](GpuTexture&& texture) { destroyGpuTexture(device->device(), *allocator, texture); });
        }
    }

    // False when the surface has no area yet: the frame is skipped and the recreation tried again.
    [[nodiscard]] core::Result<bool> recreateSwapchain(platform::PixelSize size);
    [[nodiscard]] core::Result<void> ensurePipeline();
    // Resolves the handles, sorts by material and fills the frame's buffers.
    void prepareFrame(const RenderScene& scene);
    // The view of a texture: white for "no texture", the checker when not ready or failed (G-03).
    [[nodiscard]] VkImageView resolveTexture(resources::TextureHandle texture,
                                             resources::TextureHandle checker,
                                             resources::TextureHandle white) const noexcept;
    void syncMaterialSets(const resources::ResourceCache& cache);
    // The object's mesh, or the fallback mesh when it is not ready (G-03). nullptr before the first sync.
    [[nodiscard]] std::pair<const GpuMesh*, std::uint32_t>
    resolveMesh(const RenderObject& object) const noexcept;
    // After prepareFrame: draws what it resolved.
    void recordFrame(VkCommandBuffer commands, std::uint32_t imageIndex);
    void recordScenePass(VkCommandBuffer commands, std::uint32_t imageIndex);
    void recordOverlayPass(VkCommandBuffer commands, std::uint32_t imageIndex);
};

} // namespace apex::graphics
