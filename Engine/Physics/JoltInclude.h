#pragma once

// Internal to Physics: the first Jolt header of every source of the module, and the conversions between GLM
// and Jolt (spec 5.7). No public header of the module includes it (decision 0018).

#include "Core/Math.h"

// Jolt.h must come before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

namespace apex::physics {

[[nodiscard]] inline JPH::Vec3 toJolt(const glm::vec3& value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline JPH::Quat toJolt(const glm::quat& value) noexcept {
    return {value.x, value.y, value.z, value.w};
}

[[nodiscard]] inline glm::vec3 toGlm(JPH::Vec3Arg value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

[[nodiscard]] inline glm::quat toGlm(JPH::QuatArg value) noexcept {
    return glm::quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
}

} // namespace apex::physics
