#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace apex::graphics {

struct BufferCopy {
    VkBuffer destination;
    std::span<const std::byte> data;
};

// Internal to Graphics. Fills device-local buffers through host-visible staging buffers, on the graphics
// queue, and waits for the copy: meant for loading time, not for every frame.
class UploadContext {
public:
    [[nodiscard]] static core::Result<UploadContext> create(VkDevice device, std::uint32_t queueFamily,
                                                            VkQueue queue);

    UploadContext() = default;
    ~UploadContext();
    UploadContext(UploadContext&& other) noexcept;
    UploadContext& operator=(UploadContext&& other) noexcept;
    UploadContext(const UploadContext&) = delete;
    UploadContext& operator=(const UploadContext&) = delete;

    // Returns once the GPU has finished the copies; later submissions see the data. The name appears in
    // the error message.
    [[nodiscard]] core::Result<void> upload(GpuAllocator& allocator, std::string_view name,
                                            std::span<const BufferCopy> copies);

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_commands = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace apex::graphics
