#pragma once

#include "Core/Error.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace apex::game {

struct CommandLineOptions {
    // --frames N: quit by itself after N frames (decision 0006: lets an agent run the demo unattended).
    std::optional<std::uint64_t> maxFrames;
    // --no-vsync: for performance measurements (spec 7.1).
    bool vsync = true;
    // --benchmark S: the camera follows a fixed path; after a warm-up, S seconds of frame times are
    // measured and logged, then the demo quits (spec 7.1).
    std::optional<std::uint64_t> benchmarkSeconds;
    // --assets DIR: the asset folder, instead of the Assets/ copied next to the executable.
    std::optional<std::string> assetsDirectory;
    // --model PATH: one more model, relative to the asset folder, placed in front of the camera. A path
    // that does not load shows the fallback cube (AC-11).
    std::optional<std::string> extraModel;
    // --scene PATH: the scene to load, relative to the asset folder (decision 0017).
    std::string scene = "Scenes/demo.json";
    // --config DIR: the folder of engine.json and input.json, instead of the Config/ copied next to the
    // executable.
    std::optional<std::string> configDirectory;
};

// Arguments without the program name. Unknown or malformed options are an InvalidArgument error.
[[nodiscard]] core::Result<CommandLineOptions> parseCommandLine(std::span<const std::string_view> arguments);

} // namespace apex::game
