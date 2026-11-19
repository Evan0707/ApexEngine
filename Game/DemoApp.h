#pragma once

#include "CommandLine.h"
#include "FreeCamera.h"

#include "Core/Error.h"
#include "Core/FramePhases.h"
#include "Core/Logger.h"
#include "Debug/BenchmarkRecorder.h"
#include "Debug/DebugUi.h"
#include "Debug/FrameTimeStats.h"
#include "Debug/LogHistory.h"
#include "Graphics/RenderScene.h"
#include "Graphics/Renderer.h"
#include "Input/ActionMap.h"
#include "Input/ActionState.h"
#include "Platform/Window.h"
#include "Resources/ResourceCache.h"
#include "Serialization/EngineConfig.h"

#include <entt/entity/registry.hpp>

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
    resources::ResourceCache& cache;
};

// Phase 5 demo: loads a scene file into its registry (spec 5.1: last), gives its entities "camera" and
// "spinner" their behavior, and draws what the registry holds, seen by the free camera, with the debug
// panel (F1). Logs window events and actions, toggles cursor capture with the ToggleCursor action; the
// mouse turns the camera only while the cursor is captured.
class DemoApp final : public core::FramePhases {
public:
    // options: scene, frame limit and benchmark mode; the renderer's vsync setting is only logged.
    // config: free camera settings. Fails when the scene file is invalid or has no camera; the error
    // names the file, the entity and the field (spec 5.6).
    [[nodiscard]] static core::Result<DemoApp> create(const DemoSystems& systems, input::ActionMap actions,
                                                      const CommandLineOptions& options,
                                                      const serialization::EngineConfig& config);

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
    void stepCamera(const core::FixedStep& step);
    [[nodiscard]] core::Result<void> loadScene(const CommandLineOptions& options);
    void measureFrame();
    void recordBenchmark(double frameSeconds);
    void logBenchmark() const;
    void stopOnRenderError(const core::Result<void>& result);

    platform::Window* m_window;
    graphics::Renderer* m_renderer;
    debug::DebugUi* m_debugUi;
    const debug::LogHistory* m_logHistory;
    core::Logger* m_logger;
    resources::ResourceCache* m_cache;
    // The world of the demo (spec 5.6): owned here, passed by reference (G-06).
    entt::registry m_registry;
    entt::entity m_cameraEntity = entt::null;
    input::ActionMap m_actions;
    input::ActionState m_frame;
    input::ActionState m_simulation;

    input::ActionId m_toggleCursor{};
    input::ActionId m_toggleDebug{};
    input::ActionId m_sprint{};
    input::ActionId m_moveForward{};
    input::ActionId m_moveRight{};
    input::ActionId m_moveUp{};
    input::ActionId m_lookX{};
    input::ActionId m_lookY{};
    std::vector<input::ActionId> m_frameButtons;
    std::vector<input::ActionId> m_gameplayButtons;
    std::vector<AxisLog> m_keyAxes;
    std::vector<input::ActionId> m_mouseAxes;
    std::vector<float> m_mouseTotals;

    FreeCameraSettings m_cameraSettings;
    debug::DebugControls m_controls;
    graphics::RenderScene m_renderScene;

    debug::FrameTimeStats m_frameTimes;
    std::optional<std::chrono::steady_clock::time_point> m_frameStart;

    std::optional<std::uint64_t> m_maxFrames;
    std::optional<std::uint64_t> m_benchmarkSeconds;
    debug::BenchmarkRecorder m_benchmark;
    double m_benchmarkWarmupSeconds = 0.0;
    bool m_vsync = true;
    std::uint64_t m_frameCount = 0;
    bool m_quitRequested = false;
    bool m_failed = false;
};

} // namespace apex::game
