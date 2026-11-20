#include "Graphics/ShadowMap.h"

#include <format>
#include <utility>

namespace apex::graphics {

core::Result<ShadowMap> ShadowMap::create(const Device& device, GpuAllocator& allocator, std::uint32_t size,
                                          VkFormat format) {
    core::Result<DepthTarget> target =
        DepthTarget::create(device, allocator, VkExtent2D{size, size}, format, "shadow map", true);
    if (!target) {
        return std::unexpected(target.error());
    }

    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device.physicalDevice(), format, &properties);
    const bool linear =
        (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    const VkFilter filter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    // Outside the map: depth 1, the farthest, so every compared point is lit.
    const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter = filter,
                                          .minFilter = filter,
                                          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                          .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                          .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                          .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                          .compareEnable = VK_TRUE,
                                          .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                                          .maxLod = 0.0f,
                                          .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
    ShadowMap map;
    map.m_device = device.device();
    map.m_target = std::move(*target);
    map.m_size = size;
    if (const VkResult result = vkCreateSampler(map.m_device, &samplerInfo, nullptr, &map.m_sampler);
        result != VK_SUCCESS) {
        return core::makeError(
            core::ErrorCode::GpuFailure,
            std::format("Cannot create the shadow map sampler: VkResult {}", static_cast<int>(result)));
    }
    return map;
}

ShadowMap::~ShadowMap() {
    destroy();
}

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)), m_target(std::move(other.m_target)),
      m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)), m_size(std::exchange(other.m_size, 0)) {}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_target = std::move(other.m_target);
        m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
        m_size = std::exchange(other.m_size, 0);
    }
    return *this;
}

void ShadowMap::destroy() noexcept {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    m_target = DepthTarget();
}

VkImage ShadowMap::image() const noexcept {
    return m_target.image();
}

VkImageView ShadowMap::view() const noexcept {
    return m_target.view();
}

VkFormat ShadowMap::format() const noexcept {
    return m_target.format();
}

VkSampler ShadowMap::sampler() const noexcept {
    return m_sampler;
}

std::uint32_t ShadowMap::size() const noexcept {
    return m_size;
}

} // namespace apex::graphics
