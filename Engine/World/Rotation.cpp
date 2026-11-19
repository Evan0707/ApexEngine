#include "World/Rotation.h"

#include <algorithm>
#include <cmath>

namespace apex::world {

glm::quat rotationFromEulerDegrees(const glm::vec3& degrees) noexcept {
    const glm::vec3 radians = glm::radians(degrees);
    return glm::angleAxis(radians.y, core::kWorldUp) * glm::angleAxis(radians.x, core::kWorldRight) *
           glm::angleAxis(radians.z, glm::vec3(0.0f, 0.0f, 1.0f));
}

glm::vec3 eulerDegreesFromRotation(const glm::quat& rotation) noexcept {
    // R = Ry(yaw) * Rx(pitch) * Rz(roll). With r(row, column) = matrix[column][row]:
    //   r(1,2) = -sin(pitch); r(0,2) = sin(yaw) cos(pitch); r(2,2) = cos(yaw) cos(pitch);
    //   r(1,0) = cos(pitch) sin(roll); r(1,1) = cos(pitch) cos(roll).
    const glm::mat3 matrix = glm::mat3_cast(glm::normalize(rotation));
    const float sinPitch = std::clamp(-matrix[2][1], -1.0f, 1.0f);
    const float pitch = std::asin(sinPitch);
    float yaw = 0.0f;
    float roll = 0.0f;
    constexpr float kGimbalLock = 0.99999f;
    if (std::abs(sinPitch) < kGimbalLock) {
        yaw = std::atan2(matrix[2][0], matrix[2][2]);
        roll = std::atan2(matrix[0][1], matrix[1][1]);
    } else {
        // Looking straight up or down: with roll = 0, r(0,0) = cos(yaw) and r(2,0) = -sin(yaw).
        yaw = std::atan2(-matrix[0][2], matrix[0][0]);
    }
    return glm::degrees(glm::vec3(pitch, yaw, roll));
}

} // namespace apex::world
