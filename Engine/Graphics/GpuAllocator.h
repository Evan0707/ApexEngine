#pragma once

#include "Core/Error.h"
#include "Graphics/Device.h"
#include "Graphics/GpuMemoryStats.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

namespace apex::graphics {

struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VkDeviceSize size = 0;
};

// Internal to Graphics. The only user of VMA (decision 0007). Must outlive every buffer it created
// and be destroyed before the device.
class GpuAllocator {
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    [[nodiscard]] static core::Result<std::unique_ptr<GpuAllocator>> create(const Device& device);

    GpuAllocator(ConstructionKey key, VmaAllocator allocator) noexcept;
    ~GpuAllocator();
    GpuAllocator(const GpuAllocator&) = delete;
    GpuAllocator& operator=(const GpuAllocator&) = delete;

    // Host-visible buffer written once at creation: fine for small static data. The name appears in
    // the error message (spec 5.4) and in VMA's statistics.
    [[nodiscard]] core::Result<GpuBuffer>
    createBufferWithData(std::string_view name, VkBufferUsageFlags usage, std::span<const std::byte> data);
    // Immediate: the caller makes sure no frame in flight still uses the buffer (DeferredDeleter).
    void destroyBuffer(const GpuBuffer& buffer) noexcept;

    [[nodiscard]] GpuMemoryStats memoryStats() const;

private:
    VmaAllocator m_allocator;
};

} // namespace apex::graphics
