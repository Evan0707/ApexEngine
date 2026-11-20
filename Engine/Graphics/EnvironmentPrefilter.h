#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/GraphicsPipeline.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>

namespace apex::graphics {

// Uniform buffer of Shaders/Prefilter.frag, std140.
struct PrefilterParams {
    // Pixels per side of the cubemap's first level, and its number of levels.
    std::uint32_t faceSize = 0;
    std::uint32_t levels = 0;
    // GGX samples per texel, for every level but the first, which copies the source.
    std::uint32_t samples = 0;
    // Steradians covered by one texel of the source's first level, on average: picks the source level that
    // each sample reads (filtered importance sampling).
    float sourceTexelSolidAngle = 0.0f;
};
static_assert(sizeof(PrefilterParams) == 16);

// Internal to Graphics. Draws the specular cubemap of an environment from its equirectangular image
// (decision 0022): one full-screen triangle per face and level, each level for a roughness from 0 (the
// first) to 1 (the last), GGX importance sampling. The allocator must outlive it.
class EnvironmentPrefilter {
public:
    // cubeFormat: the cubemap's format (G-09).
    [[nodiscard]] static core::Result<EnvironmentPrefilter> create(VkDevice device, GpuAllocator& allocator,
                                                                   VkFormat cubeFormat);

    EnvironmentPrefilter() = default;
    ~EnvironmentPrefilter();
    EnvironmentPrefilter(EnvironmentPrefilter&& other) noexcept;
    EnvironmentPrefilter& operator=(EnvironmentPrefilter&& other) noexcept;
    EnvironmentPrefilter(const EnvironmentPrefilter&) = delete;
    EnvironmentPrefilter& operator=(const EnvironmentPrefilter&) = delete;

    // source: the equirectangular image with its mips, in SHADER_READ_ONLY_OPTIMAL. faceViews: one 2D view
    // per level and face, level by level, faces in Vulkan's order (+X, -X, +Y, -Y, +Z, -Z). Every level of
    // the cube ends in SHADER_READ_ONLY_OPTIMAL. The previous recording must have finished on the GPU.
    void record(VkCommandBuffer commands, VkImageView source, VkImage cube,
                std::span<const VkImageView> faceViews, const PrefilterParams& params);

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    GpuAllocator* m_allocator = nullptr;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    GpuBuffer m_params;
    GraphicsPipeline m_pipeline;
};

} // namespace apex::graphics
