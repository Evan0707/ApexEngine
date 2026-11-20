#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"

#include <vulkan/vulkan.h>

#include <string_view>

namespace apex::graphics {

// Internal to Graphics. A depth image with its view: the depth buffer of the main pass, the size of the
// swapchain, and the shadow map. The allocator must outlive it.
class DepthTarget {
public:
    // Fails when the format cannot be a depth attachment on this GPU, or cannot be sampled when `sampled`.
    // The name appears in the error messages and in VMA.
    [[nodiscard]] static core::Result<DepthTarget> create(const Device& device, GpuAllocator& allocator,
                                                          VkExtent2D extent, VkFormat format,
                                                          std::string_view name, bool sampled);

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
