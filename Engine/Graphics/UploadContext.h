#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace apex::graphics {

struct BufferCopy {
    VkBuffer destination;
    std::span<const std::byte> data;
};

// Tightly packed texels of the first level of a 2D image, one layer.
struct ImageCopy {
    VkImage destination;
    std::uint32_t width;
    std::uint32_t height;
    std::span<const std::byte> data;
    // The image's levels: the levels after the first are made from it by linear blits. The image needs
    // TRANSFER_SRC usage then, and a format that can be blitted with linear filtering.
    std::uint32_t mipLevels = 1;
};

// Internal to Graphics. Fills device-local buffers and images through host-visible staging buffers, on
// the graphics queue, and waits for the copy: meant for loading time, not for every frame.
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
    // The image goes from UNDEFINED to SHADER_READ_ONLY_OPTIMAL, every level, ready to be sampled.
    [[nodiscard]] core::Result<void> uploadImage(GpuAllocator& allocator, std::string_view name,
                                                 const ImageCopy& copy);
    // Records `record` into a one-time command buffer, submits it and waits: GPU work done once at loading
    // time, such as prefiltering an environment. The name appears in the error message.
    [[nodiscard]] core::Result<void> run(std::string_view name,
                                         const std::function<void(VkCommandBuffer)>& record);

private:
    void destroy() noexcept;
    [[nodiscard]] core::Result<void> begin(std::string_view name);
    [[nodiscard]] core::Result<void> submitAndWait(std::string_view name);

    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_commands = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace apex::graphics
