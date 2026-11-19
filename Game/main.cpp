#include "CommandLine.h"
#include "DefaultBindings.h"
#include "DemoApp.h"
#include "DemoScene.h"

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
#include "Platform/Paths.h"
#include "Platform/Window.h"
#include "Resources/ResourceCache.h"

#include <cstdint>
#include <filesystem>
#include <memory>
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

// Every system is created here in the order of spec 5.1 and destroyed in reverse order when this returns.
[[nodiscard]] int runDemo(apex::core::Logger& logger, apex::graphics::ValidationLog& validation,
                          const apex::debug::LogHistory& logHistory,
                          const apex::game::CommandLineOptions& options) {
    using namespace apex;

    core::Result<platform::Window> window = platform::Window::create(platform::WindowConfig{}, logger);
    if (!window) {
        return kExitFailure;
    }

    core::Result<input::ActionMap> actions = input::ActionMap::create(game::defaultBindings());
    if (!actions) {
        logger.error("{}", actions.error().message);
        return kExitFailure;
    }

    const graphics::RendererConfig rendererConfig{
        .applicationName = "Apex Demo", .enableValidation = kEnableValidation, .vsync = options.vsync};
    core::Result<std::unique_ptr<graphics::Renderer>> renderer =
        graphics::Renderer::create(*window, logger, validation, rendererConfig);
    if (!renderer) {
        return kExitFailure;
    }

    // Spec 5.1: Resources after Graphics. Asset paths are relative to this folder (spec 3.3).
    std::filesystem::path assetsRoot;
    if (options.assetsDirectory) {
        assetsRoot = *options.assetsDirectory;
    } else if (core::Result<std::filesystem::path> executable = platform::executableDirectory()) {
        assetsRoot = *executable / "Assets";
    } else {
        logger.error("{}", executable.error().message);
        return kExitFailure;
    }
    if (std::error_code error; !std::filesystem::is_directory(assetsRoot, error)) {
        logger.warning("Asset folder not found: '{}'; models will show as fallback cubes",
                       assetsRoot.generic_string());
    } else {
        logger.info("Asset folder: '{}'", assetsRoot.generic_string());
    }
    resources::ResourceCache cache(assetsRoot, logger);

    core::Result<std::unique_ptr<debug::DebugUi>> debugUi =
        debug::DebugUi::create(*window, **renderer, logger);
    if (!debugUi) {
        return kExitFailure;
    }

    // Loaded last (spec 5.1), destroyed first.
    game::DemoScene scene = game::DemoScene::create(cache, options.extraModel);
    logger.info("Demo scene: {} objects", scene.objectCount());

    const game::DemoSystems systems{.window = *window,
                                    .renderer = **renderer,
                                    .debugUi = **debugUi,
                                    .logHistory = logHistory,
                                    .logger = logger,
                                    .cache = cache,
                                    .scene = scene};
    core::Result<game::DemoApp> app = game::DemoApp::create(systems, std::move(*actions), options);
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
