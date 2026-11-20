#pragma once

#include "Core/Error.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace apex::graphics {

// Internal to Graphics. Descriptor set 2 of the scene and sky pipelines (decision 0022): binding 0 the
// prefiltered specular cubemap of the environment, binding 1 the split-sum BRDF table. One set per
// environment on the GPU, plus the default one; both images read through one linear, clamped sampler.
class LightingSets {
public:
    // maxSets: sets alive at once, counting those waiting for deferred destruction.
    [[nodiscard]] static core::Result<LightingSets> create(VkDevice device, std::uint32_t maxSets);

    LightingSets() = default;
    ~LightingSets();
    LightingSets(LightingSets&& other) noexcept;
    LightingSets& operator=(LightingSets&& other) noexcept;
    LightingSets(const LightingSets&) = delete;
    LightingSets& operator=(const LightingSets&) = delete;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;

    // Views in SHADER_READ_ONLY_OPTIMAL. Nothing when the pool is exhausted.
    [[nodiscard]] std::optional<VkDescriptorSet> allocate(VkImageView specularCube, VkImageView brdfTable);
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
