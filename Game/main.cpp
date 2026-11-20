#include "Bindings.h"
#include "CommandLine.h"
#include "DemoApp.h"

#include "Core/ConsoleLogSink.h"
#include "Core/FileLogSink.h"
#include "Core/Logger.h"
#include "Core/MainLoop.h"
#include "Core/Version.h"
#include "Debug/DebugUi.h"
#include "Debug/LogHistory.h"
#include "Debug/LogHistorySink.h"
#include "Graphics/Renderer.h"
#include "Graphics/ValidationLog.h"
#include "Input/ActionMap.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Runtime.h"
#include "Platform/Paths.h"
#include "Platform/Window.h"
#include "Resources/ResourceCache.h"
#include "Serialization/EngineConfig.h"
#include "Serialization/InputConfig.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitFailure = 1;
// The run ended, but the Vulkan validation layers reported at least one message (decision 0006).
constexpr int kExitValidationMessages = 2;

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

// A configuration file of Config/. Absent: the defaults, with a warning. Invalid: nothing, with the error
// logged; the demo then stops (decision 0017).
template <typename Config, typename Load>
[[nodiscard]] std::optional<Config> loadConfig(apex::core::Logger& logger, const std::filesystem::path& file,
                                               std::string_view name, Load&& load, Config defaults) {
    apex::core::Result<Config> config = load(file, name);
    if (config) {
        return std::move(*config);
    }
    if (config.error().code == apex::core::ErrorCode::NotFound) {
        logger.warning("{}; using the default values", config.error().message);
        return defaults;
    }
    logger.error("{}", config.error().message);
    return std::nullopt;
}

// Every system is created here in the order of spec 5.1 and destroyed in reverse order when this returns.
[[nodiscard]] int runDemo(apex::core::Logger& logger, apex::graphics::ValidationLog& validation,
                          const apex::debug::LogHistory& logHistory,
                          const apex::game::CommandLineOptions& options) {
    using namespace apex;

    core::Result<std::filesystem::path> executable = platform::executableDirectory();
    if (!executable) {
        logger.error("{}", executable.error().message);
        return kExitFailure;
    }

    // Configuration first: the window needs its size (spec 5.2, decision 0017).
    const std::filesystem::path configRoot =
        options.configDirectory ? std::filesystem::path(*options.configDirectory) : *executable / "Config";
    const std::optional<serialization::EngineConfig> engineConfig = loadConfig<serialization::EngineConfig>(
        logger, configRoot / "engine.json", "Config/engine.json", serialization::loadEngineConfigFile, {});
    const std::optional<serialization::InputConfig> inputConfig = loadConfig<serialization::InputConfig>(
        logger, configRoot / "input.json", "Config/input.json", serialization::loadInputConfigFile,
        serialization::defaultInputConfig());
    if (!engineConfig || !inputConfig) {
        return kExitFailure;
    }

    core::Result<platform::Window> window = platform::Window::create(
        platform::WindowConfig{.width = engineConfig->windowWidth, .height = engineConfig->windowHeight},
        logger);
    if (!window) {
        return kExitFailure;
    }

    core::Result<input::ActionMap> actions = input::ActionMap::create(game::toInputBindings(*inputConfig));
    if (!actions) {
        logger.error("Configuration 'Config/input.json': {}", actions.error().message);
        return kExitFailure;
    }

    const graphics::RendererConfig rendererConfig{.applicationName = "Apex Demo",
                                                  .enableValidation = kEnableValidation,
                                                  .vsync = options.vsync,
                                                  .shadowMapSize = engineConfig->shadowMapSize};
    core::Result<std::unique_ptr<graphics::Renderer>> renderer =
        graphics::Renderer::create(*window, logger, validation, rendererConfig);
    if (!renderer) {
        return kExitFailure;
    }

    // Spec 5.1: Resources after Graphics. Asset paths are relative to this folder (spec 3.3).
    const std::filesystem::path assetsRoot =
        options.assetsDirectory ? std::filesystem::path(*options.assetsDirectory) : *executable / "Assets";
    if (std::error_code error; !std::filesystem::is_directory(assetsRoot, error)) {
        logger.warning("Asset folder not found: '{}'; models will show as fallback cubes",
                       assetsRoot.generic_string());
    } else {
        logger.info("Asset folder: '{}'", assetsRoot.generic_string());
    }
    resources::ResourceCache cache(assetsRoot, logger);

    // Spec 5.1: Physics after Resources. The runtime holds Jolt's process-wide state (decision 0019).
    core::Result<physics::Runtime> physicsRuntime = physics::Runtime::create();
    if (!physicsRuntime) {
        logger.error("{}", physicsRuntime.error().message);
        return kExitFailure;
    }
    core::Result<std::unique_ptr<physics::PhysicsWorld>> physicsWorld = physics::PhysicsWorld::create(
        *physicsRuntime, physics::PhysicsSettings{.gravity = engineConfig->gravity});
    if (!physicsWorld) {
        logger.error("{}", physicsWorld.error().message);
        return kExitFailure;
    }

    core::Result<std::unique_ptr<debug::DebugUi>> debugUi =
        debug::DebugUi::create(*window, **renderer, logger);
    if (!debugUi) {
        return kExitFailure;
    }

    const game::DemoSystems systems{.window = *window,
                                    .renderer = **renderer,
                                    .debugUi = **debugUi,
                                    .logHistory = logHistory,
                                    .logger = logger,
                                    .cache = cache,
                                    .physics = **physicsWorld};
    // The scene is loaded last (spec 5.1), by the application that owns the registry.
    core::Result<game::DemoApp> app =
        game::DemoApp::create(systems, std::move(*actions), options, *engineConfig);
    if (!app) {
        logger.error("{}", app.error().message);
        return kExitFailure;
    }

    core::MainLoop loop;
    loop.run(*app);

    logger.info("Shutting down");
    return app->failed() ? kExitFailure : kExitSuccess;
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace apex;

    // Declared first so that they outlive every system: shutdown messages are logged and counted too.
    // The history outlives the logger, whose sink writes into it.
    debug::LogHistory logHistory;
    core::Logger logger;
    logger.addSink(std::make_unique<core::ConsoleLogSink>());
    logger.addSink(std::make_unique<debug::LogHistorySink>(logHistory));
    if (auto fileSink = core::FileLogSink::open("Logs/apex.log")) {
        logger.addSink(std::move(*fileSink));
    } else {
        logger.warning("{}", fileSink.error().message);
    }
    graphics::ValidationLog validation(logger);
    logger.info("Apex Engine {}", core::engineVersion().toString());

    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    const core::Result<game::CommandLineOptions> options = game::parseCommandLine(arguments);
    if (!options) {
        logger.error("{}", options.error().message);
        return kExitFailure;
    }

    const int exitCode = runDemo(logger, validation, logHistory, *options);
    if (const std::uint32_t count = validation.messageCount(); count > 0) {
        logger.error("{} Vulkan validation message(s) during the run", count);
        return exitCode == kExitSuccess ? kExitValidationMessages : exitCode;
    }
    return exitCode;
}
