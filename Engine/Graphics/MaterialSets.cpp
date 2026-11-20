#include "Graphics/MaterialSets.h"

#include <array>
#include <format>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

constexpr std::uint32_t kTexturesPerMaterial = 2;

[[nodiscard]] std::unexpected<core::Error> setsError(std::string_view what, VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("Cannot create the material {}: VkResult {}", what, static_cast<int>(result)));
}

} // namespace

core::Result<MaterialSets> MaterialSets::create(VkDevice device, std::uint32_t maxSets, float maxAnisotropy) {
    MaterialSets sets;
    sets.m_device = device;

    const std::array bindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    const VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    if (const VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &sets.m_layout);
        result != VK_SUCCESS) {
        return setsError("descriptor set layout", result);
    }

    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        kTexturesPerMaterial * maxSets};
    const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                              .maxSets = maxSets,
                                              .poolSizeCount = 1,
                                              .pPoolSizes = &poolSize};
    if (const VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &sets.m_pool);
        result != VK_SUCCESS) {
        return setsError("descriptor pool", result);
    }

    // Trilinear, repeating, every mip level: the samplers of glTF files are ignored (decisions 0014, 0021).
    const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter = VK_FILTER_LINEAR,
                                          .minFilter = VK_FILTER_LINEAR,
                                          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                          .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .anisotropyEnable = maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE,
                                          .maxAnisotropy = maxAnisotropy,
                                          .maxLod = VK_LOD_CLAMP_NONE};
    if (const VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sets.m_sampler);
        result != VK_SUCCESS) {
        return setsError("sampler", result);
    }
    return sets;
}

MaterialSets::~MaterialSets() {
    destroy();
}

MaterialSets::MaterialSets(MaterialSets&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)),
      m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)) {}

MaterialSets& MaterialSets::operator=(MaterialSets&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
        m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    }
    return *this;
}

void MaterialSets::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    // Frees every set too.
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    vkDestroySampler(m_device, m_sampler, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    m_device = VK_NULL_HANDLE;
}

VkDescriptorSetLayout MaterialSets::layout() const noexcept {
    return m_layout;
}

std::optional<VkDescriptorSet> MaterialSets::allocate(VkImageView baseColor, VkImageView metallicRoughness) {
    const VkDescriptorSetAllocateInfo allocateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   .descriptorPool = m_pool,
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = &m_layout};
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(m_device, &allocateInfo, &set) != VK_SUCCESS) {
        return std::nullopt;
    }
    const std::array images{
        VkDescriptorImageInfo{m_sampler, baseColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        VkDescriptorImageInfo{m_sampler, metallicRoughness, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };
    std::array<VkWriteDescriptorSet, kTexturesPerMaterial> writes{};
    for (std::uint32_t binding = 0; binding < writes.size(); ++binding) {
        writes[binding] = VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               .dstSet = set,
                                               .dstBinding = binding,
                                               .descriptorCount = 1,
                                               .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               .pImageInfo = &images[binding]};
    }
    vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    return set;
}

void MaterialSets::free(VkDescriptorSet set) noexcept {
    (void)vkFreeDescriptorSets(m_device, m_pool, 1, &set);
}

} // namespace apex::graphics
