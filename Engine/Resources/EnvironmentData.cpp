#include "Resources/EnvironmentData.h"

#include <algorithm>
#include <cmath>

namespace apex::resources {

glm::vec2 environmentUv(const glm::vec3& direction) noexcept {
    const float azimuth = std::atan2(direction.x, -direction.z);
    const float polar = std::acos(std::clamp(direction.y, -1.0f, 1.0f));
    return {0.5f + azimuth / glm::two_pi<float>(), polar / glm::pi<float>()};
}

glm::vec3 environmentDirection(const EnvironmentData& environment, std::uint32_t x,
                               std::uint32_t y) noexcept {
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(environment.width);
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(environment.height);
    const float azimuth = (u - 0.5f) * glm::two_pi<float>();
    const float polar = v * glm::pi<float>();
    return {std::sin(polar) * std::sin(azimuth), std::cos(polar), -std::sin(polar) * std::cos(azimuth)};
}

} // namespace apex::resources
