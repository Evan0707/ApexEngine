#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/UploadContext.h"
#include "Resources/TextureData.h"

#include <vulkan/vulkan.h>

#include <string_view>

namespace apex::graphics {

// Internal to Graphics. A sampled texture: RGBA 8 bits with its full mip chain (decision 0021), in an sRGB
// format for colors and a UNORM format for data (spec 3.3).
struct GpuTexture {
    GpuImage image;
    VkImageView view = VK_NULL_HANDLE;
};

[[nodiscard]] VkFormat textureFormat(resources::ColorSpace colorSpace) noexcept;

// Uploads the texture and waits for the copy. The error names the texture (spec 5.4).
[[nodiscard]] core::Result<GpuTexture> createGpuTexture(VkDevice device, GpuAllocator& allocator,
                                                        UploadContext& upload, std::string_view name,
                                                        const resources::TextureData& texture);

// Immediate: the caller makes sure no frame in flight still samples it (DeferredDeleter).
void destroyGpuTexture(VkDevice device, GpuAllocator& allocator, const GpuTexture& texture) noexcept;

} // namespace apex::graphics
