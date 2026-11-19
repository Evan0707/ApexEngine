#include "Graphics/Renderer.h"

#include "Core/Profiling.h"
#include "Graphics/DeferredDeleter.h"
#include "Graphics/DepthTarget.h"
#include "Graphics/Device.h"
#include "Graphics/DrawList.h"
#include "Graphics/FrameBuffers.h"
#include "Graphics/FrameData.h"
#include "Graphics/FrameSync.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/GpuMesh.h"
#include "Graphics/MaterialTable.h"
#include "Graphics/ResourceMirror.h"
#include "Graphics/ScenePipeline.h"
#include "Graphics/Swapchain.h"
#include "Graphics/SwapchainState.h"
#include "Graphics/UploadContext.h"
#include "Graphics/VulkanBackendAccess.h"
#include "Resources/ResourceCache.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace apex::graphics {

namespace {

// Spec 5.4.
constexpr std::uint32_t kFramesInFlight = 2;

// Room in the per-frame material table, default material included.
constexpr std::uint32_t kMaxMaterials = 256;
// Room in the per-frame object buffer; objects beyond are not drawn, with a warning.
constexpr std::uint32_t kMaxObjects = 4096;

// Linear color, written to an sRGB swapchain: a dark blue, easy to tell from a black window.
constexpr VkClearColorValue kClearColor{{0.02f, 0.04f, 0.10f, 1.0f}};

// 32-bit float depth (spec 5.4). Chosen here only, then passed to the pipelines (G-09).
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

[[nodiscard]] std::unexpected<core::Error> vulkanError(core::Logger& logger, std::string_view what,
                                                       VkResult result) {
    std::string message = std::format("{} failed: VkResult {}", what, static_cast<int>(result));
    logger.error("{}", message);
    return core::makeError(core::ErrorCode::GpuFailure, std::move(message));
}

[[nodiscard]] std::string_view presentModeName(bool vsync) noexcept {
    return vsync ? "vsync on" : "vsync off";
}

[[nodiscard]] bool isSrgb(VkFormat format) noexcept {
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB ||
           format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}

struct ImageBarrier {
    VkImage image;
    VkImageAspectFlags aspect;
    VkImageLayout from;
    VkImageLayout to;
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;
    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;
};

void transitionImage(VkCommandBuffer commands, const ImageBarrier& transition) {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = transition.srcStage,
        .srcAccessMask = transition.srcAccess,
        .dstStageMask = transition.dstStage,
        .dstAccessMask = transition.dstAccess,
        .oldLayout = transition.from,
        .newLayout = transition.to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = transition.image,
        .subresourceRange = {transition.aspect, 0, 1, 0, 1},
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(commands, &dependency);
}

} // namespace

// Members are destroyed in reverse order: swapchain and frames, then the deleter (which may still free
// buffers), then the allocator, then the device.
struct Renderer::State {
    platform::Window* window = nullptr;
    core::Logger* logger = nullptr;
    std::unique_ptr<Device> device;
    std::unique_ptr<GpuAllocator> allocator;
    DeferredDeleter deleter{kFramesInFlight};
    FrameSync frames;
    UploadContext upload;
    ResourceMirror<resources::MeshHandle, resources::MeshData, GpuMesh> meshes;
    resources::MeshHandle fallbackMesh;
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
    std::vector<ObjectGpu> objectData;
    bool warnedTooManyObjects = false;

    ~State() {
        if (device) {
            device->waitIdle();
        }
        if (allocator) {
            meshes.releaseAll([this](GpuMesh&& mesh) { destroyGpuMesh(*allocator, mesh); });
        }
    }

    // False when the surface has no area yet: the frame is skipped and the recreation tried again.
    [[nodiscard]] core::Result<bool> recreateSwapchain(platform::PixelSize size);
    [[nodiscard]] core::Result<void> ensurePipeline();
    // Resolves the handles, sorts by material and fills the frame's buffers.
    void prepareFrame(const RenderScene& scene);
    // The object's mesh, or the fallback mesh when it is not ready (G-03). nullptr before the first sync.
    [[nodiscard]] std::pair<const GpuMesh*, std::uint32_t>
    resolveMesh(const RenderObject& object) const noexcept;
    // After prepareFrame: draws what it resolved.
    void recordFrame(VkCommandBuffer commands, std::uint32_t imageIndex);
    void recordScenePass(VkCommandBuffer commands, std::uint32_t imageIndex);
    void recordOverlayPass(VkCommandBuffer commands, std::uint32_t imageIndex);
};

core::Result<bool> Renderer::State::recreateSwapchain(platform::PixelSize size) {
    // The window can be minimized before its event is polled: it still reports its old size while the
    // surface already has none, and a swapchain cannot be created for it.
    VkSurfaceCapabilitiesKHR capabilities{};
    if (const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice(),
                                                                          device->surface(), &capabilities);
        result != VK_SUCCESS) {
        return vulkanError(*logger, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
    }
    if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0) {
        return false;
    }

    // Simple and always correct: no frame in flight can still use the old images or semaphores.
    device->waitIdle();
    core::Result<Swapchain> created =
        Swapchain::create(*device, SwapchainConfig{size.width, size.height, vsync}, swapchain.handle());
    if (!created) {
        logger->error("{}", created.error().message);
        return std::unexpected(created.error());
    }
    // The old swapchain is destroyed here, after the new one was created from it.
    swapchain = std::move(*created);
    // Same size as the swapchain. The GPU is idle: the old one can go at once.
    depth = DepthTarget();
    core::Result<DepthTarget> depthTarget =
        DepthTarget::create(*device, *allocator, swapchain.extent(), kDepthFormat);
    if (!depthTarget) {
        logger->error("{}", depthTarget.error().message);
        return std::unexpected(depthTarget.error());
    }
    depth = std::move(*depthTarget);
    swapchainState.recreated(size);
    if (core::Result<void> pipeline = ensurePipeline(); !pipeline) {
        return std::unexpected(pipeline.error());
    }

    const VkExtent2D extent = swapchain.extent();
    logger->info("Swapchain: {}x{} pixels, {} images, format {}, {}", extent.width, extent.height,
                 swapchain.imageCount(), static_cast<int>(swapchain.format()), presentModeName(vsync));
    if (!isSrgb(swapchain.format())) {
        logger->warning("The swapchain format is not sRGB: colors will look too dark");
    }
    return true;
}

core::Result<void> Renderer::State::ensurePipeline() {
    const RenderTargetDesc target{
        .colorFormat = swapchain.format(), .depthFormat = depth.format(), .samples = VK_SAMPLE_COUNT_1_BIT};
    if (scenePipeline.handle() != VK_NULL_HANDLE && scenePipeline.target() == target) {
        return {};
    }
    core::Result<ScenePipeline> created =
        ScenePipeline::create(device->device(), target, frameBuffers.layout());
    if (!created) {
        logger->error("{}", created.error().message);
        return std::unexpected(created.error());
    }
    // A frame in flight may still use the old pipeline.
    deleter.enqueue([old = std::move(scenePipeline)]() mutable { old = ScenePipeline(); });
    scenePipeline = std::move(*created);
    return {};
}

std::pair<const GpuMesh*, std::uint32_t>
Renderer::State::resolveMesh(const RenderObject& object) const noexcept {
    if (const GpuMesh* mesh = meshes.find(object.mesh)) {
        return {mesh, object.mesh.index};
    }
    return {meshes.find(fallbackMesh), fallbackMesh.index};
}

void Renderer::State::prepareFrame(const RenderScene& scene) {
    drawItems.clear();
    for (std::uint32_t i = 0; i < scene.objects.size(); ++i) {
        const RenderObject& object = scene.objects[i];
        const auto [mesh, meshIndex] = resolveMesh(object);
        if (mesh == nullptr) {
            continue;
        }
        drawItems.push_back(DrawItem{
            .objectIndex = i, .materialIndex = materials.gpuIndex(object.material), .meshIndex = meshIndex});
    }
    if (drawItems.size() > kMaxObjects) {
        if (!warnedTooManyObjects) {
            logger->warning("{} objects to draw, only the first {} are (logged once)", drawItems.size(),
                            kMaxObjects);
            warnedTooManyObjects = true;
        }
        drawItems.resize(kMaxObjects);
    }
    sortDrawItems(drawItems);

    objectData.clear();
    drawMeshes.clear();
    for (const DrawItem& item : drawItems) {
        drawMeshes.push_back(resolveMesh(scene.objects[item.objectIndex]).first);
        objectData.push_back(
            makeObjectGpu(scene.objects[item.objectIndex].modelRelativeToCamera, item.materialIndex));
    }
    const VkExtent2D extent = swapchain.extent();
    const float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    frameBuffers.write(frameIndex, makeFrameUniforms(scene, aspectRatio), objectData, materials.gpuData());
}

void Renderer::State::recordFrame(VkCommandBuffer commands, std::uint32_t imageIndex) {
    const VkImage image = swapchain.image(imageIndex);
    // UNDEFINED: the previous content is cleared anyway. Waits for the acquire semaphore (same stage).
    transitionImage(commands, {image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
    // One depth buffer for both frames in flight: the previous frame's depth writes must be done.
    constexpr VkPipelineStageFlags2 kDepthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    transitionImage(commands, {depth.image(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, kDepthStages,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, kDepthStages,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});

    recordScenePass(commands, imageIndex);
    // Second pass, without depth: the interface is drawn over the scene (decisions 0008 and 0011).
    if (overlay != nullptr) {
        transitionImage(
            commands,
            {image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
        recordOverlayPass(commands, imageIndex);
    }

    transitionImage(commands,
                    {image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE});
}

void Renderer::State::recordScenePass(VkCommandBuffer commands, std::uint32_t imageIndex) {
    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = kClearColor},
    };
    // Depth is not needed after the pass.
    const VkRenderingAttachmentInfo depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {1.0f, 0}},
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, swapchain.extent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment,
    };
    vkCmdBeginRendering(commands, &rendering);

    const VkExtent2D extent = swapchain.extent();
    const VkViewport viewport{.x = 0.0f,
                              .y = 0.0f,
                              .width = static_cast<float>(extent.width),
                              .height = static_cast<float>(extent.height),
                              .minDepth = 0.0f,
                              .maxDepth = 1.0f};
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.handle());
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    const VkDescriptorSet frameSet = frameBuffers.descriptorSet(frameIndex);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.layout(), 0, 1,
                            &frameSet, 0, nullptr);

    counters = FrameCounters{};
    const GpuMesh* boundMesh = nullptr;
    for (std::uint32_t drawIndex = 0; drawIndex < drawItems.size(); ++drawIndex) {
        const GpuMesh* mesh = drawMeshes[drawIndex];
        if (mesh != boundMesh) {
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commands, 0, 1, &mesh->vertices.buffer, &offset);
            vkCmdBindIndexBuffer(commands, mesh->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            boundMesh = mesh;
        }
        // firstInstance = place in the object buffer: the shader reads it through gl_InstanceIndex (G-02).
        vkCmdDrawIndexed(commands, mesh->indexCount, 1, 0, 0, drawIndex);
        ++counters.drawCalls;
        counters.triangles += mesh->indexCount / 3;
    }

    vkCmdEndRendering(commands);
}

void Renderer::State::recordOverlayPass(VkCommandBuffer commands, std::uint32_t imageIndex) {
    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, swapchain.extent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    vkCmdBeginRendering(commands, &rendering);
    overlay->record(OverlayCommands{.commandBuffer = commands, .colorFormat = swapchain.format()});
    vkCmdEndRendering(commands);
}

core::Result<std::unique_ptr<Renderer>> Renderer::create(platform::Window& window, core::Logger& logger,
                                                         ValidationLog& validation,
                                                         const RendererConfig& config) {
    auto state = std::make_unique<State>();
    state->window = &window;
    state->logger = &logger;
    state->vsync = config.vsync;

    core::Result<std::unique_ptr<Device>> device = Device::create(
        window, logger, validation, DeviceConfig{config.applicationName, config.enableValidation});
    if (!device) {
        return std::unexpected(device.error());
    }
    state->device = std::move(*device);

    core::Result<std::unique_ptr<GpuAllocator>> allocator = GpuAllocator::create(*state->device);
    if (!allocator) {
        logger.error("{}", allocator.error().message);
        return std::unexpected(allocator.error());
    }
    state->allocator = std::move(*allocator);

    core::Result<FrameBuffers> frameBuffers = FrameBuffers::create(
        state->device->device(), *state->allocator,
        FrameBufferLimits{
            .framesInFlight = kFramesInFlight, .maxObjects = kMaxObjects, .maxMaterials = kMaxMaterials});
    if (!frameBuffers) {
        logger.error("{}", frameBuffers.error().message);
        return std::unexpected(frameBuffers.error());
    }
    state->frameBuffers = std::move(*frameBuffers);
    const GpuMemoryStats memory = state->allocator->memoryStats();
    constexpr double kMebibyte = 1024.0 * 1024.0;
    logger.info("GPU memory: {:.0f} MiB used, {:.0f} MiB budget{}",
                static_cast<double>(memory.usedBytes) / kMebibyte,
                static_cast<double>(memory.budgetBytes) / kMebibyte,
                state->device->memoryBudgetEnabled() ? "" : " (estimate: VK_EXT_memory_budget unavailable)");

    core::Result<FrameSync> frames =
        FrameSync::create(state->device->device(), state->device->graphicsQueueFamily(), kFramesInFlight);
    if (!frames) {
        logger.error("{}", frames.error().message);
        return std::unexpected(frames.error());
    }
    state->frames = std::move(*frames);

    core::Result<UploadContext> upload = UploadContext::create(
        state->device->device(), state->device->graphicsQueueFamily(), state->device->graphicsQueue());
    if (!upload) {
        logger.error("{}", upload.error().message);
        return std::unexpected(upload.error());
    }
    state->upload = std::move(*upload);

    // Created now when the window has a size: the debug interface needs its format at creation.
    // Otherwise the first renderFrame creates it.
    if (const platform::PixelSize size = window.sizeInPixels(); size.width > 0 && size.height > 0) {
        if (const core::Result<bool> created = state->recreateSwapchain(size); !created) {
            return std::unexpected(created.error());
        }
    }
    return std::make_unique<Renderer>(ConstructionKey{}, std::move(state));
}

Renderer::Renderer(ConstructionKey /*key*/, std::unique_ptr<State> state) noexcept
    : m_state(std::move(state)) {}

Renderer::~Renderer() = default;

core::Result<void> Renderer::renderFrame(const RenderScene& scene) {
    State& state = *m_state;
    APEX_PROFILE_SCOPE("Renderer::renderFrame");

    const platform::PixelSize size = state.window->sizeInPixels();
    switch (state.swapchainState.decide(size)) {
    case FrameDecision::Skip:
        return {};
    case FrameDecision::RecreateThenRender:
        if (const core::Result<bool> recreated = state.recreateSwapchain(size); !recreated) {
            return std::unexpected(recreated.error());
        } else if (!*recreated) {
            return {};
        }
        break;
    case FrameDecision::Render:
        break;
    }

    const VkDevice device = state.device->device();
    const FrameResources& frame = state.frames.frame(state.frameIndex);
    const auto waitStart = std::chrono::steady_clock::now();
    constexpr std::uint64_t kNoTimeout = std::numeric_limits<std::uint64_t>::max();
    if (const VkResult result = vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, kNoTimeout);
        result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkWaitForFences", result);
    }
    if (!state.slotWaited[state.frameIndex]) {
        state.deleter.advanceFrame();
        state.slotWaited[state.frameIndex] = true;
    }

    std::uint32_t imageIndex = 0;
    const VkResult acquired = vkAcquireNextImageKHR(device, state.swapchain.handle(), kNoTimeout,
                                                    frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    const std::chrono::duration<double> waited = std::chrono::steady_clock::now() - waitStart;
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        // Nothing was signaled or submitted: the frame is skipped and the swapchain recreated next time.
        state.swapchainState.markOutOfDate();
        return {};
    }
    if (acquired == VK_SUBOPTIMAL_KHR) {
        // The image is acquired: it must still be rendered and presented.
        state.swapchainState.markOutOfDate();
    } else if (acquired != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkAcquireNextImageKHR", acquired);
    }

    if (const VkResult result = vkResetFences(device, 1, &frame.inFlight); result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkResetFences", result);
    }
    if (const VkResult result = vkResetCommandPool(device, frame.commandPool, 0); result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkResetCommandPool", result);
    }

    const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    if (const VkResult result = vkBeginCommandBuffer(frame.commandBuffer, &beginInfo); result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkBeginCommandBuffer", result);
    }
    state.prepareFrame(scene);
    state.recordFrame(frame.commandBuffer, imageIndex);
    if (const VkResult result = vkEndCommandBuffer(frame.commandBuffer); result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkEndCommandBuffer", result);
    }

    const VkSemaphoreSubmitInfo waitInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                         .semaphore = frame.imageAvailable,
                                         .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSemaphoreSubmitInfo signalInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                           .semaphore = state.swapchain.renderFinished(imageIndex),
                                           .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkCommandBufferSubmitInfo commandInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                .commandBuffer = frame.commandBuffer};
    const VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };
    if (const VkResult result = vkQueueSubmit2(state.device->graphicsQueue(), 1, &submit, frame.inFlight);
        result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkQueueSubmit2", result);
    }

    state.counters.waitSeconds = waited.count();
    state.slotWaited[state.frameIndex] = false;
    state.frameIndex = (state.frameIndex + 1) % kFramesInFlight;
    state.acquiredImage = imageIndex;
    return {};
}

void Renderer::syncResources(const resources::ResourceCache& cache) {
    State& state = *m_state;
    APEX_PROFILE_SCOPE("Renderer::syncResources");
    state.fallbackMesh = cache.fallbackMesh();
    state.meshes.sync(
        cache.meshes(),
        [&state](resources::MeshHandle, const resources::MeshData& mesh) -> std::optional<GpuMesh> {
            core::Result<GpuMesh> created = createGpuMesh(*state.allocator, state.upload, mesh.name, mesh);
            if (!created) {
                state.logger->error("{}", created.error().message);
                return std::nullopt;
            }
            return *created;
        },
        [&state](GpuMesh&& mesh) {
            // A frame in flight may still draw it.
            state.deleter.enqueue(
                [allocator = state.allocator.get(), mesh] { destroyGpuMesh(*allocator, mesh); });
        });
    if (const std::size_t leftOut = state.materials.sync(cache.materials()); leftOut > 0) {
        state.logger->warning("{} material(s) beyond the table's {} entries get the default material",
                              leftOut, kMaxMaterials);
    }
}

GpuMemoryStats Renderer::memoryStats() const {
    return m_state->allocator->memoryStats();
}

FrameCounters Renderer::lastFrameCounters() const noexcept {
    return m_state->counters;
}

void Renderer::setOverlay(RenderOverlay* overlay) noexcept {
    m_state->overlay = overlay;
}

VulkanBackendInfo vulkanBackendInfo(const Renderer& renderer) {
    const Renderer::State& state = *renderer.m_state;
    const bool hasSwapchain = state.swapchain.handle() != VK_NULL_HANDLE;
    return VulkanBackendInfo{
        .apiVersion = VK_API_VERSION_1_3,
        .instance = state.device->instance(),
        .physicalDevice = state.device->physicalDevice(),
        .device = state.device->device(),
        .queueFamily = state.device->graphicsQueueFamily(),
        .queue = state.device->graphicsQueue(),
        .colorFormat = hasSwapchain ? state.swapchain.format() : VK_FORMAT_B8G8R8A8_SRGB,
        .imageCount = std::max(hasSwapchain ? state.swapchain.imageCount() : 0u, kFramesInFlight),
    };
}

core::Result<void> Renderer::presentFrame() {
    State& state = *m_state;
    if (!state.acquiredImage) {
        return {};
    }
    APEX_PROFILE_SCOPE("Renderer::presentFrame");

    const std::uint32_t imageIndex = *std::exchange(state.acquiredImage, std::nullopt);
    const VkSemaphore renderFinished = state.swapchain.renderFinished(imageIndex);
    const VkSwapchainKHR swapchain = state.swapchain.handle();
    const VkPresentInfoKHR present{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };
    const auto presentStart = std::chrono::steady_clock::now();
    const VkResult result = vkQueuePresentKHR(state.device->presentQueue(), &present);
    state.counters.waitSeconds +=
        std::chrono::duration<double>(std::chrono::steady_clock::now() - presentStart).count();
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        state.swapchainState.markOutOfDate();
        return {};
    }
    if (result != VK_SUCCESS) {
        return vulkanError(*state.logger, "vkQueuePresentKHR", result);
    }
    return {};
}

} // namespace apex::graphics
