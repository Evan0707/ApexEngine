#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Graphics/FrameCounters.h"
#include "Graphics/GpuMemoryStats.h"
#include "Graphics/RenderOverlay.h"
#include "Graphics/RenderScene.h"
#include "Graphics/ValidationLog.h"
#include "Platform/Window.h"

#include <cstdint>
#include <memory>
#include <string>

namespace apex::resources {
class ResourceCache;
} // namespace apex::resources

namespace apex::graphics {

struct VulkanBackendInfo;

struct RendererConfig {
    std::string applicationName = "Apex Engine";
    // Debug builds turn it on (decision 0006). Ignored, with a warning, when the layers are not installed.
    bool enableValidation = false;
    // Off for performance measurements only (spec 5.4, 7.1).
    bool vsync = true;
    // Pixels per side of the shadow map (spec 5.4, G-09). Fixed for the renderer's life.
    std::uint32_t shadowMapSize = 2048;
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

    // Render phase: acquires a swapchain image, records and submits the frame. The scene is copied: it
    // can change as soon as this returns (G-01). Handles that are not ready give the fallbacks (G-03).
    // Does nothing while the window is minimized (spec 5.2). An error means the device is unusable.
    [[nodiscard]] core::Result<void> renderFrame(const RenderScene& scene);
    // Presentation phase: presents the image of the last renderFrame, if there is one.
    [[nodiscard]] core::Result<void> presentFrame();

    // Mirrors the cache on the GPU (decision 0014): uploads what became ready, frees what was unloaded
    // once no frame in flight uses it. Called each frame before renderFrame; cheap when nothing changed.
    // A GPU failure is logged with the resource's name; the fallback is drawn instead (spec 5.4).
    void syncResources(const resources::ResourceCache& cache);

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
