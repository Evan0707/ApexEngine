#include "DemoApp.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace apex::game {

namespace {

// Actions handled in the input phase: they act on the application, not on the simulation.
constexpr std::array<std::string_view, 4> kFrameButtons{"ToggleCursor", "ToggleDebug", "ToggleColliders",
                                                        "ToggleShadows"};
constexpr std::array<std::string_view, 3> kGameplayButtons{"Jump", "Sprint", "SwitchCamera"};
constexpr std::array<std::string_view, 3> kKeyAxes{"MoveForward", "MoveRight", "MoveUp"};
constexpr std::array<std::string_view, 2> kMouseAxes{"LookX", "LookY"};

// One log line per second of simulation for the mouse, instead of one per step.
constexpr std::uint64_t kMouseLogPeriodTicks = 60;

[[nodiscard]] core::Result<input::ActionId> findAction(const input::ActionMap& actions,
                                                       std::string_view name) {
    if (const std::optional<input::ActionId> id = actions.find(name)) {
        return *id;
    }
    return core::makeError(core::ErrorCode::NotFound,
                           std::format("The bindings define no action '{}'", name));
}

template <std::size_t Count>
[[nodiscard]] core::Result<std::vector<input::ActionId>>
findActions(const input::ActionMap& actions, const std::array<std::string_view, Count>& names) {
    std::vector<input::ActionId> ids;
    for (const std::string_view name : names) {
        const core::Result<input::ActionId> id = findAction(actions, name);
        if (!id) {
            return std::unexpected(id.error());
        }
        ids.push_back(*id);
    }
    return ids;
}

} // namespace

core::Result<DemoApp> DemoApp::create(platform::Window& window, core::Logger& logger,
                                      input::ActionMap actions) {
    DemoApp app(window, logger, std::move(actions));

    const core::Result<input::ActionId> toggleCursor = findAction(app.m_actions, "ToggleCursor");
    if (!toggleCursor) {
        return std::unexpected(toggleCursor.error());
    }
    auto frameButtons = findActions(app.m_actions, kFrameButtons);
    if (!frameButtons) {
        return std::unexpected(frameButtons.error());
    }
    auto gameplayButtons = findActions(app.m_actions, kGameplayButtons);
    if (!gameplayButtons) {
        return std::unexpected(gameplayButtons.error());
    }
    auto keyAxes = findActions(app.m_actions, kKeyAxes);
    if (!keyAxes) {
        return std::unexpected(keyAxes.error());
    }
    auto mouseAxes = findActions(app.m_actions, kMouseAxes);
    if (!mouseAxes) {
        return std::unexpected(mouseAxes.error());
    }

    app.m_toggleCursor = *toggleCursor;
    app.m_frameButtons = std::move(*frameButtons);
    app.m_gameplayButtons = std::move(*gameplayButtons);
    for (const input::ActionId id : *keyAxes) {
        app.m_keyAxes.push_back({id});
    }
    app.m_mouseAxes = std::move(*mouseAxes);
    app.m_mouseTotals.assign(app.m_mouseAxes.size(), 0.0f);
    return app;
}

DemoApp::DemoApp(platform::Window& window, core::Logger& logger, input::ActionMap actions)
    : m_window(&window), m_logger(&logger), m_actions(std::move(actions)), m_frame(m_actions),
      m_simulation(m_actions) {}

bool DemoApp::processInput() {
    const std::span<const platform::Event> events = m_window->pollEvents();
    handleWindowEvents(events);

    // Sequence documented in Engine/Input/README.md.
    m_frame.apply(m_actions, events);
    handleFrameActions();
    m_simulation.accumulate(m_frame);
    m_frame.clearTransient();

    return !m_quitRequested;
}

void DemoApp::handleWindowEvents(std::span<const platform::Event> events) {
    for (const platform::Event& event : events) {
        if (std::holds_alternative<platform::CloseRequested>(event)) {
            if (!m_quitRequested) {
                m_logger->info("Close requested");
            }
            m_quitRequested = true;
        } else if (const auto* resized = std::get_if<platform::WindowResized>(&event)) {
            m_logger->info("Window resized: {}x{} pixels", resized->width, resized->height);
        } else if (std::holds_alternative<platform::WindowMinimized>(event)) {
            m_logger->info("Window minimized");
        } else if (std::holds_alternative<platform::WindowRestored>(event)) {
            m_logger->info("Window restored");
        } else if (std::holds_alternative<platform::FocusLost>(event)) {
            m_logger->info("Focus lost: every key is released");
        }
    }
}

void DemoApp::handleFrameActions() {
    for (const input::ActionId id : m_frameButtons) {
        if (m_frame.wasPressed(id)) {
            m_logger->info("Action {} pressed", m_actions.name(id));
        }
    }
    if (m_frame.wasPressed(m_toggleCursor)) {
        m_window->setCursorCaptured(!m_window->isCursorCaptured());
        m_logger->info("Cursor {}", m_window->isCursorCaptured() ? "captured" : "released");
    }
}

void DemoApp::simulate(const core::FixedStep& step) {
    for (const input::ActionId id : m_gameplayButtons) {
        if (m_simulation.wasPressed(id)) {
            m_logger->info("[tick {}] Action {} pressed", step.tick, m_actions.name(id));
        }
        if (m_simulation.wasReleased(id)) {
            m_logger->info("[tick {}] Action {} released", step.tick, m_actions.name(id));
        }
    }
    for (AxisLog& axis : m_keyAxes) {
        const float value = m_simulation.axis(axis.id);
        if (value != axis.lastValue) {
            m_logger->info("[tick {}] Axis {} = {}", step.tick, m_actions.name(axis.id), value);
            axis.lastValue = value;
        }
    }
    logMouseLook(step);

    m_simulation.clearTransient();
}

void DemoApp::logMouseLook(const core::FixedStep& step) {
    bool moved = false;
    for (std::size_t i = 0; i < m_mouseAxes.size(); ++i) {
        m_mouseTotals[i] += m_simulation.axis(m_mouseAxes[i]);
        moved = moved || m_mouseTotals[i] != 0.0f;
    }
    if ((step.tick + 1) % kMouseLogPeriodTicks != 0) {
        return;
    }
    if (moved) {
        std::string line = std::format("[tick {}] Mouse over the last second:", step.tick);
        for (std::size_t i = 0; i < m_mouseAxes.size(); ++i) {
            line += std::format(" {} = {}", m_actions.name(m_mouseAxes[i]), m_mouseTotals[i]);
        }
        m_logger->info("{}", line);
    }
    m_mouseTotals.assign(m_mouseTotals.size(), 0.0f);
}

void DemoApp::extract(double /*alpha*/) {}

void DemoApp::render() {}

void DemoApp::present() {
    // No swapchain before phase 2: sleep instead of spinning a core, longer while minimized.
    const auto pause = m_window->isMinimized() ? std::chrono::milliseconds(10) : std::chrono::milliseconds(1);
    std::this_thread::sleep_for(pause);
}

} // namespace apex::game
