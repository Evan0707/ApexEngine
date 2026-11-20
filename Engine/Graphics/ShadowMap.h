#pragma once

#include "Core/Error.h"
#include "Graphics/DepthTarget.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace apex::graphics {

// Internal to Graphics. The depth image of the shadow pass (spec 5.4, decision 0021), square, with the
// comparison sampler the scene shader reads it through: a texel lit when the compared depth is not greater
// than the stored one, outside the image always lit. The allocator must outlive it.
class ShadowMap {
public:
    // size: pixels per side (G-09). Linear filtering (a 2 x 2 comparison per tap) when the format allows it.
    [[nodiscard]] static core::Result<ShadowMap> create(const Device& device, GpuAllocator& allocator,
                                                        std::uint32_t size, VkFormat format);

    ShadowMap() = default;
    ~ShadowMap();
    ShadowMap(ShadowMap&& other) noexcept;
    ShadowMap& operator=(ShadowMap&& other) noexcept;
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    [[nodiscard]] VkImage image() const noexcept;
    [[nodiscard]] VkImageView view() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;
    [[nodiscard]] VkSampler sampler() const noexcept;
    [[nodiscard]] std::uint32_t size() const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    DepthTarget m_target;
    VkSampler m_sampler = VK_NULL_HANDLE;
    std::uint32_t m_size = 0;
};

} // namespace apex::graphics
