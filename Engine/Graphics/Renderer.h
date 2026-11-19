#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Graphics/FrameCounters.h"
#include "Graphics/GpuMemoryStats.h"
#include "Graphics/RenderOverlay.h"
#include "Graphics/ValidationLog.h"
#include "Platform/Window.h"

#include <memory>
#include <string>

namespace apex::graphics {

struct VulkanBackendInfo;

struct RendererConfig {
    std::string applicationName = "Apex Engine";
    // Debug builds turn it on (decision 0006). Ignored, with a warning, when the layers are not installed.
    bool enableValidation = false;
    // Off for performance measurements only (spec 5.4, 7.1).
    bool vsync = true;
};

// The only entry point of Graphics for the application. No Vulkan type in this header (G-01, G-10).
class Renderer {
    struct State;
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    // The window, the logger and the validation log must outlive the renderer.
    [[nodiscard]] static core::Result<std::unique_ptr<Renderer>> create(platform::Window& window,
                                                                        core::Logger& logger,
                                                                        ValidationLog& validation,
                                                                        const RendererConfig& config);

    Renderer(ConstructionKey key, std::unique_ptr<State> state) noexcept;
    // Waits for the GPU, then destroys every GPU object before the device.
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Render phase: acquires a swapchain image, records and submits the frame.
    // Does nothing while the window is minimized (spec 5.2). An error means the device is unusable.
    [[nodiscard]] core::Result<void> renderFrame();
    // Presentation phase: presents the image of the last renderFrame, if there is one.
    [[nodiscard]] core::Result<void> presentFrame();

    [[nodiscard]] GpuMemoryStats memoryStats() const;
    // Counters of the last rendered frame. Unchanged by a skipped frame.
    [[nodiscard]] FrameCounters lastFrameCounters() const noexcept;

    // Drawn at the end of the main pass of each frame. nullptr removes it. Must outlive its registration.
    void setOverlay(RenderOverlay* overlay) noexcept;

    // Graphics/VulkanBackendAccess.h, for the ImGui backend of Debug only (decision 0009).
    friend VulkanBackendInfo vulkanBackendInfo(const Renderer& renderer);

private:
    std::unique_ptr<State> m_state;
};

} // namespace apex::graphics
