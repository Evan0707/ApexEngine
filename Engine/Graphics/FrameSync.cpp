#include "Graphics/FrameSync.h"

#include "Core/Assert.h"

#include <format>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

[[nodiscard]] std::unexpected<core::Error> frameError(std::string_view what, VkResult result) {
    return core::makeError(core::ErrorCode::GpuFailure,
                           std::format("Cannot create the {}: VkResult {}", what, static_cast<int>(result)));
}

} // namespace

core::Result<FrameSync> FrameSync::create(VkDevice device, std::uint32_t queueFamily,
                                          std::uint32_t framesInFlight) {
    APEX_ASSERT(framesInFlight > 0, "At least one frame is in flight");
    FrameSync sync;
    sync.m_device = device;

    for (std::uint32_t index = 0; index < framesInFlight; ++index) {
        // Pushed first: destroy() releases whatever was created if a later call fails.
        FrameResources& frame = sync.m_frames.emplace_back();

        // Transient: the pool is reset every frame and its buffer recorded again.
        const VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                               .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                               .queueFamilyIndex = queueFamily};
        if (const VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &frame.commandPool);
            result != VK_SUCCESS) {
            return frameError("command pool", result);
        }

        const VkCommandBufferAllocateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                     .commandPool = frame.commandPool,
                                                     .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                     .commandBufferCount = 1};
        if (const VkResult result = vkAllocateCommandBuffers(device, &bufferInfo, &frame.commandBuffer);
            result != VK_SUCCESS) {
            return frameError("command buffer", result);
        }

        const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                          .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        if (const VkResult result = vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlight);
            result != VK_SUCCESS) {
            return frameError("frame fence", result);
        }

        const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (const VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable);
            result != VK_SUCCESS) {
            return frameError("frame semaphore", result);
        }
    }
    return sync;
}

FrameSync::~FrameSync() {
    destroy();
}

FrameSync::FrameSync(FrameSync&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)), m_frames(std::exchange(other.m_frames, {})) {}

FrameSync& FrameSync::operator=(FrameSync&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_frames = std::exchange(other.m_frames, {});
    }
    return *this;
}

void FrameSync::destroy() noexcept {
    // The vkDestroy functions accept VK_NULL_HANDLE: a partly created frame is released too.
    for (const FrameResources& frame : m_frames) {
        vkDestroySemaphore(m_device, frame.imageAvailable, nullptr);
        vkDestroyFence(m_device, frame.inFlight, nullptr);
        // Frees the command buffer with it.
        vkDestroyCommandPool(m_device, frame.commandPool, nullptr);
    }
    m_frames.clear();
}

std::uint32_t FrameSync::frameCount() const noexcept {
    return static_cast<std::uint32_t>(m_frames.size());
}

const FrameResources& FrameSync::frame(std::uint32_t index) const noexcept {
    APEX_ASSERT(index < m_frames.size(), "Frame index out of range");
    return m_frames[index];
}

} // namespace apex::graphics
