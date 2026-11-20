#include "Graphics/ImageInputSet.h"

#include <format>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

[[nodiscard]] std::unexpected<core::Error> inputError(std::string_view what, VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("Cannot create the image input {}: VkResult {}", what, static_cast<int>(result)));
}

} // namespace

core::Result<ImageInputSet> ImageInputSet::create(VkDevice device) {
    ImageInputSet input;
    input.m_device = device;
    const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    const VkDescriptorSetLayoutCreateInfo layoutInfo{.sType =
                                                         VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                     .bindingCount = 1,
                                                     .pBindings = &binding};
    if (const VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &input.m_layout);
        result != VK_SUCCESS) {
        return inputError("descriptor set layout", result);
    }
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              .maxSets = 1,
                                              .poolSizeCount = 1,
                                              .pPoolSizes = &poolSize};
    if (const VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &input.m_pool);
        result != VK_SUCCESS) {
        return inputError("descriptor pool", result);
    }
    const VkDescriptorSetAllocateInfo allocateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   .descriptorPool = input.m_pool,
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = &input.m_layout};
    if (const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &input.m_set);
        result != VK_SUCCESS) {
        return inputError("descriptor set", result);
    }
    const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter = VK_FILTER_NEAREST,
                                          .minFilter = VK_FILTER_NEAREST,
                                          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                          .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          .maxLod = 0.0f};
    if (const VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &input.m_sampler);
        result != VK_SUCCESS) {
        return inputError("sampler", result);
    }
    return input;
}

ImageInputSet::~ImageInputSet() {
    destroy();
}

ImageInputSet::ImageInputSet(ImageInputSet&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)), m_set(std::exchange(other.m_set, VK_NULL_HANDLE)),
      m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)) {}

ImageInputSet& ImageInputSet::operator=(ImageInputSet&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
        m_set = std::exchange(other.m_set, VK_NULL_HANDLE);
        m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    }
    return *this;
}

void ImageInputSet::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    vkDestroySampler(m_device, m_sampler, nullptr);
    // Frees the set too.
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    m_device = VK_NULL_HANDLE;
}

VkDescriptorSetLayout ImageInputSet::layout() const noexcept {
    return m_layout;
}

VkDescriptorSet ImageInputSet::set() const noexcept {
    return m_set;
}

void ImageInputSet::bind(VkImageView view) noexcept {
    const VkDescriptorImageInfo image{m_sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_set,
                                     .dstBinding = 0,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .pImageInfo = &image};
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

} // namespace apex::graphics
