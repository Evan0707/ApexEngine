#pragma once

#include "Core/Error.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace apex::graphics {

// What one frame in flight needs on its own (spec 5.4: two frames in flight).
struct FrameResources {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    // Signaled when the GPU has finished the frame. Created signaled: the first wait returns at once.
    VkFence inFlight = VK_NULL_HANDLE;
    // Signaled when the acquired swapchain image can be written.
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
};

// Internal to Graphics. Owns the per-frame command buffers and synchronization objects.
class FrameSync {
public:
    [[nodiscard]] static core::Result<FrameSync> create(VkDevice device, std::uint32_t queueFamily,
                                                        std::uint32_t framesInFlight);

    FrameSync() = default;
    ~FrameSync();
    FrameSync(FrameSync&& other) noexcept;
    FrameSync& operator=(FrameSync&& other) noexcept;
    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    [[nodiscard]] std::uint32_t frameCount() const noexcept;
    [[nodiscard]] const FrameResources& frame(std::uint32_t index) const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<FrameResources> m_frames;
};

} // namespace apex::graphics
