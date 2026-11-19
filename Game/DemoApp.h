#pragma once

#include "Core/Error.h"
#include "Core/FramePhases.h"
#include "Core/Logger.h"
#include "Graphics/Renderer.h"
#include "Input/ActionMap.h"
#include "Input/ActionState.h"
#include "Platform/Window.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace apex::game {

// Phase 2 demo: clears the window through the renderer, logs window events and actions, toggles cursor
// capture with the ToggleCursor action.
class DemoApp final : public core::FramePhases {
public:
    // The window, the renderer and the logger must outlive the app. maxFrames: quit after that many frames.
    [[nodiscard]] static core::Result<DemoApp> create(platform::Window& window, graphics::Renderer& renderer,
                                                      core::Logger& logger, input::ActionMap actions,
                                                      std::optional<std::uint64_t> maxFrames);

    [[nodiscard]] bool processInput() override;
    void simulate(const core::FixedStep& step) override;
    void extract(double alpha) override;
    void render() override;
    void present() override;

    // True when the renderer reported an error: the run stopped early.
    [[nodiscard]] bool failed() const noexcept;

private:
    struct AxisLog {
        input::ActionId id;
        float lastValue = 0.0f;
    };

    DemoApp(platform::Window& window, graphics::Renderer& renderer, core::Logger& logger,
            input::ActionMap actions);

    void handleWindowEvents(std::span<const platform::Event> events);
    void handleFrameActions();
    void logMouseLook(const core::FixedStep& step);
    void stopOnRenderError(const core::Result<void>& result);

    platform::Window* m_window;
    graphics::Renderer* m_renderer;
    core::Logger* m_logger;
    input::ActionMap m_actions;
    input::ActionState m_frame;
    input::ActionState m_simulation;

    input::ActionId m_toggleCursor{};
    std::vector<input::ActionId> m_frameButtons;
    std::vector<input::ActionId> m_gameplayButtons;
    std::vector<AxisLog> m_keyAxes;
    std::vector<input::ActionId> m_mouseAxes;
    std::vector<float> m_mouseTotals;

    std::optional<std::uint64_t> m_maxFrames;
    std::uint64_t m_frameCount = 0;
    bool m_quitRequested = false;
    bool m_failed = false;
};

} // namespace apex::game
