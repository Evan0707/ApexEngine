#pragma once

#include "Core/Error.h"

#include <vulkan/vulkan.h>

namespace apex::graphics {

// Internal to Graphics. One descriptor set with one sampled image, binding 0, for the fragment stage: how the
// tonemapping pass reads the HDR image of the scene (decision 0021). Point sampling, clamped to the edge.
class ImageInputSet {
public:
    [[nodiscard]] static core::Result<ImageInputSet> create(VkDevice device);

    ImageInputSet() = default;
    ~ImageInputSet();
    ImageInputSet(ImageInputSet&& other) noexcept;
    ImageInputSet& operator=(ImageInputSet&& other) noexcept;
    ImageInputSet(const ImageInputSet&) = delete;
    ImageInputSet& operator=(const ImageInputSet&) = delete;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;
    [[nodiscard]] VkDescriptorSet set() const noexcept;

    // The view, in SHADER_READ_ONLY_OPTIMAL. The set must not be in use by a frame in flight.
    void bind(VkImageView view) noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace apex::graphics
