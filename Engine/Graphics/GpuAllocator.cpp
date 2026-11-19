#include "Graphics/GpuAllocator.h"

#include "Core/Assert.h"

#include <array>
#include <format>
#include <string>

namespace apex::graphics {

core::Result<std::unique_ptr<GpuAllocator>> GpuAllocator::create(const Device& device) {
    VmaAllocatorCreateInfo info{};
    info.flags = device.memoryBudgetEnabled() ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0;
    info.physicalDevice = device.physicalDevice();
    info.device = device.device();
    info.instance = device.instance();
    info.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaAllocator allocator = nullptr;
    if (const VkResult result = vmaCreateAllocator(&info, &allocator); result != VK_SUCCESS) {
        return core::makeError(
            core::ErrorCode::GpuFailure,
            std::format("Cannot create the GPU memory allocator: VkResult {}", static_cast<int>(result)));
    }
    return std::make_unique<GpuAllocator>(ConstructionKey{}, allocator);
}

GpuAllocator::GpuAllocator(ConstructionKey /*key*/, VmaAllocator allocator) noexcept
    : m_allocator(allocator) {}

GpuAllocator::~GpuAllocator() {
    vmaDestroyAllocator(m_allocator);
}

core::Result<GpuBuffer> GpuAllocator::createBufferWithData(std::string_view name, VkBufferUsageFlags usage,
                                                           std::span<const std::byte> data) {
    APEX_ASSERT(!data.empty(), "A buffer cannot be empty");
    const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                        .size = data.size_bytes(),
                                        .usage = usage,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    GpuBuffer buffer;
    buffer.size = data.size_bytes();
    if (const VkResult result = vmaCreateBuffer(m_allocator, &bufferInfo, &allocationInfo, &buffer.buffer,
                                                &buffer.allocation, nullptr);
        result != VK_SUCCESS) {
        return core::makeError(core::ErrorCode::GpuFailure,
                               std::format("Cannot create the GPU buffer '{}' ({} bytes): VkResult {}", name,
                                           data.size_bytes(), static_cast<int>(result)));
    }
    const std::string ownedName(name);
    vmaSetAllocationName(m_allocator, buffer.allocation, ownedName.c_str());

    // Maps, copies and flushes when the memory is not coherent.
    if (const VkResult result =
            vmaCopyMemoryToAllocation(m_allocator, data.data(), buffer.allocation, 0, data.size_bytes());
        result != VK_SUCCESS) {
        destroyBuffer(buffer);
        return core::makeError(
            core::ErrorCode::GpuFailure,
            std::format("Cannot fill the GPU buffer '{}': VkResult {}", name, static_cast<int>(result)));
    }
    return buffer;
}

core::Result<GpuBuffer> GpuAllocator::createDeviceBuffer(std::string_view name, VkDeviceSize size,
                                                         VkBufferUsageFlags usage) {
    APEX_ASSERT(size > 0, "A buffer cannot be empty");
    const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                        .size = size,
                                        .usage = usage,
                                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    GpuBuffer buffer;
    buffer.size = size;
    if (const VkResult result = vmaCreateBuffer(m_allocator, &bufferInfo, &allocationInfo, &buffer.buffer,
                                                &buffer.allocation, nullptr);
        result != VK_SUCCESS) {
        return core::makeError(core::ErrorCode::GpuFailure,
                               std::format("Cannot create the GPU buffer '{}' ({} bytes): VkResult {}", name,
                                           size, static_cast<int>(result)));
    }
    const std::string ownedName(name);
    vmaSetAllocationName(m_allocator, buffer.allocation, ownedName.c_str());
    return buffer;
}

void GpuAllocator::destroyBuffer(const GpuBuffer& buffer) noexcept {
    vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
}

core::Result<GpuImage> GpuAllocator::createImage(std::string_view name, const VkImageCreateInfo& info) {
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    // Render targets are large and recreated with the swapchain: their own memory block each.
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    GpuImage image;
    if (const VkResult result =
            vmaCreateImage(m_allocator, &info, &allocationInfo, &image.image, &image.allocation, nullptr);
        result != VK_SUCCESS) {
        return core::makeError(core::ErrorCode::GpuFailure,
                               std::format("Cannot create the GPU image '{}' ({}x{}): VkResult {}", name,
                                           info.extent.width, info.extent.height, static_cast<int>(result)));
    }
    const std::string ownedName(name);
    vmaSetAllocationName(m_allocator, image.allocation, ownedName.c_str());
    return image;
}

void GpuAllocator::destroyImage(const GpuImage& image) noexcept {
    vmaDestroyImage(m_allocator, image.image, image.allocation);
}

GpuMemoryStats GpuAllocator::memoryStats() const {
    const VkPhysicalDeviceMemoryProperties* properties = nullptr;
    vmaGetMemoryProperties(m_allocator, &properties);
    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
    vmaGetHeapBudgets(m_allocator, budgets.data());

    GpuMemoryStats stats;
    for (std::uint32_t heap = 0; heap < properties->memoryHeapCount; ++heap) {
        if ((properties->memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
            stats.usedBytes += budgets[heap].usage;
            stats.budgetBytes += budgets[heap].budget;
        }
    }
    return stats;
}

} // namespace apex::graphics
