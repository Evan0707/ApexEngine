#pragma once

#include "Core/Error.h"
#include "Graphics/RenderTargetDesc.h"

#include <vulkan/vulkan.h>

namespace apex::graphics {

// Internal to Graphics. The pipeline of the main pass: vertex format of resources::Vertex, descriptor
// set 0 of the frame (FrameBuffers), no push constant (G-02), back faces culled, counter-clockwise front
// faces (spec 3.3), depth test "less", dynamic viewport and scissor, dynamic rendering into the given
// target (G-09).
class ScenePipeline {
public:
    [[nodiscard]] static core::Result<ScenePipeline> create(VkDevice device, const RenderTargetDesc& target,
                                                            VkDescriptorSetLayout frameLayout);

    ScenePipeline() = default;
    ~ScenePipeline();
    ScenePipeline(ScenePipeline&& other) noexcept;
    ScenePipeline& operator=(ScenePipeline&& other) noexcept;
    ScenePipeline(const ScenePipeline&) = delete;
    ScenePipeline& operator=(const ScenePipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept;
    [[nodiscard]] VkPipelineLayout layout() const noexcept;
    [[nodiscard]] const RenderTargetDesc& target() const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    RenderTargetDesc m_target;
};

} // namespace apex::graphics
