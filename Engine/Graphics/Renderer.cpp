#include "Graphics/Renderer.h"

#include "Core/Profiling.h"
#include "Graphics/RendererState.h"
#include "Graphics/ShaderBinaries.h"
#include "Graphics/VulkanBackendAccess.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

// 32-bit float depth (spec 5.4). Chosen here only, then passed to the pipelines (G-09).
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
// 16-bit fixed point (decision 0021): a constant depth bias in steps of 1/65536 of the shadow depth range,
// which converts to meters. Chosen here only (G-09).
constexpr VkFormat kShadowMapFormat = VK_FORMAT_D16_UNORM;
// The scene's linear HDR image (spec 5.4): 16-bit floats. Chosen here only (G-09).
constexpr VkFormat kSceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

[[nodiscard]] std::string_view presentModeName(bool vsync) noexcept {
    return vsync ? "vsync on" : "vsync off";
}

[[nodiscard]] bool isSrgb(VkFormat format) noexcept {
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB ||
           format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}

} // namespace

std::unexpected<core::Error> vulkanError(core::Logger& logger, std::string_view what, VkResult result) {
    std::string message = std::format("{} failed: VkResult {}", what, static_cast<int>(result));
    logger.error("{}", message);
    return core::makeError(core::ErrorCode::GpuFailure, std::move(message));
}

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
        DepthTarget::create(*device, *allocator, swapchain.extent(), kDepthFormat, "depth buffer", false);
    if (!depthTarget) {
        logger->error("{}", depthTarget.error().message);
        return std::unexpected(depthTarget.error());
    }
    depth = std::move(*depthTarget);
    sceneColor = ColorTarget();
    core::Result<ColorTarget> colorTarget =
        ColorTarget::create(*device, *allocator, swapchain.extent(), kSceneColorFormat, "HDR scene image");
    if (!colorTarget) {
        logger->error("{}", colorTarget.error().message);
        return std::unexpected(colorTarget.error());
    }
    sceneColor = std::move(*colorTarget);
    // The GPU is idle: no frame in flight reads the set.
    sceneColorInput.bind(sceneColor.view());
    swapchainState.recreated(size);
    if (core::Result<void> pipeline = ensurePipelines(); !pipeline) {
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

core::Result<void> Renderer::State::ensurePipelines() {
    const RenderTargetDesc tonemapTarget{.colorFormat = swapchain.format(),
                                         .depthFormat = VK_FORMAT_UNDEFINED,
                                         .samples = VK_SAMPLE_COUNT_1_BIT};
    if (tonemapPipeline.handle() == VK_NULL_HANDLE || !(tonemapPipeline.target() == tonemapTarget)) {
        const std::array tonemapLayouts{frameBuffers.layout(), sceneColorInput.layout()};
        core::Result<GraphicsPipeline> tonemap =
            GraphicsPipeline::create(device->device(), tonemapTarget,
                                     PipelineDesc{.name = "tonemapping pipeline",
                                                  .vertexShader = fullscreenVertexShader(),
                                                  .fragmentShader = tonemapFragmentShader(),
                                                  .setLayouts = tonemapLayouts,
                                                  .cullMode = VK_CULL_MODE_NONE});
        if (!tonemap) {
            logger->error("{}", tonemap.error().message);
            return std::unexpected(tonemap.error());
        }
        deleter.enqueue([old = std::move(tonemapPipeline)]() mutable { old = GraphicsPipeline(); });
        tonemapPipeline = std::move(*tonemap);
    }
    const RenderTargetDesc lineTarget{
        .colorFormat = swapchain.format(), .depthFormat = depth.format(), .samples = VK_SAMPLE_COUNT_1_BIT};
    if (linePipeline.handle() == VK_NULL_HANDLE || !(linePipeline.target() == lineTarget)) {
        const std::array lineLayouts{frameBuffers.layout()};
        core::Result<GraphicsPipeline> lines =
            GraphicsPipeline::create(device->device(), lineTarget,
                                     PipelineDesc{.name = "line pipeline",
                                                  .vertexShader = lineVertexShader(),
                                                  .fragmentShader = lineFragmentShader(),
                                                  .setLayouts = lineLayouts,
                                                  .lineVertices = true,
                                                  .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                                  .cullMode = VK_CULL_MODE_NONE,
                                                  .depthWrite = false,
                                                  .depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL});
        if (!lines) {
            logger->error("{}", lines.error().message);
            return std::unexpected(lines.error());
        }
        deleter.enqueue([old = std::move(linePipeline)]() mutable { old = GraphicsPipeline(); });
        linePipeline = std::move(*lines);
    }

    const RenderTargetDesc target{
        .colorFormat = sceneColor.format(), .depthFormat = depth.format(), .samples = VK_SAMPLE_COUNT_1_BIT};
    if (scenePipeline.handle() != VK_NULL_HANDLE && scenePipeline.target() == target) {
        return {};
    }
    const std::array setLayouts{frameBuffers.layout(), materialSets.layout(), lightingSets.layout()};
    // Same layouts as the scene's: the sets bound for the objects stay bound for the sky.
    core::Result<GraphicsPipeline> sky =
        GraphicsPipeline::create(device->device(), target,
                                 PipelineDesc{.name = "sky pipeline",
                                              .vertexShader = skyVertexShader(),
                                              .fragmentShader = skyFragmentShader(),
                                              .setLayouts = setLayouts,
                                              .cullMode = VK_CULL_MODE_NONE,
                                              .depthWrite = false,
                                              .depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL});
    if (!sky) {
        logger->error("{}", sky.error().message);
        return std::unexpected(sky.error());
    }
    deleter.enqueue([old = std::move(skyPipeline)]() mutable { old = GraphicsPipeline(); });
    skyPipeline = std::move(*sky);
    core::Result<GraphicsPipeline> created = GraphicsPipeline::create(
        device->device(), target,
        PipelineDesc{.name = "scene pipeline",
                     .vertexShader = sceneVertexShader(),
                     .fragmentShader = sceneFragmentShader(),
                     .setLayouts = setLayouts,
                     .meshAttributes = kMeshPosition | kMeshNormal | kMeshTangent | kMeshUv});
    if (!created) {
        logger->error("{}", created.error().message);
        return std::unexpected(created.error());
    }
    // A frame in flight may still use the old pipeline.
    deleter.enqueue([old = std::move(scenePipeline)]() mutable { old = GraphicsPipeline(); });
    scenePipeline = std::move(*created);
    return {};
}

core::Result<void> Renderer::State::createLightingResources() {
    core::Result<LightingSets> sets = LightingSets::create(device->device(), kMaxLightingSets);
    if (!sets) {
        logger->error("{}", sets.error().message);
        return std::unexpected(sets.error());
    }
    lightingSets = std::move(*sets);
    core::Result<GpuTexture> table = createBrdfTable(device->device(), *allocator, upload);
    if (!table) {
        logger->error("{}", table.error().message);
        return std::unexpected(table.error());
    }
    brdfTable = *table;
    core::Result<GpuTexture> black = createBlackCube(device->device(), *allocator, upload);
    if (!black) {
        logger->error("{}", black.error().message);
        return std::unexpected(black.error());
    }
    blackCube = *black;
    const std::optional<VkDescriptorSet> set = lightingSets.allocate(blackCube.view, brdfTable.view);
    if (!set) {
        return vulkanError(*logger, "Allocating the default lighting set", VK_ERROR_OUT_OF_POOL_MEMORY);
    }
    defaultLightingSet = *set;
    core::Result<EnvironmentPrefilter> created =
        EnvironmentPrefilter::create(device->device(), *allocator, kEnvironmentFormat);
    if (!created) {
        logger->error("{}", created.error().message);
        return std::unexpected(created.error());
    }
    prefilter = std::move(*created);
    return {};
}

core::Result<void> Renderer::State::createShadowResources(std::uint32_t size) {
    core::Result<ShadowMap> map = ShadowMap::create(*device, *allocator, size, kShadowMapFormat);
    if (!map) {
        logger->error("{}", map.error().message);
        return std::unexpected(map.error());
    }
    shadowMap = std::move(*map);
    frameBuffers.bindShadowMap(shadowMap.view(), shadowMap.sampler());

    const std::array setLayouts{frameBuffers.layout()};
    // No culling: a thin or open mesh still casts its shadow; the depth bias keeps lit faces out of it.
    core::Result<GraphicsPipeline> pipeline =
        GraphicsPipeline::create(device->device(),
                                 RenderTargetDesc{.colorFormat = VK_FORMAT_UNDEFINED,
                                                  .depthFormat = kShadowMapFormat,
                                                  .samples = VK_SAMPLE_COUNT_1_BIT},
                                 PipelineDesc{.name = "shadow pipeline",
                                              .vertexShader = shadowVertexShader(),
                                              .setLayouts = setLayouts,
                                              .meshAttributes = kMeshPosition,
                                              .cullMode = VK_CULL_MODE_NONE,
                                              .dynamicDepthBias = true});
    if (!pipeline) {
        logger->error("{}", pipeline.error().message);
        return std::unexpected(pipeline.error());
    }
    shadowPipeline = std::move(*pipeline);
    logger->info("Shadow map: {}x{} pixels", size, size);
    return {};
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

    core::Result<FrameBuffers> frameBuffers =
        FrameBuffers::create(state->device->device(), *state->allocator,
                             FrameBufferLimits{.framesInFlight = kFramesInFlight,
                                               .maxObjects = kMaxObjects,
                                               .maxMaterials = kMaxMaterials,
                                               .maxLineVertices = kMaxLineVertices});
    if (!frameBuffers) {
        logger.error("{}", frameBuffers.error().message);
        return std::unexpected(frameBuffers.error());
    }
    state->frameBuffers = std::move(*frameBuffers);

    core::Result<MaterialSets> materialSets =
        MaterialSets::create(state->device->device(), kMaxMaterialSets, state->device->maxAnisotropy());
    if (!materialSets) {
        logger.error("{}", materialSets.error().message);
        return std::unexpected(materialSets.error());
    }
    state->materialSets = std::move(*materialSets);
    if (core::Result<void> shadows = state->createShadowResources(config.shadowMapSize); !shadows) {
        return std::unexpected(shadows.error());
    }
    core::Result<ImageInputSet> sceneColorInput = ImageInputSet::create(state->device->device());
    if (!sceneColorInput) {
        logger.error("{}", sceneColorInput.error().message);
        return std::unexpected(sceneColorInput.error());
    }
    state->sceneColorInput = std::move(*sceneColorInput);
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

    core::Result<GpuTimer> gpuTimer =
        GpuTimer::create(state->device->device(), state->device->timestampFormat(), kFramesInFlight);
    if (!gpuTimer) {
        logger.error("{}", gpuTimer.error().message);
        return std::unexpected(gpuTimer.error());
    }
    state->gpuTimer = std::move(*gpuTimer);

    core::Result<UploadContext> upload = UploadContext::create(
        state->device->device(), state->device->graphicsQueueFamily(), state->device->graphicsQueue());
    if (!upload) {
        logger.error("{}", upload.error().message);
        return std::unexpected(upload.error());
    }
    state->upload = std::move(*upload);
    if (core::Result<void> lighting = state->createLightingResources(); !lighting) {
        return std::unexpected(lighting.error());
    }

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
        if (const std::optional<double> gpuSeconds = state.gpuTimer.read(state.frameIndex)) {
            state.counters.gpuSeconds = gpuSeconds;
        }
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
