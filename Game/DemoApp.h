#pragma once

#include "Core/Error.h"
#include "Core/FramePhases.h"
#include "Core/Logger.h"
#include "Input/ActionMap.h"
#include "Input/ActionState.h"
#include "Platform/Window.h"

#include <vector>

namespace apex::game {

// Phase 1 demo: logs window events and actions, toggles cursor capture with the ToggleCursor action.
class DemoApp final : public core::FramePhases {
public:
    // The window and the logger must outlive the app.
    [[nodiscard]] static core::Result<DemoApp> create(platform::Window& window, core::Logger& logger,
                                                      input::ActionMap actions);

    [[nodiscard]] bool processInput() override;
    void simulate(const core::FixedStep& step) override;
    void extract(double alpha) override;
    void render() override;
    void present() override;

private:
    struct AxisLog {
        input::ActionId id;
        float lastValue = 0.0f;
    };

    DemoApp(platform::Window& window, core::Logger& logger, input::ActionMap actions);

    void handleWindowEvents(std::span<const platform::Event> events);
    void handleFrameActions();
    void logMouseLook(const core::FixedStep& step);

    platform::Window* m_window;
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

    bool m_quitRequested = false;
};

} // namespace apex::game
