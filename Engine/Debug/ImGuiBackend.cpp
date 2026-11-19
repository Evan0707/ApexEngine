#include "Debug/ImGuiBackend.h"

#include "Graphics/VulkanBackendAccess.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include <format>
#include <utility>

namespace apex::debug {

namespace {

[[nodiscard]] ImGui_ImplVulkan_PipelineInfo pipelineInfo(const VkFormat& colorFormat) {
    ImGui_ImplVulkan_PipelineInfo info{};
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
    };
    return info;
}

} // namespace

core::Result<std::unique_ptr<ImGuiBackend>> ImGuiBackend::create(platform::Window& window,
                                                                 graphics::Renderer& renderer) {
    if (!ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(window.nativeHandle()))) {
        return core::makeError(core::ErrorCode::PlatformFailure,
                               "Cannot initialize the SDL3 backend of Dear ImGui");
    }

    const graphics::VulkanBackendInfo vulkan = graphics::vulkanBackendInfo(renderer);
    const VkFormat colorFormat = vulkan.colorFormat;
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = vulkan.apiVersion;
    info.Instance = vulkan.instance;
    info.PhysicalDevice = vulkan.physicalDevice;
    info.Device = vulkan.device;
    info.QueueFamily = vulkan.queueFamily;
    info.Queue = vulkan.queue;
    // The backend creates its own pool, sized for its font atlas.
    info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    info.MinImageCount = 2;
    info.ImageCount = vulkan.imageCount;
    info.UseDynamicRendering = true;
    info.PipelineInfoMain = pipelineInfo(colorFormat);
    if (!ImGui_ImplVulkan_Init(&info)) {
        ImGui_ImplSDL3_Shutdown();
        return core::makeError(core::ErrorCode::GpuFailure,
                               "Cannot initialize the Vulkan backend of Dear ImGui");
    }

    auto backend =
        std::make_unique<ImGuiBackend>(ConstructionKey{}, window, static_cast<std::int32_t>(colorFormat));
    backend->m_renderer = &renderer;
    // ImGui sees the raw SDL events: mouse position, wheel, text input (decision 0009).
    window.setNativeEventObserver(
        [](const void* event) { ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(event)); });
    return backend;
}

ImGuiBackend::ImGuiBackend(ConstructionKey /*key*/, platform::Window& window,
                           std::int32_t colorFormat) noexcept
    : m_window(&window), m_colorFormat(colorFormat) {}

ImGuiBackend::~ImGuiBackend() {
    m_window->setNativeEventObserver({});
    if (m_renderer != nullptr) {
        vkDeviceWaitIdle(graphics::vulkanBackendInfo(*m_renderer).device);
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void ImGuiBackend::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void ImGuiBackend::render(const graphics::OverlayCommands& commands) {
    if (static_cast<std::int32_t>(commands.colorFormat) != m_colorFormat) {
        // Only after a swapchain recreation, which waited for the GPU: the old pipeline is no longer in use.
        const ImGui_ImplVulkan_PipelineInfo info = pipelineInfo(commands.colorFormat);
        ImGui_ImplVulkan_CreateMainPipeline(&info);
        m_colorFormat = static_cast<std::int32_t>(commands.colorFormat);
    }
    if (ImDrawData* drawData = ImGui::GetDrawData(); drawData != nullptr) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commands.commandBuffer);
    }
}

} // namespace apex::debug
