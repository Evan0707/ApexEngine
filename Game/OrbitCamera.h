#pragma once

#include "Core/Math.h"
#include "Core/WorldPosition.h"

#include <optional>

namespace apex::game {

// Third-person camera (spec 5.3): orbits around a point above the character.
struct OrbitCameraSettings {
    // Meters: from the aim point back to the camera, and from the character's feet up to the aim point.
    float distance = 4.0f;
    float aimHeight = 1.6f;
    // Radians per pixel of mouse motion.
    float lookSensitivity = 0.0025f;
    // Radians. A negative pitch looks down, from above the character.
    float minPitch = glm::radians(-80.0f);
    float maxPitch = glm::radians(60.0f);
    // Meters: radius of the sphere swept from the aim point to the camera (decision 0019). Larger than the
    // camera's near plane, so that the view never cuts into what stopped the sphere.
    float probeRadius = 0.2f;
    // Seconds: the aim point covers 63 % of its lag in this time.
    float followTime = 0.08f;
    // Meters per second: how fast the camera backs away again once an obstacle is gone.
    float extendSpeed = 6.0f;
};

// Angles in radians, with the free camera's convention: yaw 0 looks towards -Z, a positive yaw turns left,
// a positive pitch looks up.
struct OrbitCameraState {
    float yaw = 0.0f;
    float pitch = 0.0f;
    // The smoothed aim point: what the camera looks at.
    core::WorldPosition target{0.0f};
    // Meters between the aim point and the camera, shortened by obstacles.
    float distance = 4.0f;
};

// Mouse motion in pixels since the previous step, +Y downwards.
struct OrbitCameraInput {
    float lookX = 0.0f;
    float lookY = 0.0f;
};

// First half of a fixed step (G-05): turns with the mouse, and moves the aim point towards `wantedTarget`
// without jerks.
[[nodiscard]] OrbitCameraState aimOrbitCamera(const OrbitCameraState& state, const OrbitCameraInput& input,
                                              const core::WorldPosition& wantedTarget, double deltaSeconds,
                                              const OrbitCameraSettings& settings) noexcept;

// Second half: the distance to keep. `freeDistance` is how far a sphere swept from the aim point towards
// the camera's place could go; empty when nothing is in the way. The camera comes closer at once and backs
// away progressively.
[[nodiscard]] float orbitCameraDistance(float currentDistance, std::optional<float> freeDistance,
                                        double deltaSeconds, const OrbitCameraSettings& settings) noexcept;

// Unit vector from the camera to the aim point.
[[nodiscard]] glm::vec3 orbitCameraForward(const OrbitCameraState& state) noexcept;
[[nodiscard]] core::WorldPosition orbitCameraPosition(const OrbitCameraState& state) noexcept;
// Camera to world, looking at the aim point.
[[nodiscard]] glm::quat orbitCameraOrientation(const OrbitCameraState& state) noexcept;

} // namespace apex::game
