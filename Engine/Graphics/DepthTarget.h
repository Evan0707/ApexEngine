#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"

#include <vulkan/vulkan.h>

namespace apex::graphics {

// Internal to Graphics. The depth buffer of the main pass, the size of the swapchain. The allocator must
// outlive it.
class DepthTarget {
public:
    // Fails when the format cannot be a depth attachment on this GPU.
    [[nodiscard]] static core::Result<DepthTarget> create(const Device& device, GpuAllocator& allocator,
                                                          VkExtent2D extent, VkFormat format);

    DepthTarget() = default;
    ~DepthTarget();
    DepthTarget(DepthTarget&& other) noexcept;
    DepthTarget& operator=(DepthTarget&& other) noexcept;
    DepthTarget(const DepthTarget&) = delete;
    DepthTarget& operator=(const DepthTarget&) = delete;

    [[nodiscard]] VkImage image() const noexcept;
    [[nodiscard]] VkImageView view() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    GpuAllocator* m_allocator = nullptr;
    GpuImage m_image;
    VkImageView m_view = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
};

} // namespace apex::graphics
