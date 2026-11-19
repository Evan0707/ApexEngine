#pragma once

#include "Core/Error.h"

#include <cstdint>
#include <optional>
#include <span>
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
};

// Arguments without the program name. Unknown or malformed options are an InvalidArgument error.
[[nodiscard]] core::Result<CommandLineOptions> parseCommandLine(std::span<const std::string_view> arguments);

} // namespace apex::game
