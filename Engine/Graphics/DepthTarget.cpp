#include "Graphics/DepthTarget.h"

#include <format>
#include <utility>

namespace apex::graphics {

core::Result<DepthTarget> DepthTarget::create(const Device& device, GpuAllocator& allocator,
                                              VkExtent2D extent, VkFormat format, std::string_view name,
                                              bool sampled) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device.physicalDevice(), format, &properties);
    VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (sampled) {
        needed |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    }
    if ((properties.optimalTilingFeatures & needed) != needed) {
        return core::makeError(core::ErrorCode::GpuFailure,
                               std::format("The GPU cannot use format {} for the {}{}",
                                           static_cast<int>(format), name,
                                           sampled ? " (depth attachment, sampled)" : ""));
    }

    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 (sampled ? VkImageUsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT} : VkImageUsageFlags{0}),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    core::Result<GpuImage> image = allocator.createImage(name, imageInfo, true);
    if (!image) {
        return std::unexpected(image.error());
    }

    DepthTarget target;
    target.m_device = device.device();
    target.m_allocator = &allocator;
    target.m_image = *image;
    target.m_format = format;

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = target.m_image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
    };
    if (const VkResult result = vkCreateImageView(target.m_device, &viewInfo, nullptr, &target.m_view);
        result != VK_SUCCESS) {
        return core::makeError(
            core::ErrorCode::GpuFailure,
            std::format("Cannot create the {} view: VkResult {}", name, static_cast<int>(result)));
    }
    return target;
}

DepthTarget::~DepthTarget() {
    destroy();
}

DepthTarget::DepthTarget(DepthTarget&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_allocator(std::exchange(other.m_allocator, nullptr)),
      m_image(std::exchange(other.m_image, GpuImage{})), m_view(std::exchange(other.m_view, VK_NULL_HANDLE)),
      m_format(other.m_format) {}

DepthTarget& DepthTarget::operator=(DepthTarget&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_image = std::exchange(other.m_image, GpuImage{});
        m_view = std::exchange(other.m_view, VK_NULL_HANDLE);
        m_format = other.m_format;
    }
    return *this;
}

void DepthTarget::destroy() noexcept {
    if (m_allocator == nullptr) {
        return;
    }
    vkDestroyImageView(m_device, m_view, nullptr);
    m_allocator->destroyImage(m_image);
    m_view = VK_NULL_HANDLE;
    m_image = GpuImage{};
    m_allocator = nullptr;
}

VkImage DepthTarget::image() const noexcept {
    return m_image.image;
}

VkImageView DepthTarget::view() const noexcept {
    return m_view;
}

VkFormat DepthTarget::format() const noexcept {
    return m_format;
}

} // namespace apex::graphics
