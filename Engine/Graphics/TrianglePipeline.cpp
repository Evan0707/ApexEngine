#include "Graphics/TrianglePipeline.h"

#include "Graphics/ShaderBinaries.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>
#include <utility>

namespace apex::graphics {

namespace {

[[nodiscard]] std::unexpected<core::Error> pipelineError(std::string_view what, VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("Cannot create the triangle {}: VkResult {}", what, static_cast<int>(result)));
}

// Destroys the module at the end of pipeline creation: a pipeline does not need it afterwards.
class ShaderModule {
public:
    ShaderModule(VkDevice device, VkShaderModule module) noexcept : m_device(device), m_module(module) {}
    ~ShaderModule() { vkDestroyShaderModule(m_device, m_module, nullptr); }
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    [[nodiscard]] VkShaderModule handle() const noexcept { return m_module; }

private:
    VkDevice m_device;
    VkShaderModule m_module;
};

[[nodiscard]] VkResult createModule(VkDevice device, std::span<const std::uint32_t> code,
                                    VkShaderModule& module) {
    const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                        .codeSize = code.size_bytes(),
                                        .pCode = code.data()};
    return vkCreateShaderModule(device, &info, nullptr, &module);
}

} // namespace

core::Result<TrianglePipeline> TrianglePipeline::create(VkDevice device, const RenderTargetDesc& target) {
    TrianglePipeline pipeline;
    pipeline.m_device = device;
    pipeline.m_target = target;

    VkShaderModule vertexHandle = VK_NULL_HANDLE;
    if (const VkResult result = createModule(device, triangleVertexShader(), vertexHandle);
        result != VK_SUCCESS) {
        return pipelineError("vertex shader module", result);
    }
    const ShaderModule vertexModule(device, vertexHandle);
    VkShaderModule fragmentHandle = VK_NULL_HANDLE;
    if (const VkResult result = createModule(device, triangleFragmentShader(), fragmentHandle);
        result != VK_SUCCESS) {
        return pipelineError("fragment shader module", result);
    }
    const ShaderModule fragmentModule(device, fragmentHandle);

    // No descriptor and no push constant: per-object data will come from a per-frame buffer (G-02).
    const VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (const VkResult result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipeline.m_layout);
        result != VK_SUCCESS) {
        return pipelineError("pipeline layout", result);
    }

    const std::array stages{
        VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                        .module = vertexModule.handle(),
                                        .pName = "main"},
        VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                        .module = fragmentModule.handle(),
                                        .pName = "main"},
    };

    const VkVertexInputBindingDescription binding{
        .binding = 0, .stride = sizeof(TriangleVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    const std::array attributes{
        VkVertexInputAttributeDescription{.location = 0,
                                          .binding = 0,
                                          .format = VK_FORMAT_R32G32B32_SFLOAT,
                                          .offset = offsetof(TriangleVertex, position)},
        VkVertexInputAttributeDescription{.location = 1,
                                          .binding = 0,
                                          .format = VK_FORMAT_R32G32B32_SFLOAT,
                                          .offset = offsetof(TriangleVertex, color)},
    };
    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
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
    // No culling before the camera of phase 3. Front faces are counter-clockwise, as in glTF (spec 3.3).
    const VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f};
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = target.samples};
    const VkPipelineColorBlendAttachmentState blendAttachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT};
    const VkPipelineColorBlendStateCreateInfo blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment};
    const std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};
    const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                  .colorAttachmentCount = 1,
                                                  .pColorAttachmentFormats = &target.colorFormat};

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
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = pipeline.m_layout,
    };
    if (const VkResult result =
            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline.m_pipeline);
        result != VK_SUCCESS) {
        return pipelineError("graphics pipeline", result);
    }
    return pipeline;
}

TrianglePipeline::~TrianglePipeline() {
    destroy();
}

TrianglePipeline::TrianglePipeline(TrianglePipeline&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE)), m_target(other.m_target) {}

TrianglePipeline& TrianglePipeline::operator=(TrianglePipeline&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
        m_target = other.m_target;
    }
    return *this;
}

void TrianglePipeline::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
}

VkPipeline TrianglePipeline::handle() const noexcept {
    return m_pipeline;
}

const RenderTargetDesc& TrianglePipeline::target() const noexcept {
    return m_target;
}

} // namespace apex::graphics
