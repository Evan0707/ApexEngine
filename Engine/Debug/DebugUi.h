#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Debug/StatsPanel.h"
#include "Graphics/RenderOverlay.h"
#include "Graphics/Renderer.h"
#include "Platform/Window.h"

#include <memory>

struct ImGuiContext;

namespace apex::debug {

class ImGuiBackend;

// The debug interface (spec 5.9, decision 0008): owns the Dear ImGui context and draws over the scene.
// No ImGui type in this header: Game uses this class only.
class DebugUi final : public graphics::RenderOverlay {
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    // One per process. The window, the renderer and the logger must outlive it. Registers itself as the
    // renderer's overlay.
    [[nodiscard]] static core::Result<std::unique_ptr<DebugUi>>
    create(platform::Window& window, graphics::Renderer& renderer, core::Logger& logger);

    DebugUi(ConstructionKey key, graphics::Renderer& renderer, ImGuiContext* context) noexcept;
    ~DebugUi() override;
    DebugUi(const DebugUi&) = delete;
    DebugUi& operator=(const DebugUi&) = delete;

    // Builds the interface of this frame. Called once per frame, after the input phase and before
    // Renderer::renderFrame. The panel's widgets write into controls.
    void buildFrame(const DebugPanelData& data, DebugControls& controls);

    // The panel of spec 5.9, shown and hidden by F1. Hidden at start.
    void setPanelVisible(bool visible) noexcept;
    [[nodiscard]] bool panelVisible() const noexcept;

    // Off while the game holds the cursor (relative mode): the hidden cursor must not drive the interface.
    void setMouseInput(bool enabled);

    // ImGui uses the keyboard or the mouse: the game must not react to them.
    [[nodiscard]] bool wantsKeyboard() const;
    [[nodiscard]] bool wantsMouse() const;

    void record(const graphics::OverlayCommands& commands) override;

private:
    graphics::Renderer* m_renderer;
    ImGuiContext* m_context;
    std::unique_ptr<ImGuiBackend> m_backend;
    bool m_panelVisible = false;
};

} // namespace apex::debug
