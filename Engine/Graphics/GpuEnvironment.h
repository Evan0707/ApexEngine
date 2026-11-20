#pragma once

#include "Core/Error.h"
#include "Graphics/EnvironmentLighting.h"
#include "Graphics/EnvironmentPrefilter.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/GpuTexture.h"
#include "Graphics/LightingSets.h"
#include "Graphics/UploadContext.h"
#include "Resources/EnvironmentData.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string_view>

namespace apex::graphics {

// The specular cubemap (decision 0022): pixels per side of its first level, and its levels, one per
// roughness from 0 to 1. Its format is chosen here only (G-09).
inline constexpr std::uint32_t kSpecularCubeSize = 256;
inline constexpr std::uint32_t kSpecularCubeLevels = 6;
inline constexpr VkFormat kEnvironmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr std::uint32_t kPrefilterSamples = 128;

// Internal to Graphics. An environment on the GPU: its prefiltered specular cubemap, its irradiance (nine
// coefficients, copied into the frame uniforms) and its lighting set.
struct GpuEnvironment {
    GpuTexture specular;
    IrradianceSh irradiance;
    VkDescriptorSet lightingSet = VK_NULL_HANDLE;
};

// Projects the irradiance, uploads the equirectangular image with its mips, prefilters the cubemap and waits
// for the GPU; the image is freed afterwards. brdfTable: the view the lighting set refers to. The error
// names the environment (spec 5.4).
[[nodiscard]] core::Result<GpuEnvironment>
createGpuEnvironment(VkDevice device, GpuAllocator& allocator, UploadContext& upload,
                     EnvironmentPrefilter& prefilter, LightingSets& lightingSets, VkImageView brdfTable,
                     std::string_view name, const resources::EnvironmentData& environment);

// Immediate: the caller makes sure no frame in flight still uses it (DeferredDeleter).
void destroyGpuEnvironment(VkDevice device, GpuAllocator& allocator, LightingSets& lightingSets,
                           const GpuEnvironment& environment) noexcept;

// The split-sum table (computeBrdfTable), as a 16-bit float RG texture.
[[nodiscard]] core::Result<GpuTexture> createBrdfTable(VkDevice device, GpuAllocator& allocator,
                                                       UploadContext& upload);

// A black cubemap of 1 x 1 pixels per face: the lighting set without an environment, never lit.
[[nodiscard]] core::Result<GpuTexture> createBlackCube(VkDevice device, GpuAllocator& allocator,
                                                       UploadContext& upload);

} // namespace apex::graphics
