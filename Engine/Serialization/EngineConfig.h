#pragma once

#include "Core/Error.h"
#include "Core/Math.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace apex::serialization {

// The only configuration file version this build reads (spec 3.1).
inline constexpr int kConfigVersion = 1;

// Config/engine.json (spec 5.2, 5.3, 5.7, decisions 0017 and 0019). The default values are those of the
// specification: a field left out of the file keeps them.
struct EngineConfig {
    // Size of the window's drawing area at start, in pixels.
    std::uint32_t windowWidth = 1920;
    std::uint32_t windowHeight = 1080;
    // Free camera: meters per second, factor while sprinting, radians per pixel of mouse motion.
    float freeCameraSpeed = 5.0f;
    float freeCameraSprintMultiplier = 4.0f;
    float freeCameraLookSensitivity = 0.0025f;
    // Physics: meters per second squared (spec 5.7).
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    // Character (spec 5.8): meters, meters per second, degrees, newtons. The height is the whole capsule's.
    float characterHeight = 1.8f;
    float characterRadius = 0.3f;
    float characterWalkSpeed = 4.0f;
    float characterRunSpeed = 7.0f;
    float characterJumpHeight = 1.2f;
    float characterMaxSlopeDegrees = 45.0f;
    float characterStepHeight = 0.3f;
    float characterPushForce = 100.0f;
    // Third-person camera (spec 5.3): meters behind the aim point, and height of the aim point above the
    // character's feet.
    float thirdPersonCameraDistance = 4.0f;
    float thirdPersonCameraAimHeight = 1.6f;

    [[nodiscard]] bool operator==(const EngineConfig&) const noexcept = default;
};

// An error names the file (`sourceName`) and the field. Unknown fields are errors.
[[nodiscard]] core::Result<EngineConfig> parseEngineConfig(std::string_view text,
                                                           std::string_view sourceName);

// NotFound when the file does not exist: the caller then decides to use EngineConfig{}.
[[nodiscard]] core::Result<EngineConfig> loadEngineConfigFile(const std::filesystem::path& file,
                                                              std::string_view sourceName);

} // namespace apex::serialization
