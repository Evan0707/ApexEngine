#include "Graphics/EnvironmentPrefilter.h"

#include "Core/Assert.h"
#include "Graphics/MipLevels.h"
#include "Graphics/ShaderBinaries.h"

#include <array>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

constexpr std::uint32_t kCubeFaces = 6;

[[nodiscard]] std::unexpected<core::Error> prefilterError(std::string_view what, VkResult result) {
    return core::makeError(core::ErrorCode::GpuFailure,
                           std::format("Cannot create the environment prefilter's {}: VkResult {}", what,
                                       static_cast<int>(result)));
}

void cubeBarrier(VkCommandBuffer commands, VkImage cube, VkImageLayout from, VkImageLayout to,
                 VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                 VkAccessFlags2 dstAccess) {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = cube,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, kCubeFaces}};
    const VkDependencyInfo dependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                      .imageMemoryBarrierCount = 1,
                                      .pImageMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(commands, &dependency);
}

} // namespace

core::Result<EnvironmentPrefilter> EnvironmentPrefilter::create(VkDevice device, GpuAllocator& allocator,
                                                                VkFormat cubeFormat) {
    EnvironmentPrefilter prefilter;
    prefilter.m_device = device;
    prefilter.m_allocator = &allocator;

    const std::array bindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                     nullptr},
    };
    const VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    if (const VkResult result =
            vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &prefilter.m_layout);
        result != VK_SUCCESS) {
        return prefilterError("descriptor set layout", result);
    }
    const std::array poolSizes{VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
    const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              .maxSets = 1,
                                              .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
                                              .pPoolSizes = poolSizes.data()};
    if (const VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &prefilter.m_pool);
        result != VK_SUCCESS) {
        return prefilterError("descriptor pool", result);
    }
    const VkDescriptorSetAllocateInfo allocateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   .descriptorPool = prefilter.m_pool,
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = &prefilter.m_layout};
    if (const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &prefilter.m_set);
        result != VK_SUCCESS) {
        return prefilterError("descriptor set", result);
    }
    // Around the vertical axis the image wraps; it stops at the poles.
    const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter = VK_FILTER_LINEAR,
                                          .minFilter = VK_FILTER_LINEAR,
                                          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                          .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                          .maxLod = VK_LOD_CLAMP_NONE};
    if (const VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &prefilter.m_sampler);
        result != VK_SUCCESS) {
        return prefilterError("sampler", result);
    }
    core::Result<GpuBuffer> params = allocator.createMappedBuffer(
        "Prefilter parameters", sizeof(PrefilterParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    if (!params) {
        return std::unexpected(params.error());
    }
    prefilter.m_params = *params;

    const std::array setLayouts{prefilter.m_layout};
    core::Result<GraphicsPipeline> pipeline = GraphicsPipeline::create(
        device,
        RenderTargetDesc{
            .colorFormat = cubeFormat, .depthFormat = VK_FORMAT_UNDEFINED, .samples = VK_SAMPLE_COUNT_1_BIT},
        PipelineDesc{.name = "environment prefilter pipeline",
                     .vertexShader = prefilterVertexShader(),
                     .fragmentShader = prefilterFragmentShader(),
                     .setLayouts = setLayouts,
                     .cullMode = VK_CULL_MODE_NONE});
    if (!pipeline) {
        return std::unexpected(pipeline.error());
    }
    prefilter.m_pipeline = std::move(*pipeline);
    return prefilter;
}

EnvironmentPrefilter::~EnvironmentPrefilter() {
    destroy();
}

EnvironmentPrefilter::EnvironmentPrefilter(EnvironmentPrefilter&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_allocator(std::exchange(other.m_allocator, nullptr)),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)), m_set(std::exchange(other.m_set, VK_NULL_HANDLE)),
      m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)),
      m_params(std::exchange(other.m_params, GpuBuffer{})), m_pipeline(std::move(other.m_pipeline)) {}

EnvironmentPrefilter& EnvironmentPrefilter::operator=(EnvironmentPrefilter&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
        m_set = std::exchange(other.m_set, VK_NULL_HANDLE);
        m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
        m_params = std::exchange(other.m_params, GpuBuffer{});
        m_pipeline = std::move(other.m_pipeline);
    }
    return *this;
}

void EnvironmentPrefilter::destroy() noexcept {
    m_pipeline = GraphicsPipeline();
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    if (m_params.buffer != VK_NULL_HANDLE) {
        m_allocator->destroyBuffer(m_params);
        m_params = GpuBuffer{};
    }
    vkDestroySampler(m_device, m_sampler, nullptr);
    // Frees the set too.
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    m_device = VK_NULL_HANDLE;
}

void EnvironmentPrefilter::record(VkCommandBuffer commands, VkImageView source, VkImage cube,
                                  std::span<const VkImageView> faceViews, const PrefilterParams& params) {
    APEX_ASSERT(faceViews.size() == std::size_t{params.levels} * kCubeFaces, "One view per level and face");
    std::memcpy(m_params.mapped, &params, sizeof(params));
    m_allocator->flush(m_params, sizeof(params));
    const VkDescriptorImageInfo image{m_sampler, source, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorBufferInfo buffer{m_params.buffer, 0, sizeof(PrefilterParams)};
    const std::array writes{
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = m_set,
                             .dstBinding = 0,
                             .descriptorCount = 1,
                             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             .pImageInfo = &image},
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = m_set,
                             .dstBinding = 1,
                             .descriptorCount = 1,
                             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             .pBufferInfo = &buffer},
    };
    vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);

    cubeBarrier(commands, cube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    for (std::uint32_t level = 0; level < params.levels; ++level) {
        const std::uint32_t size = mipSize(params.faceSize, level);
        const VkViewport viewport{.x = 0.0f,
                                  .y = 0.0f,
                                  .width = static_cast<float>(size),
                                  .height = static_cast<float>(size),
                                  .minDepth = 0.0f,
                                  .maxDepth = 1.0f};
        const VkRect2D scissor{{0, 0}, {size, size}};
        for (std::uint32_t face = 0; face < kCubeFaces; ++face) {
            const std::uint32_t draw = level * kCubeFaces + face;
            const VkRenderingAttachmentInfo attachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                       .imageView = faceViews[draw],
                                                       .imageLayout =
                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                       .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                       .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
            const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                            .renderArea = scissor,
                                            .layerCount = 1,
                                            .colorAttachmentCount = 1,
                                            .pColorAttachments = &attachment};
            vkCmdBeginRendering(commands, &rendering);
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.handle());
            vkCmdSetViewport(commands, 0, 1, &viewport);
            vkCmdSetScissor(commands, 0, 1, &scissor);
            vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.layout(), 0, 1,
                                    &m_set, 0, nullptr);
            // The shader finds the level and the face from the instance index.
            vkCmdDraw(commands, 3, 1, 0, draw);
            vkCmdEndRendering(commands);
        }
    }
    cubeBarrier(commands, cube, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

} // namespace apex::graphics
