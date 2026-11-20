#include "Graphics/FrameBuffers.h"

#include "Core/Assert.h"

#include <array>
#include <cstring>
#include <format>
#include <utility>

namespace apex::graphics {

namespace {

[[nodiscard]] std::unexpected<core::Error> frameError(std::string_view what, VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("Cannot create the per-frame {}: VkResult {}", what, static_cast<int>(result)));
}

void copyTo(GpuAllocator& allocator, const GpuBuffer& buffer, const void* data, std::size_t size) {
    APEX_ASSERT(size <= buffer.size, "Per-frame data larger than its buffer");
    if (size == 0) {
        return;
    }
    std::memcpy(buffer.mapped, data, size);
    allocator.flush(buffer, size);
}

} // namespace

core::Result<FrameBuffers> FrameBuffers::create(VkDevice device, GpuAllocator& allocator,
                                                const FrameBufferLimits& limits) {
    APEX_ASSERT(limits.maxObjects > 0 && limits.maxMaterials > 0, "Empty per-frame buffers");
    FrameBuffers buffers;
    buffers.m_device = device;
    buffers.m_allocator = &allocator;
    buffers.m_limits = limits;

    constexpr VkShaderStageFlags kBothStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const std::array bindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kBothStages, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kBothStages, nullptr},
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                     nullptr},
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    const VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    if (const VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &buffers.m_layout);
        result != VK_SUCCESS) {
        return frameError("descriptor set layout", result);
    }

    const std::array poolSizes{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, limits.framesInFlight},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * limits.framesInFlight},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, limits.framesInFlight},
    };
    const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              .maxSets = limits.framesInFlight,
                                              .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
                                              .pPoolSizes = poolSizes.data()};
    if (const VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &buffers.m_pool);
        result != VK_SUCCESS) {
        return frameError("descriptor pool", result);
    }

    for (std::uint32_t frame = 0; frame < limits.framesInFlight; ++frame) {
        PerFrame& perFrame = buffers.m_frames.emplace_back();
        const struct {
            GpuBuffer* buffer;
            const char* name;
            VkDeviceSize size;
            VkBufferUsageFlags usage;
        } requests[] = {
            {&perFrame.uniforms, "Frame uniforms", sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
            {&perFrame.objects, "Object data", sizeof(ObjectGpu) * VkDeviceSize{limits.maxObjects},
             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
            {&perFrame.materials, "Material table", sizeof(MaterialGpu) * VkDeviceSize{limits.maxMaterials},
             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
        };
        for (const auto& request : requests) {
            core::Result<GpuBuffer> created = allocator.createMappedBuffer(
                std::format("{} (frame {})", request.name, frame), request.size, request.usage);
            if (!created) {
                return std::unexpected(created.error());
            }
            *request.buffer = *created;
        }

        const VkDescriptorSetAllocateInfo allocateInfo{.sType =
                                                           VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                       .descriptorPool = buffers.m_pool,
                                                       .descriptorSetCount = 1,
                                                       .pSetLayouts = &buffers.m_layout};
        if (const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &perFrame.set);
            result != VK_SUCCESS) {
            return frameError("descriptor set", result);
        }
        const std::array infos{
            VkDescriptorBufferInfo{perFrame.uniforms.buffer, 0, VK_WHOLE_SIZE},
            VkDescriptorBufferInfo{perFrame.objects.buffer, 0, VK_WHOLE_SIZE},
            VkDescriptorBufferInfo{perFrame.materials.buffer, 0, VK_WHOLE_SIZE},
        };
        std::array<VkWriteDescriptorSet, 3> writes{};
        for (std::uint32_t binding = 0; binding < writes.size(); ++binding) {
            writes[binding] = VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                   .dstSet = perFrame.set,
                                                   .dstBinding = binding,
                                                   .descriptorCount = 1,
                                                   .descriptorType = bindings[binding].descriptorType,
                                                   .pBufferInfo = &infos[binding]};
        }
        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    return buffers;
}

FrameBuffers::~FrameBuffers() {
    destroy();
}

FrameBuffers::FrameBuffers(FrameBuffers&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_allocator(std::exchange(other.m_allocator, nullptr)), m_limits(other.m_limits),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)), m_frames(std::exchange(other.m_frames, {})) {}

FrameBuffers& FrameBuffers::operator=(FrameBuffers&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_limits = other.m_limits;
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
        m_frames = std::exchange(other.m_frames, {});
    }
    return *this;
}

void FrameBuffers::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    for (const PerFrame& frame : m_frames) {
        for (const GpuBuffer* buffer : {&frame.uniforms, &frame.objects, &frame.materials}) {
            if (buffer->buffer != VK_NULL_HANDLE) {
                m_allocator->destroyBuffer(*buffer);
            }
        }
    }
    m_frames.clear();
    // Frees the descriptor sets too.
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    m_device = VK_NULL_HANDLE;
}

VkDescriptorSetLayout FrameBuffers::layout() const noexcept {
    return m_layout;
}

VkDescriptorSet FrameBuffers::descriptorSet(std::uint32_t frame) const noexcept {
    return m_frames[frame].set;
}

void FrameBuffers::bindShadowMap(VkImageView view, VkSampler sampler) {
    const VkDescriptorImageInfo image{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    for (const PerFrame& frame : m_frames) {
        const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                         .dstSet = frame.set,
                                         .dstBinding = 3,
                                         .descriptorCount = 1,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         .pImageInfo = &image};
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
}

const FrameBufferLimits& FrameBuffers::limits() const noexcept {
    return m_limits;
}

void FrameBuffers::write(std::uint32_t frame, const FrameUniforms& uniforms,
                         std::span<const ObjectGpu> objects, std::span<const MaterialGpu> materials) {
    const PerFrame& perFrame = m_frames[frame];
    copyTo(*m_allocator, perFrame.uniforms, &uniforms, sizeof(uniforms));
    copyTo(*m_allocator, perFrame.objects, objects.data(), objects.size_bytes());
    copyTo(*m_allocator, perFrame.materials, materials.data(), materials.size_bytes());
}

} // namespace apex::graphics
