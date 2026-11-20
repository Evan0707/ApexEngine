#pragma once

#include "Core/Error.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace apex::graphics {

// Internal to Graphics. Descriptor set 1, one per material (spec 5.4): binding 0 the base color texture,
// binding 1 the metallic-roughness texture, both through one trilinear, repeating sampler, anisotropic when
// the GPU allows it (decisions 0014 and 0021). The factors stay in the per-frame material table (decision
// 0011).
class MaterialSets {
public:
    // maxSets: sets alive at once, counting those waiting for deferred destruction. maxAnisotropy: 1 for
    // trilinear filtering only (decision 0021).
    [[nodiscard]] static core::Result<MaterialSets> create(VkDevice device, std::uint32_t maxSets,
                                                           float maxAnisotropy);

    MaterialSets() = default;
    ~MaterialSets();
    MaterialSets(MaterialSets&& other) noexcept;
    MaterialSets& operator=(MaterialSets&& other) noexcept;
    MaterialSets(const MaterialSets&) = delete;
    MaterialSets& operator=(const MaterialSets&) = delete;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;

    // Views in SHADER_READ_ONLY_OPTIMAL. Nothing when the pool is exhausted.
    [[nodiscard]] std::optional<VkDescriptorSet> allocate(VkImageView baseColor,
                                                          VkImageView metallicRoughness);
    // Immediate: the caller makes sure no frame in flight still binds it (DeferredDeleter).
    void free(VkDescriptorSet set) noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace apex::graphics
