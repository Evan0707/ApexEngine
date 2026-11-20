#include "Graphics/GpuTexture.h"

#include "Graphics/MipLevels.h"

#include <format>
#include <span>

namespace apex::graphics {

VkFormat textureFormat(resources::ColorSpace colorSpace) noexcept {
    return colorSpace == resources::ColorSpace::Srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
}

core::Result<GpuTexture> createGpuTexture(VkDevice device, GpuAllocator& allocator, UploadContext& upload,
                                          std::string_view name, const resources::TextureData& texture) {
    const VkFormat format = textureFormat(texture.colorSpace);
    // The whole chain, made on the GPU from the first level (decision 0021).
    const std::uint32_t levels = mipLevelCount(texture.width, texture.height);
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {texture.width, texture.height, 1},
        .mipLevels = levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    core::Result<GpuImage> image = allocator.createImage(name, imageInfo, false);
    if (!image) {
        return std::unexpected(image.error());
    }
    GpuTexture gpuTexture{.image = *image};

    const ImageCopy copy{.destination = gpuTexture.image.image,
                         .width = texture.width,
                         .height = texture.height,
                         .data = std::as_bytes(std::span(texture.pixels)),
                         .mipLevels = levels};
    if (core::Result<void> uploaded = upload.uploadImage(allocator, name, copy); !uploaded) {
        destroyGpuTexture(device, allocator, gpuTexture);
        return std::unexpected(uploaded.error());
    }

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = gpuTexture.image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1},
    };
    if (const VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &gpuTexture.view);
        result != VK_SUCCESS) {
        destroyGpuTexture(device, allocator, gpuTexture);
        return core::makeError(core::ErrorCode::GpuFailure,
                               std::format("Cannot create the view of texture '{}': VkResult {}", name,
                                           static_cast<int>(result)));
    }
    return gpuTexture;
}

void destroyGpuTexture(VkDevice device, GpuAllocator& allocator, const GpuTexture& texture) noexcept {
    vkDestroyImageView(device, texture.view, nullptr);
    allocator.destroyImage(texture.image);
}

} // namespace apex::graphics
