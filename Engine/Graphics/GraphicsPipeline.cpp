#include "Graphics/GraphicsPipeline.h"

#include "Resources/MeshData.h"

#include <array>
#include <cstddef>
#include <format>
#include <utility>
#include <vector>

namespace apex::graphics {

namespace {

[[nodiscard]] std::unexpected<core::Error> pipelineError(std::string_view pipeline, std::string_view what,
                                                         VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("Cannot create the {} {}: VkResult {}", pipeline, what, static_cast<int>(result)));
}

// Destroys the module at the end of pipeline creation: a pipeline does not need it afterwards.
class ShaderModule {
public:
    ShaderModule() = default;
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;
    ~ShaderModule() {
        if (m_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_module, nullptr);
        }
    }

    [[nodiscard]] VkResult create(VkDevice device, std::span<const std::uint32_t> code) {
        m_device = device;
        const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .codeSize = code.size_bytes(),
                                            .pCode = code.data()};
        return vkCreateShaderModule(device, &info, nullptr, &m_module);
    }

    [[nodiscard]] VkShaderModule handle() const noexcept { return m_module; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkShaderModule m_module = VK_NULL_HANDLE;
};

struct AttributeLayout {
    MeshAttribute bit;
    std::uint32_t location;
    VkFormat format;
    std::uint32_t offset;
};

constexpr std::array kMeshAttributes{
    AttributeLayout{kMeshPosition, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(resources::Vertex, position)},
    AttributeLayout{kMeshNormal, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(resources::Vertex, normal)},
    AttributeLayout{kMeshTangent, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(resources::Vertex, tangent)},
    AttributeLayout{kMeshUv, 3, VK_FORMAT_R32G32_SFLOAT, offsetof(resources::Vertex, uv)},
};

} // namespace

core::Result<GraphicsPipeline> GraphicsPipeline::create(VkDevice device, const RenderTargetDesc& target,
                                                        const PipelineDesc& desc) {
    GraphicsPipeline pipeline;
    pipeline.m_device = device;
    pipeline.m_target = target;

    ShaderModule vertexModule;
    if (const VkResult result = vertexModule.create(device, desc.vertexShader); result != VK_SUCCESS) {
        return pipelineError(desc.name, "vertex shader module", result);
    }
    ShaderModule fragmentModule;
    if (!desc.fragmentShader.empty()) {
        if (const VkResult result = fragmentModule.create(device, desc.fragmentShader);
            result != VK_SUCCESS) {
            return pipelineError(desc.name, "fragment shader module", result);
        }
    }

    // No push constant: per-object data comes from the per-frame buffer (G-02).
    const VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount =
                                                    static_cast<std::uint32_t>(desc.setLayouts.size()),
                                                .pSetLayouts = desc.setLayouts.data()};
    if (const VkResult result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipeline.m_layout);
        result != VK_SUCCESS) {
        return pipelineError(desc.name, "pipeline layout", result);
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages{
        VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                        .module = vertexModule.handle(),
                                        .pName = "main"}};
    if (fragmentModule.handle() != VK_NULL_HANDLE) {
        stages.push_back(
            VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                            .module = fragmentModule.handle(),
                                            .pName = "main"});
    }

    const VkVertexInputBindingDescription binding{
        .binding = 0, .stride = sizeof(resources::Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    std::vector<VkVertexInputAttributeDescription> attributes;
    for (const AttributeLayout& attribute : kMeshAttributes) {
        if ((desc.meshAttributes & attribute.bit) != 0) {
            attributes.push_back(VkVertexInputAttributeDescription{.location = attribute.location,
                                                                   .binding = 0,
                                                                   .format = attribute.format,
                                                                   .offset = attribute.offset});
        }
    }
    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = attributes.empty() ? 0u : 1u,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
        .pVertexAttributeDescriptions = attributes.data(),
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    const VkPipelineViewportStateCreateInfo viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1};
    // Front faces are counter-clockwise, as in glTF (spec 3.3); the projections keep them so.
    const VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = desc.cullMode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = desc.dynamicDepthBias ? VK_TRUE : VK_FALSE,
        .lineWidth = 1.0f};
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = target.samples};
    const bool hasDepth = target.depthFormat != VK_FORMAT_UNDEFINED;
    const VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = hasDepth ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = hasDepth && desc.depthWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp = desc.depthCompare};
    const VkPipelineColorBlendAttachmentState blendAttachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT};
    const bool hasColor = target.colorFormat != VK_FORMAT_UNDEFINED;
    const VkPipelineColorBlendStateCreateInfo blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = hasColor ? 1u : 0u,
        .pAttachments = &blendAttachment};
    std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    if (desc.dynamicDepthBias) {
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    }
    const VkPipelineDynamicStateCreateInfo dynamic{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};
    const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                  .colorAttachmentCount = hasColor ? 1u : 0u,
                                                  .pColorAttachmentFormats = &target.colorFormat,
                                                  .depthAttachmentFormat = target.depthFormat};

    const VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = static_cast<std::uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = pipeline.m_layout,
    };
    if (const VkResult result =
            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline.m_pipeline);
        result != VK_SUCCESS) {
        return pipelineError(desc.name, "graphics pipeline", result);
    }
    return pipeline;
}

GraphicsPipeline::~GraphicsPipeline() {
    destroy();
}

GraphicsPipeline::GraphicsPipeline(GraphicsPipeline&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE)), m_target(other.m_target) {}

GraphicsPipeline& GraphicsPipeline::operator=(GraphicsPipeline&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
        m_target = other.m_target;
    }
    return *this;
}

void GraphicsPipeline::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
}

VkPipeline GraphicsPipeline::handle() const noexcept {
    return m_pipeline;
}

VkPipelineLayout GraphicsPipeline::layout() const noexcept {
    return m_layout;
}

const RenderTargetDesc& GraphicsPipeline::target() const noexcept {
    return m_target;
}

} // namespace apex::graphics
