#pragma once

#include "Core/Error.h"
#include "Core/FramePhases.h"
#include "Core/Logger.h"
#include "Debug/DebugUi.h"
#include "Debug/FrameTimeStats.h"
#include "Debug/LogHistory.h"
#include "Graphics/Renderer.h"
#include "Input/ActionMap.h"
#include "Input/ActionState.h"
#include "Platform/Window.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace apex::game {

// The systems the demo uses. All of them must outlive the app.
struct DemoSystems {
    platform::Window& window;
    graphics::Renderer& renderer;
    debug::DebugUi& debugUi;
    const debug::LogHistory& logHistory;
    core::Logger& logger;
};

// Phase 2 demo: draws the triangle and the debug panel (F1), logs window events and actions, toggles
// cursor capture with the ToggleCursor action.
class DemoApp final : public core::FramePhases {
public:
    // maxFrames: quit after that many frames.
    [[nodiscard]] static core::Result<DemoApp> create(const DemoSystems& systems, input::ActionMap actions,
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

    DemoApp(const DemoSystems& systems, input::ActionMap actions);

    void handleWindowEvents(std::span<const platform::Event> events);
    void handleFrameActions();
    void logMouseLook(const core::FixedStep& step);
    void measureFrame();
    void stopOnRenderError(const core::Result<void>& result);

    platform::Window* m_window;
    graphics::Renderer* m_renderer;
    debug::DebugUi* m_debugUi;
    const debug::LogHistory* m_logHistory;
    core::Logger* m_logger;
    input::ActionMap m_actions;
    input::ActionState m_frame;
    input::ActionState m_simulation;

    input::ActionId m_toggleCursor{};
    input::ActionId m_toggleDebug{};
    std::vector<input::ActionId> m_frameButtons;
    std::vector<input::ActionId> m_gameplayButtons;
    std::vector<AxisLog> m_keyAxes;
    std::vector<input::ActionId> m_mouseAxes;
    std::vector<float> m_mouseTotals;

    debug::FrameTimeStats m_frameTimes;
    std::optional<std::chrono::steady_clock::time_point> m_frameStart;

    std::optional<std::uint64_t> m_maxFrames;
    std::uint64_t m_frameCount = 0;
    bool m_quitRequested = false;
    bool m_failed = false;
};

} // namespace apex::game
