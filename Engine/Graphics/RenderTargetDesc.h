#pragma once

#include <vulkan/vulkan.h>

namespace apex::graphics {

// Internal to Graphics. What a pipeline must know about the target it draws into (G-09): passed at
// pipeline creation, never written as constants in the pipeline code.
struct RenderTargetDesc {
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    // VK_FORMAT_UNDEFINED: no depth buffer.
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    [[nodiscard]] bool operator==(const RenderTargetDesc&) const noexcept = default;
};

} // namespace apex::graphics
