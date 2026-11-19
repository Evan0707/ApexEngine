#include "Graphics/UploadContext.h"

#include <format>
#include <limits>
#include <utility>
#include <vector>

namespace apex::graphics {

namespace {

[[nodiscard]] std::unexpected<core::Error> uploadError(std::string_view what, std::string_view name,
                                                       VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("{} failed while uploading '{}': VkResult {}", what, name, static_cast<int>(result)));
}

// Destroys the staging buffers when the upload returns, whatever the outcome.
class StagingBuffers {
public:
    explicit StagingBuffers(GpuAllocator& allocator) noexcept : m_allocator(&allocator) {}
    ~StagingBuffers() {
        for (const GpuBuffer& buffer : m_buffers) {
            m_allocator->destroyBuffer(buffer);
        }
    }
    StagingBuffers(const StagingBuffers&) = delete;
    StagingBuffers& operator=(const StagingBuffers&) = delete;

    void add(const GpuBuffer& buffer) { m_buffers.push_back(buffer); }
    [[nodiscard]] const GpuBuffer& operator[](std::size_t index) const noexcept { return m_buffers[index]; }

private:
    GpuAllocator* m_allocator;
    std::vector<GpuBuffer> m_buffers;
};

} // namespace

core::Result<UploadContext> UploadContext::create(VkDevice device, std::uint32_t queueFamily, VkQueue queue) {
    UploadContext context;
    context.m_device = device;
    context.m_queue = queue;

    const VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                           .queueFamilyIndex = queueFamily};
    if (const VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &context.m_pool);
        result != VK_SUCCESS) {
        return uploadError("vkCreateCommandPool", "upload context", result);
    }
    const VkCommandBufferAllocateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                 .commandPool = context.m_pool,
                                                 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                 .commandBufferCount = 1};
    if (const VkResult result = vkAllocateCommandBuffers(device, &bufferInfo, &context.m_commands);
        result != VK_SUCCESS) {
        return uploadError("vkAllocateCommandBuffers", "upload context", result);
    }
    const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (const VkResult result = vkCreateFence(device, &fenceInfo, nullptr, &context.m_fence);
        result != VK_SUCCESS) {
        return uploadError("vkCreateFence", "upload context", result);
    }
    return context;
}

UploadContext::~UploadContext() {
    destroy();
}

UploadContext::UploadContext(UploadContext&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_queue(std::exchange(other.m_queue, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)),
      m_commands(std::exchange(other.m_commands, VK_NULL_HANDLE)),
      m_fence(std::exchange(other.m_fence, VK_NULL_HANDLE)) {}

UploadContext& UploadContext::operator=(UploadContext&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_queue = std::exchange(other.m_queue, VK_NULL_HANDLE);
        m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
        m_commands = std::exchange(other.m_commands, VK_NULL_HANDLE);
        m_fence = std::exchange(other.m_fence, VK_NULL_HANDLE);
    }
    return *this;
}

void UploadContext::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyFence(m_device, m_fence, nullptr);
    // Frees the command buffer too.
    vkDestroyCommandPool(m_device, m_pool, nullptr);
    m_device = VK_NULL_HANDLE;
}

core::Result<void> UploadContext::begin(std::string_view name) {
    if (const VkResult result = vkResetCommandPool(m_device, m_pool, 0); result != VK_SUCCESS) {
        return uploadError("vkResetCommandPool", name, result);
    }
    const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    if (const VkResult result = vkBeginCommandBuffer(m_commands, &beginInfo); result != VK_SUCCESS) {
        return uploadError("vkBeginCommandBuffer", name, result);
    }
    return {};
}

core::Result<void> UploadContext::submitAndWait(std::string_view name) {
    if (const VkResult result = vkEndCommandBuffer(m_commands); result != VK_SUCCESS) {
        return uploadError("vkEndCommandBuffer", name, result);
    }
    const VkCommandBufferSubmitInfo commandInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                .commandBuffer = m_commands};
    const VkSubmitInfo2 submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                               .commandBufferInfoCount = 1,
                               .pCommandBufferInfos = &commandInfo};
    if (const VkResult result = vkResetFences(m_device, 1, &m_fence); result != VK_SUCCESS) {
        return uploadError("vkResetFences", name, result);
    }
    if (const VkResult result = vkQueueSubmit2(m_queue, 1, &submit, m_fence); result != VK_SUCCESS) {
        return uploadError("vkQueueSubmit2", name, result);
    }
    // The staging buffers are destroyed on return: the copy must be over.
    if (const VkResult result =
            vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        result != VK_SUCCESS) {
        return uploadError("vkWaitForFences", name, result);
    }
    return {};
}

core::Result<void> UploadContext::upload(GpuAllocator& allocator, std::string_view name,
                                         std::span<const BufferCopy> copies) {
    StagingBuffers staging(allocator);
    for (const BufferCopy& copy : copies) {
        core::Result<GpuBuffer> buffer = allocator.createBufferWithData(
            std::format("{} (staging)", name), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, copy.data);
        if (!buffer) {
            return std::unexpected(buffer.error());
        }
        staging.add(*buffer);
    }
    if (core::Result<void> begun = begin(name); !begun) {
        return begun;
    }
    for (std::size_t i = 0; i < copies.size(); ++i) {
        const VkBufferCopy region{.size = copies[i].data.size_bytes()};
        vkCmdCopyBuffer(m_commands, staging[i].buffer, copies[i].destination, 1, &region);
    }
    // The second scope reaches later submissions: frames that read these buffers need no barrier of their
    // own.
    const VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                   .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                   .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                   .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                   .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT};
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(m_commands, &dependency);
    return submitAndWait(name);
}

core::Result<void> UploadContext::uploadImage(GpuAllocator& allocator, std::string_view name,
                                              const ImageCopy& copy) {
    StagingBuffers staging(allocator);
    core::Result<GpuBuffer> buffer = allocator.createBufferWithData(
        std::format("{} (staging)", name), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, copy.data);
    if (!buffer) {
        return std::unexpected(buffer.error());
    }
    staging.add(*buffer);
    if (core::Result<void> begun = begin(name); !begun) {
        return begun;
    }

    const VkImageSubresourceRange whole{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    const VkImageMemoryBarrier2 toTransfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                           .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                           .srcAccessMask = VK_ACCESS_2_NONE,
                                           .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                           .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                           .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                           .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                           .image = copy.destination,
                                           .subresourceRange = whole};
    const VkDependencyInfo beforeCopy{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                      .imageMemoryBarrierCount = 1,
                                      .pImageMemoryBarriers = &toTransfer};
    vkCmdPipelineBarrier2(m_commands, &beforeCopy);

    const VkBufferImageCopy region{.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                   .imageExtent = {copy.width, copy.height, 1}};
    vkCmdCopyBufferToImage(m_commands, staging[0].buffer, copy.destination,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Ready to be sampled by later frames: their fragment shaders wait for the copy.
    const VkImageMemoryBarrier2 toShader{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                         .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                         .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                         .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                         .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                         .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .image = copy.destination,
                                         .subresourceRange = whole};
    const VkDependencyInfo afterCopy{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                     .imageMemoryBarrierCount = 1,
                                     .pImageMemoryBarriers = &toShader};
    vkCmdPipelineBarrier2(m_commands, &afterCopy);
    return submitAndWait(name);
}

} // namespace apex::graphics
