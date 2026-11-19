#pragma once

#include "Core/Math.h"

namespace apex::world {

// Scene files give rotations as angles in degrees around X, Y and Z (spec 3.3, decision 0017), applied in
// the order Y, X, Z: yaw around the world's up, then pitch, then roll. Radians everywhere else.

// (x, y, z) = (pitch, yaw, roll) in degrees.
[[nodiscard]] glm::quat rotationFromEulerDegrees(const glm::vec3& degrees) noexcept;

// The inverse: pitch in [-90, 90], yaw and roll in (-180, 180]. At a pitch of +-90 degrees yaw and roll
// turn around the same axis: the roll is reported as 0.
[[nodiscard]] glm::vec3 eulerDegreesFromRotation(const glm::quat& rotation) noexcept;

} // namespace apex::world
