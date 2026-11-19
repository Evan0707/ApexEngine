#include "DefaultBindings.h"
#include "DemoApp.h"

#include "Core/ConsoleLogSink.h"
#include "Core/FileLogSink.h"
#include "Core/Logger.h"
#include "Core/MainLoop.h"
#include "Core/Version.h"
#include "Input/ActionMap.h"
#include "Platform/Window.h"

#include <memory>
#include <utility>

int main() {
    using namespace apex;

    // Declared first so that it outlives every system (spec 5.1: shutdown in reverse order).
    core::Logger logger;
    logger.addSink(std::make_unique<core::ConsoleLogSink>());
    if (auto fileSink = core::FileLogSink::open("Logs/apex.log")) {
        logger.addSink(std::move(*fileSink));
    } else {
        logger.warning("{}", fileSink.error().message);
    }
    logger.info("Apex Engine {}", core::engineVersion().toString());

    core::Result<platform::Window> window = platform::Window::create(platform::WindowConfig{}, logger);
    if (!window) {
        return 1;
    }

    core::Result<input::ActionMap> actions = input::ActionMap::create(game::defaultBindings());
    if (!actions) {
        logger.error("{}", actions.error().message);
        return 1;
    }

    core::Result<game::DemoApp> app = game::DemoApp::create(*window, logger, std::move(*actions));
    if (!app) {
        logger.error("{}", app.error().message);
        return 1;
    }

    core::MainLoop loop;
    loop.run(*app);

    logger.info("Shutting down");
    return 0;
}
