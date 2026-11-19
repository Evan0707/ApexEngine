#include "CommandLine.h"
#include "DefaultBindings.h"
#include "DemoApp.h"

#include "Core/ConsoleLogSink.h"
#include "Core/FileLogSink.h"
#include "Core/Logger.h"
#include "Core/MainLoop.h"
#include "Core/Version.h"
#include "Graphics/Renderer.h"
#include "Graphics/ValidationLog.h"
#include "Input/ActionMap.h"
#include "Platform/Window.h"

#include <cstdint>
#include <memory>
#include <string_view>
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

    core::Result<game::DemoApp> app =
        game::DemoApp::create(*window, **renderer, logger, std::move(*actions), options.maxFrames);
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
    core::Logger logger;
    logger.addSink(std::make_unique<core::ConsoleLogSink>());
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

    const int exitCode = runDemo(logger, validation, *options);
    if (const std::uint32_t count = validation.messageCount(); count > 0) {
        logger.error("{} Vulkan validation message(s) during the run", count);
        return exitCode == kExitSuccess ? kExitValidationMessages : exitCode;
    }
    return exitCode;
}
