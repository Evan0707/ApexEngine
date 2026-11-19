#pragma once

// The only public header of Graphics with Vulkan types. Included by Debug/ImGuiBackend.cpp only
// (decision 0009): the Vulkan backend of Dear ImGui needs the renderer's handles.

#include "Graphics/RenderOverlay.h"
#include "Graphics/Renderer.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace apex::graphics {

struct OverlayCommands {
    // In the recording state, inside the dynamic rendering pass of the swapchain image.
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
};

struct VulkanBackendInfo {
    std::uint32_t apiVersion = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    // Format of the swapchain, or the format it will most likely get when it does not exist yet.
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    // At least the number of frames in flight.
    std::uint32_t imageCount = 0;
};

[[nodiscard]] VulkanBackendInfo vulkanBackendInfo(const Renderer& renderer);

} // namespace apex::graphics
