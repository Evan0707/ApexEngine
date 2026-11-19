#pragma once

#include "Core/Error.h"
#include "Graphics/RenderOverlay.h"
#include "Graphics/Renderer.h"
#include "Platform/Window.h"

#include <cstdint>
#include <memory>

namespace apex::debug {

// Internal to Debug. The SDL3 and Vulkan backends of Dear ImGui (decisions 0008 and 0009). Its .cpp is
// the only file of the engine that includes SDL, Vulkan and the ImGui backends together.
// Needs a current ImGui context for its whole lifetime.
class ImGuiBackend {
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    // The window and the renderer must outlive the backend.
    [[nodiscard]] static core::Result<std::unique_ptr<ImGuiBackend>> create(platform::Window& window,
                                                                            graphics::Renderer& renderer);

    ImGuiBackend(ConstructionKey key, platform::Window& window, std::int32_t colorFormat) noexcept;
    // Waits for the GPU: the backend's buffers and textures may still be in use by a frame in flight.
    ~ImGuiBackend();
    ImGuiBackend(const ImGuiBackend&) = delete;
    ImGuiBackend& operator=(const ImGuiBackend&) = delete;

    // Before ImGui::NewFrame.
    void newFrame();
    // Records the draw data of the last ImGui::Render.
    void render(const graphics::OverlayCommands& commands);

private:
    platform::Window* m_window;
    graphics::Renderer* m_renderer = nullptr;
    // VkFormat of the pipeline, kept as an integer: no Vulkan type in this header.
    std::int32_t m_colorFormat;
};

} // namespace apex::debug
