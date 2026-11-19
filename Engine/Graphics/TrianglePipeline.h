#pragma once

#include "Core/Error.h"
#include "Graphics/RenderTargetDesc.h"

#include <vulkan/vulkan.h>

#include <array>

namespace apex::graphics {

// Phase 2 vertex: position already in clip space, linear color. Replaced by the mesh format in phase 3.
struct TriangleVertex {
    std::array<float, 3> position;
    std::array<float, 3> color;
};

// Internal to Graphics. The pipeline of the phase 2 triangle: no descriptor, no push constant,
// dynamic viewport and scissor, dynamic rendering into the given target (G-09).
class TrianglePipeline {
public:
    [[nodiscard]] static core::Result<TrianglePipeline> create(VkDevice device,
                                                               const RenderTargetDesc& target);

    TrianglePipeline() = default;
    ~TrianglePipeline();
    TrianglePipeline(TrianglePipeline&& other) noexcept;
    TrianglePipeline& operator=(TrianglePipeline&& other) noexcept;
    TrianglePipeline(const TrianglePipeline&) = delete;
    TrianglePipeline& operator=(const TrianglePipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept;
    [[nodiscard]] const RenderTargetDesc& target() const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    RenderTargetDesc m_target;
};

} // namespace apex::graphics
