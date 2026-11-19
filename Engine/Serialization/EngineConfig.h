#pragma once

#include "Core/Error.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace apex::serialization {

// The only configuration file version this build reads (spec 3.1).
inline constexpr int kConfigVersion = 1;

// Config/engine.json (spec 5.2, 5.3, decision 0017). The default values are those of the specification:
// a field left out of the file keeps them.
struct EngineConfig {
    // Size of the window's drawing area at start, in pixels.
    std::uint32_t windowWidth = 1920;
    std::uint32_t windowHeight = 1080;
    // Free camera: meters per second, factor while sprinting, radians per pixel of mouse motion.
    float freeCameraSpeed = 5.0f;
    float freeCameraSprintMultiplier = 4.0f;
    float freeCameraLookSensitivity = 0.0025f;

    [[nodiscard]] bool operator==(const EngineConfig&) const noexcept = default;
};

// An error names the file (`sourceName`) and the field. Unknown fields are errors.
[[nodiscard]] core::Result<EngineConfig> parseEngineConfig(std::string_view text,
                                                           std::string_view sourceName);

// NotFound when the file does not exist: the caller then decides to use EngineConfig{}.
[[nodiscard]] core::Result<EngineConfig> loadEngineConfigFile(const std::filesystem::path& file,
                                                              std::string_view sourceName);

} // namespace apex::serialization
