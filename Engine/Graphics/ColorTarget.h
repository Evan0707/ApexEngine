#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"

#include <vulkan/vulkan.h>

#include <string_view>

namespace apex::graphics {

// Internal to Graphics. A color image drawn into, then sampled by a later pass: the HDR image of the scene
// (decision 0021), the size of the swapchain. The allocator must outlive it.
class ColorTarget {
public:
    // Fails when the format cannot be both a color attachment and sampled on this GPU. The name appears in
    // the error messages and in VMA.
    [[nodiscard]] static core::Result<ColorTarget> create(const Device& device, GpuAllocator& allocator,
                                                          VkExtent2D extent, VkFormat format,
                                                          std::string_view name);

    ColorTarget() = default;
    ~ColorTarget();
    ColorTarget(ColorTarget&& other) noexcept;
    ColorTarget& operator=(ColorTarget&& other) noexcept;
    ColorTarget(const ColorTarget&) = delete;
    ColorTarget& operator=(const ColorTarget&) = delete;

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
