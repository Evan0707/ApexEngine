#pragma once

#include "Core/Error.h"
#include "Graphics/FrameData.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/MaterialTable.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>
#include <vector>

namespace apex::graphics {

struct FrameBufferLimits {
    std::uint32_t framesInFlight = 2;
    std::uint32_t maxObjects = 0;
    std::uint32_t maxMaterials = 0;
};

// Internal to Graphics. Descriptor set 0 of each frame in flight (spec 5.4, decision 0011): frame
// uniforms, per-object storage buffer, material table. Host-visible buffers, mapped for the whole run
// and rewritten each frame. The allocator must outlive it.
class FrameBuffers {
public:
    [[nodiscard]] static core::Result<FrameBuffers> create(VkDevice device, GpuAllocator& allocator,
                                                           const FrameBufferLimits& limits);

    FrameBuffers() = default;
    ~FrameBuffers();
    FrameBuffers(FrameBuffers&& other) noexcept;
    FrameBuffers& operator=(FrameBuffers&& other) noexcept;
    FrameBuffers(const FrameBuffers&) = delete;
    FrameBuffers& operator=(const FrameBuffers&) = delete;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;
    [[nodiscard]] VkDescriptorSet descriptorSet(std::uint32_t frame) const noexcept;
    [[nodiscard]] const FrameBufferLimits& limits() const noexcept;

    // The frame's fence must have been waited for. objects and materials must fit the limits.
    void write(std::uint32_t frame, const FrameUniforms& uniforms, std::span<const ObjectGpu> objects,
               std::span<const MaterialGpu> materials);

private:
    struct PerFrame {
        GpuBuffer uniforms;
        GpuBuffer objects;
        GpuBuffer materials;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    GpuAllocator* m_allocator = nullptr;
    FrameBufferLimits m_limits;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::vector<PerFrame> m_frames;
};

} // namespace apex::graphics
