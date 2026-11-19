#include "Graphics/Renderer.h"

#include "Core/Profiling.h"
#include "Graphics/DeferredDeleter.h"
#include "Graphics/Device.h"
#include "Graphics/FrameSync.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/Swapchain.h"
#include "Graphics/SwapchainState.h"
#include "Graphics/TrianglePipeline.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

// Spec 5.4.
constexpr std::uint32_t kFramesInFlight = 2;

// Linear color, written to an sRGB swapchain: a dark blue, easy to tell from a black window.
constexpr VkClearColorValue kClearColor{{0.02f, 0.04f, 0.10f, 1.0f}};

// Vulkan clip space: +Y points down until the phase 3 projection flips it (spec 3.3). Linear colors.
constexpr std::array<TriangleVertex, 3> kTriangle{{
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
}};

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

void transitionImage(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
                     VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                     VkAccessFlags2 dstAccess) {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
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
    GpuBuffer triangleVertices;
    TrianglePipeline trianglePipeline;
    Swapchain swapchain;
    FrameCounters counters;
    bool vsync = true;
    SwapchainState swapchainState;
    std::uint32_t frameIndex = 0;
    // True once the fence of a slot was waited for and the deleter advanced, until the slot is submitted
    // again.
    std::array<bool, kFramesInFlight> slotWaited{};
    // Set by renderFrame, consumed by presentFrame.
    std::optional<std::uint32_t> acquiredImage;

    ~State() {
        if (device) {
            device->waitIdle();
        }
        if (allocator && triangleVertices.buffer != VK_NULL_HANDLE) {
            allocator->destroyBuffer(triangleVertices);
        }
    }

    // False when the surface has no area yet: the frame is skipped and the recreation tried again.
    [[nodiscard]] core::Result<bool> recreateSwapchain(platform::PixelSize size);
    [[nodiscard]] core::Result<void> ensurePipeline();
    void recordFrame(VkCommandBuffer commands, std::uint32_t imageIndex);
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
    const RenderTargetDesc target{.colorFormat = swapchain.format(), .samples = VK_SAMPLE_COUNT_1_BIT};
    if (trianglePipeline.handle() != VK_NULL_HANDLE && trianglePipeline.target() == target) {
        return {};
    }
    core::Result<TrianglePipeline> created = TrianglePipeline::create(device->device(), target);
    if (!created) {
        logger->error("{}", created.error().message);
        return std::unexpected(created.error());
    }
    // A frame in flight may still use the old pipeline.
    deleter.enqueue([old = std::move(trianglePipeline)]() mutable { old = TrianglePipeline(); });
    trianglePipeline = std::move(*created);
    return {};
}

void Renderer::State::recordFrame(VkCommandBuffer commands, std::uint32_t imageIndex) {
    const VkImage image = swapchain.image(imageIndex);
    // UNDEFINED: the previous content is cleared anyway. Waits for the acquire semaphore (same stage).
    transitionImage(commands, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = kClearColor},
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, swapchain.extent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
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
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline.handle());
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commands, 0, 1, &triangleVertices.buffer, &offset);
    vkCmdDraw(commands, static_cast<std::uint32_t>(kTriangle.size()), 1, 0, 0);
    counters = FrameCounters{.drawCalls = 1, .triangles = kTriangle.size() / 3};

    vkCmdEndRendering(commands);

    transitionImage(commands, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
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

    core::Result<GpuBuffer> vertices = state->allocator->createBufferWithData(
        "Triangle vertices", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, std::as_bytes(std::span(kTriangle)));
    if (!vertices) {
        logger.error("{}", vertices.error().message);
        return std::unexpected(vertices.error());
    }
    state->triangleVertices = *vertices;
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

    // The swapchain is created by the first renderFrame, once the window has a size.
    return std::make_unique<Renderer>(ConstructionKey{}, std::move(state));
}

Renderer::Renderer(ConstructionKey /*key*/, std::unique_ptr<State> state) noexcept
    : m_state(std::move(state)) {}

Renderer::~Renderer() = default;

core::Result<void> Renderer::renderFrame() {
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
    const VkResult result = vkQueuePresentKHR(state.device->presentQueue(), &present);
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
