#include "DemoApp.h"

#include "GameSystems.h"

#include "Debug/InputCapture.h"
#include "Debug/StatsPanel.h"
#include "Resources/AssetPath.h"
#include "Serialization/SceneLoader.h"
#include "World/Components.h"
#include "World/PhysicsSync.h"
#include "World/RenderExtraction.h"
#include "World/SceneQueries.h"
#include "World/ShadowFocus.h"

#include <array>
#include <chrono>
#include <cmath>
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

// Benchmark (spec 7.1): frames of the first seconds (pipeline creation, first swapchain) are not
// measured. The camera circles the scene, one turn in 20 s, always looking at its center.
constexpr double kBenchmarkWarmupSeconds = 2.0;
constexpr double kBenchmarkTurnSeconds = 20.0;
constexpr float kBenchmarkRadius = 9.0f;
constexpr float kBenchmarkHeight = 3.0f;

#ifdef NDEBUG
constexpr std::string_view kBuildName = "Release";
#else
constexpr std::string_view kBuildName = "Debug";
#endif

[[nodiscard]] FreeCameraState benchmarkCamera(double seconds) {
    const auto angle = static_cast<float>(seconds / kBenchmarkTurnSeconds * glm::two_pi<double>());
    // Yaw a looks towards (-sin a, 0, -cos a): from (sin a, 0, cos a) * radius, that is the center.
    return FreeCameraState{
        .position = {kBenchmarkRadius * std::sin(angle), kBenchmarkHeight,
                     kBenchmarkRadius * std::cos(angle)},
        .yaw = angle,
        .pitch = glm::radians(-15.0f),
    };
}

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

core::Result<DemoApp> DemoApp::create(const DemoSystems& systems, input::ActionMap actions,
                                      const CommandLineOptions& options,
                                      const serialization::EngineConfig& config) {
    DemoApp app(systems, std::move(actions));
    app.m_cameraSettings.speed = config.freeCameraSpeed;
    app.m_cameraSettings.sprintMultiplier = config.freeCameraSprintMultiplier;
    app.m_cameraSettings.lookSensitivity = config.freeCameraLookSensitivity;
    app.m_orbitSettings.distance = config.thirdPersonCameraDistance;
    app.m_orbitSettings.aimHeight = config.thirdPersonCameraAimHeight;
    app.m_orbitSettings.lookSensitivity = config.freeCameraLookSensitivity;
    app.m_playerSettings.walkSpeed = config.characterWalkSpeed;
    app.m_playerSettings.runSpeed = config.characterRunSpeed;
    app.m_playerSettings.jumpHeight = config.characterJumpHeight;
    app.m_characterBody =
        world::CharacterBody{.height = config.characterHeight,
                             .radius = config.characterRadius,
                             .maxSlopeRadians = glm::radians(config.characterMaxSlopeDegrees),
                             .stepHeight = config.characterStepHeight,
                             .maxPushForce = config.characterPushForce};
    app.m_shadowSettings.mapSize = config.shadowMapSize;
    app.m_shadowSettings.areaSize = config.shadowAreaSize;
    app.m_controls.shadows = config.shadowsEnabled;
    app.m_controls.shadowConstantBias = config.shadowConstantBias;
    app.m_controls.shadowSlopeBias = config.shadowSlopeBias;
    if (core::Result<void> loaded = app.loadScene(options); !loaded) {
        return std::unexpected(loaded.error());
    }
    app.m_maxFrames = options.maxFrames;
    app.m_benchmarkSeconds = options.benchmarkSeconds;
    app.m_benchmarkWarmupSeconds = kBenchmarkWarmupSeconds;
    app.m_vsync = options.vsync;
    if (options.benchmarkSeconds) {
        placeFreeCameraEntity(app.m_registry, app.m_cameraEntity, benchmarkCamera(0.0));
        app.m_logger->info("Benchmark: {} s measured after {} s of warm-up, camera on a fixed path",
                           *options.benchmarkSeconds, kBenchmarkWarmupSeconds);
    }

    const core::Result<input::ActionId> toggleCursor = findAction(app.m_actions, "ToggleCursor");
    if (!toggleCursor) {
        return std::unexpected(toggleCursor.error());
    }
    const core::Result<input::ActionId> toggleDebug = findAction(app.m_actions, "ToggleDebug");
    if (!toggleDebug) {
        return std::unexpected(toggleDebug.error());
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
    app.m_toggleDebug = *toggleDebug;
    app.m_toggleShadows = *app.m_actions.find("ToggleShadows");
    // Present: findActions above checked every name.
    app.m_sprint = *app.m_actions.find("Sprint");
    app.m_jump = *app.m_actions.find("Jump");
    app.m_switchCamera = *app.m_actions.find("SwitchCamera");
    app.m_moveForward = *app.m_actions.find("MoveForward");
    app.m_moveRight = *app.m_actions.find("MoveRight");
    app.m_moveUp = *app.m_actions.find("MoveUp");
    app.m_lookX = *app.m_actions.find("LookX");
    app.m_lookY = *app.m_actions.find("LookY");
    app.m_frameButtons = std::move(*frameButtons);
    app.m_gameplayButtons = std::move(*gameplayButtons);
    for (const input::ActionId id : *keyAxes) {
        app.m_keyAxes.push_back({id});
    }
    app.m_mouseAxes = std::move(*mouseAxes);
    app.m_mouseTotals.assign(app.m_mouseAxes.size(), 0.0f);
    return app;
}

DemoApp::DemoApp(const DemoSystems& systems, input::ActionMap actions)
    : m_window(&systems.window), m_renderer(&systems.renderer), m_debugUi(&systems.debugUi),
      m_logHistory(&systems.logHistory), m_logger(&systems.logger), m_cache(&systems.cache),
      m_physics(&systems.physics), m_actions(std::move(actions)), m_frame(m_actions),
      m_simulation(m_actions) {}

core::Result<void> DemoApp::loadScene(const CommandLineOptions& options) {
    const core::Result<resources::AssetPath> scenePath = resources::AssetPath::parse(options.scene);
    if (!scenePath) {
        return core::makeError(scenePath.error().code,
                               std::format("Cannot load the scene: {}", scenePath.error().message));
    }
    if (core::Result<void> loaded = serialization::loadSceneFile(m_cache->root() / scenePath->str(),
                                                                 scenePath->str(), m_registry, *m_cache);
        !loaded) {
        return loaded;
    }
    if (options.extraModel) {
        // On the floor, to the right of the character's starting place. Centered on y = 0.5: a model of 1 m
        // centered on its origin, such as the fallback cube, rests on the floor.
        const entt::entity extra = m_registry.create();
        m_registry.emplace<world::SceneId>(extra, "extra_model");
        m_registry.emplace<world::Transform>(extra, world::Transform{.position = {1.8f, 0.5f, 2.5f}});
        m_registry.emplace<world::MeshRenderer>(extra, m_cache->loadModel(*options.extraModel));
    }

    // The demo's own components, by scene id (decision 0017).
    const core::Result<GameEntities> entities = attachGameComponents(m_registry, m_characterBody);
    if (!entities) {
        return core::makeError(entities.error().code,
                               std::format("Scene '{}': {}", scenePath->str(), entities.error().message));
    }
    m_cameraEntity = entities->camera;
    m_playerEntity = entities->player;
    // The panel starts at the scene's field of view, kept inside the free camera's range (spec 5.3).
    m_controls.minVerticalFovDegrees = m_cameraSettings.minVerticalFovDegrees;
    m_controls.maxVerticalFovDegrees = m_cameraSettings.maxVerticalFovDegrees;
    m_controls.verticalFovDegrees = clampVerticalFov(
        glm::degrees(m_registry.get<world::Camera>(m_cameraEntity).verticalFovRadians), m_cameraSettings);
    // Bodies for the colliders of the scene and the character of the player (spec 4.3).
    if (core::Result<void> created = world::createPhysicsObjects(m_registry, *m_physics); !created) {
        return core::makeError(created.error().code,
                               std::format("Scene '{}': {}", scenePath->str(), created.error().message));
    }
    m_logger->info("Scene '{}' loaded: {} entities, {} physics bodies", scenePath->str(),
                   world::sceneEntityCount(m_registry), m_physics->bodyCount());
    return {};
}

bool DemoApp::processInput() {
    if (m_maxFrames && m_frameCount == *m_maxFrames) {
        m_logger->info("Frame limit reached: {} frames", m_frameCount);
        return false;
    }
    ++m_frameCount;
    measureFrame();

    const std::span<const platform::Event> events = m_window->pollEvents();
    handleWindowEvents(events);

    // What the debug interface uses does not reach the game. Sequence documented in Engine/Input/README.md.
    const std::vector<platform::Event> gameEvents =
        debug::filterCapturedEvents(events, m_debugUi->wantsKeyboard(), m_debugUi->wantsMouse());
    m_frame.apply(m_actions, gameEvents);
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
        m_debugUi->setMouseInput(!m_window->isCursorCaptured());
        m_logger->info("Cursor {}", m_window->isCursorCaptured() ? "captured" : "released");
    }
    if (m_frame.wasPressed(m_toggleDebug)) {
        m_debugUi->setPanelVisible(!m_debugUi->panelVisible());
    }
    if (m_frame.wasPressed(m_toggleShadows)) {
        m_controls.shadows = !m_controls.shadows;
        m_logger->info("Shadows {}", m_controls.shadows ? "on" : "off");
    }
}

void DemoApp::measureFrame() {
    // Real time is read here, outside the simulation (G-05): it only feeds the debug panel.
    const auto now = std::chrono::steady_clock::now();
    if (m_frameStart) {
        const double frameSeconds = std::chrono::duration<double>(now - *m_frameStart).count();
        const graphics::FrameCounters counters = m_renderer->lastFrameCounters();
        m_frameTimes.addFrame({.frameSeconds = frameSeconds,
                               .cpuSeconds = frameSeconds - counters.waitSeconds,
                               .gpuSeconds = counters.gpuSeconds});
        if (m_benchmarkSeconds) {
            recordBenchmark(frameSeconds);
        }
    }
    m_frameStart = now;
}

void DemoApp::recordBenchmark(double frameSeconds) {
    if (m_benchmarkWarmupSeconds > 0.0) {
        m_benchmarkWarmupSeconds -= frameSeconds;
        return;
    }
    m_benchmark.addFrame(frameSeconds);
    if (!m_quitRequested && m_benchmark.elapsedSeconds() >= static_cast<double>(*m_benchmarkSeconds)) {
        logBenchmark();
        m_quitRequested = true;
    }
}

void DemoApp::logBenchmark() const {
    const debug::BenchmarkSummary summary = m_benchmark.summary();
    const graphics::FrameCounters counters = m_renderer->lastFrameCounters();
    const platform::PixelSize size = m_window->sizeInPixels();
    m_logger->info("Benchmark: {} frames in {:.1f} s, {:.1f} FPS average; frame time {:.2f} ms average, "
                   "{:.2f} ms 99th percentile, {:.2f} ms max",
                   summary.frameCount, summary.durationSeconds, summary.averageFramesPerSecond,
                   summary.averageFrameMs, summary.percentile99FrameMs, summary.maxFrameMs);
    m_logger->info("Benchmark settings: {} build, {}x{} pixels, vsync {}, {} objects drawn, {} triangles, "
                   "graphics options: none",
                   kBuildName, size.width, size.height, m_vsync ? "on" : "off", counters.drawCalls,
                   counters.triangles);
    // Compared with the "GPU upload" line: a drift during the run would show here (spec 7.2).
    constexpr double kMebibyte = 1024.0 * 1024.0;
    m_logger->info("Benchmark end: GPU memory {:.0f} MiB used",
                   static_cast<double>(m_renderer->memoryStats().usedBytes) / kMebibyte);
}

void DemoApp::simulate(const core::FixedStep& step) {
    // Before anything moves: the extraction interpolates from this state (spec 5.1).
    world::rememberTransforms(m_registry);
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
    if (m_simulation.wasPressed(m_switchCamera)) {
        switchCamera();
    }
    stepCamera(step);
    stepPlayer(step);
    stepSpinners(m_registry, step.deltaSeconds);
    // After the game wrote its intents: Physics advances and the transforms follow (spec 4.3).
    world::stepPhysics(m_registry, *m_physics, step.deltaSeconds);
    // Last: the camera follows where the character is at the end of this step (decision 0019).
    stepThirdPersonCamera(step);

    m_simulation.clearTransient();
}

void DemoApp::stepCamera(const core::FixedStep& step) {
    if (m_benchmarkSeconds) {
        placeFreeCameraEntity(m_registry, m_cameraEntity,
                              benchmarkCamera(static_cast<double>(step.tick + 1) * step.deltaSeconds));
        return;
    }
    if (m_thirdPerson) {
        // The move actions belong to the character.
        return;
    }
    // Released cursor: the mouse belongs to the interface, not to the camera.
    const bool looking = m_window->isCursorCaptured();
    const FreeCameraInput input{
        .forward = m_simulation.axis(m_moveForward),
        .right = m_simulation.axis(m_moveRight),
        .up = m_simulation.axis(m_moveUp),
        .sprint = m_simulation.isHeld(m_sprint),
        .lookX = looking ? m_simulation.axis(m_lookX) : 0.0f,
        .lookY = looking ? m_simulation.axis(m_lookY) : 0.0f,
    };
    stepFreeCameraEntity(m_registry, m_cameraEntity, input, step.deltaSeconds, m_cameraSettings);
}

void DemoApp::stepPlayer(const core::FixedStep& step) {
    if (m_playerEntity == entt::null) {
        return;
    }
    // Without input the character still falls, brakes and stands: it is stepped in both camera modes.
    PlayerInput input;
    if (m_thirdPerson) {
        input = PlayerInput{.forward = m_simulation.axis(m_moveForward),
                            .right = m_simulation.axis(m_moveRight),
                            .sprint = m_simulation.isHeld(m_sprint),
                            .jump = m_simulation.wasPressed(m_jump),
                            .cameraYaw = m_registry.get<OrbitCameraController>(m_cameraEntity).state.yaw};
    }
    stepPlayerEntity(m_registry, m_playerEntity, input, m_physics->gravity(), step.deltaSeconds,
                     m_playerSettings);
}

void DemoApp::switchCamera() {
    if (m_benchmarkSeconds) {
        return;
    }
    if (m_playerEntity == entt::null) {
        m_logger->warning("The scene has no entity '{}': the camera stays free", kPlayerId);
        return;
    }
    m_thirdPerson = !m_thirdPerson;
    if (m_thirdPerson) {
        startOrbitCameraEntity(m_registry, m_cameraEntity, m_playerEntity, m_orbitSettings);
    } else {
        releaseOrbitCameraEntity(m_registry, m_cameraEntity);
    }
    m_logger->info("Camera: {}", m_thirdPerson ? "third person" : "free");
}

void DemoApp::stepThirdPersonCamera(const core::FixedStep& step) {
    if (!m_thirdPerson) {
        return;
    }
    // Released cursor: the mouse belongs to the interface, not to the camera.
    const bool looking = m_window->isCursorCaptured();
    const OrbitCameraInput input{.lookX = looking ? m_simulation.axis(m_lookX) : 0.0f,
                                 .lookY = looking ? m_simulation.axis(m_lookY) : 0.0f};
    stepOrbitCameraEntity(m_registry, m_cameraEntity, m_playerEntity, input, *m_physics, step.deltaSeconds,
                          m_orbitSettings);
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

void DemoApp::extract(double alpha) {
    // The panel's field of view is a view setting, not simulation state: applied here.
    m_registry.get<world::Camera>(m_cameraEntity).verticalFovRadians =
        glm::radians(clampVerticalFov(m_controls.verticalFovDegrees, m_cameraSettings));
    world::extractRenderScene(m_registry, *m_cache, m_cameraEntity, static_cast<float>(alpha), m_renderScene);
    m_shadowSettings.enabled = m_controls.shadows;
    m_shadowSettings.constantBias = m_controls.shadowConstantBias;
    m_shadowSettings.slopeBias = m_controls.shadowSlopeBias;
    // Centered on the character, even in free camera (spec 5.4).
    world::extractShadow(m_registry, m_cameraEntity, m_playerEntity, static_cast<float>(alpha),
                         m_shadowSettings, m_renderScene);
}

void DemoApp::render() {
    m_debugUi->buildFrame(debug::DebugPanelData{.frameTimes = m_frameTimes,
                                                .counters = m_renderer->lastFrameCounters(),
                                                .memory = m_renderer->memoryStats(),
                                                .entityCount = world::sceneEntityCount(m_registry),
                                                .bodyCount = m_physics->bodyCount(),
                                                .activeBodyCount = m_physics->activeBodyCount(),
                                                .log = *m_logHistory},
                          m_controls);
    if (!m_failed) {
        m_renderer->syncResources(*m_cache);
        stopOnRenderError(m_renderer->renderFrame(m_renderScene));
    }
}

void DemoApp::present() {
    if (!m_failed) {
        stopOnRenderError(m_renderer->presentFrame());
    }
    // Rendering is suspended while minimized (spec 5.2): nothing waits for vsync, so do not spin a core.
    if (m_window->isMinimized()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool DemoApp::failed() const noexcept {
    return m_failed;
}

void DemoApp::stopOnRenderError(const core::Result<void>& result) {
    if (!result) {
        // Already logged by the renderer. The loop stops at the next input phase.
        m_failed = true;
        m_quitRequested = true;
    }
}

} // namespace apex::game
