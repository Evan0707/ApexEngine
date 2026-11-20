#pragma once

#include "Core/Error.h"
#include "Graphics/RenderTargetDesc.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace apex::graphics {

// Vertex attributes a pipeline reads from resources::Vertex, by shader location. Only those the shaders
// use: an attribute the vertex shader does not consume is a performance warning of the validation layers.
enum MeshAttribute : std::uint32_t {
    kMeshPosition = 1u << 0,
    kMeshNormal = 1u << 1,
    kMeshTangent = 1u << 2,
    kMeshUv = 1u << 3,
};

// What differs between the pipelines of the renderer. No push constant in any of them (G-02).
struct PipelineDesc {
    // For error messages: "scene", "shadow"...
    std::string_view name;
    std::span<const std::uint32_t> vertexShader;
    // Empty: no fragment stage (depth only).
    std::span<const std::uint32_t> fragmentShader;
    std::span<const VkDescriptorSetLayout> setLayouts;
    // MeshAttribute bits. 0: no vertex buffer (a full-screen triangle made by the vertex shader).
    std::uint32_t meshAttributes = 0;
    // Instead of mesh attributes: LineVertex, position at location 0 and color at location 1.
    bool lineVertices = false;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    // Used only when the target has a depth format.
    bool depthWrite = true;
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;
    // Constant and slope factors set by vkCmdSetDepthBias before drawing.
    bool dynamicDepthBias = false;
};

// Internal to Graphics. A graphics pipeline with its layout, for dynamic rendering into the given target
// (G-09): counter-clockwise front faces (spec 3.3), dynamic viewport and scissor, depth test when the target
// has a depth format, no blending.
class GraphicsPipeline {
public:
    [[nodiscard]] static core::Result<GraphicsPipeline>
    create(VkDevice device, const RenderTargetDesc& target, const PipelineDesc& desc);

    GraphicsPipeline() = default;
    ~GraphicsPipeline();
    GraphicsPipeline(GraphicsPipeline&& other) noexcept;
    GraphicsPipeline& operator=(GraphicsPipeline&& other) noexcept;
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

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
