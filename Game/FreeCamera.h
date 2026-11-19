#pragma once

#include "Core/Math.h"
#include "Core/WorldPosition.h"

namespace apex::game {

// Spec 5.3.
struct FreeCameraSettings {
    // Meters per second.
    float speed = 5.0f;
    float sprintMultiplier = 4.0f;
    // Radians per pixel of mouse motion.
    float lookSensitivity = 0.0025f;
    float minVerticalFovDegrees = 50.0f;
    float maxVerticalFovDegrees = 100.0f;
    float defaultVerticalFovDegrees = 70.0f;
};

// Radians. Yaw 0 looks towards -Z; a positive yaw turns left (counter-clockwise seen from above). A
// positive pitch looks up. Yaw is not wrapped.
struct FreeCameraState {
    core::WorldPosition position{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

// One fixed step of input, read from the simulation's action state.
struct FreeCameraInput {
    // Axes in [-1, 1]: forward along the view direction, right, up along world +Y.
    float forward = 0.0f;
    float right = 0.0f;
    float up = 0.0f;
    bool sprint = false;
    // Mouse motion in pixels since the previous step, +Y downwards.
    float lookX = 0.0f;
    float lookY = 0.0f;
};

// Pitch stays inside (-90, 90) degrees: the view never flips over the vertical.
inline constexpr float kMaxCameraPitch = glm::radians(89.0f);

// Advances by one fixed step (G-05): turns, then moves. Diagonal moves are not faster.
[[nodiscard]] FreeCameraState stepFreeCamera(const FreeCameraState& state, const FreeCameraInput& input,
                                             double deltaSeconds,
                                             const FreeCameraSettings& settings) noexcept;

// Camera to world: yaw around world +Y, then pitch around the camera's right axis.
[[nodiscard]] glm::quat freeCameraOrientation(const FreeCameraState& state) noexcept;

[[nodiscard]] float clampVerticalFov(float degrees, const FreeCameraSettings& settings) noexcept;

} // namespace apex::game
