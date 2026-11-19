#include "CommandLine.h"

#include <charconv>
#include <format>
#include <system_error>

namespace apex::game {

namespace {

[[nodiscard]] std::optional<std::uint64_t> parsePositive(std::string_view text) noexcept {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
        return std::nullopt;
    }
    return value;
}

} // namespace

core::Result<CommandLineOptions> parseCommandLine(std::span<const std::string_view> arguments) {
    CommandLineOptions options;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        if (argument == "--no-vsync") {
            options.vsync = false;
        } else if (argument == "--frames") {
            if (index + 1 == arguments.size()) {
                return core::makeError(core::ErrorCode::InvalidArgument, "--frames needs a frame count");
            }
            const std::string_view count = arguments[++index];
            const std::optional<std::uint64_t> frames = parsePositive(count);
            if (!frames) {
                return core::makeError(core::ErrorCode::InvalidArgument,
                                       std::format("--frames needs a positive integer, got '{}'", count));
            }
            options.maxFrames = *frames;
        } else if (argument == "--benchmark") {
            if (index + 1 == arguments.size()) {
                return core::makeError(core::ErrorCode::InvalidArgument,
                                       "--benchmark needs a duration in seconds");
            }
            const std::string_view duration = arguments[++index];
            const std::optional<std::uint64_t> seconds = parsePositive(duration);
            if (!seconds) {
                return core::makeError(
                    core::ErrorCode::InvalidArgument,
                    std::format("--benchmark needs a positive number of seconds, got '{}'", duration));
            }
            options.benchmarkSeconds = *seconds;
        } else if (argument == "--assets" || argument == "--model" || argument == "--scene" ||
                   argument == "--config") {
            if (index + 1 == arguments.size() || arguments[index + 1].empty()) {
                return core::makeError(core::ErrorCode::InvalidArgument,
                                       std::format("{} needs a path", argument));
            }
            const std::string path(arguments[++index]);
            if (argument == "--assets") {
                options.assetsDirectory = path;
            } else if (argument == "--model") {
                options.extraModel = path;
            } else if (argument == "--scene") {
                options.scene = path;
            } else {
                options.configDirectory = path;
            }
        } else {
            return core::makeError(
                core::ErrorCode::InvalidArgument,
                std::format("Unknown option '{}'. Options: --frames N, --no-vsync, --benchmark S, "
                            "--assets DIR, --model PATH, --scene PATH, --config DIR",
                            argument));
        }
    }
    return options;
}

} // namespace apex::game
