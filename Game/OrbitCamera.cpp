#include "OrbitCamera.h"

#include <algorithm>
#include <cmath>

namespace apex::game {

OrbitCameraState aimOrbitCamera(const OrbitCameraState& state, const OrbitCameraInput& input,
                                const core::WorldPosition& wantedTarget, double deltaSeconds,
                                const OrbitCameraSettings& settings) noexcept {
    OrbitCameraState next = state;
    // Mouse right turns right (negative yaw); mouse down looks down.
    next.yaw -= input.lookX * settings.lookSensitivity;
    next.pitch =
        std::clamp(next.pitch - input.lookY * settings.lookSensitivity, settings.minPitch, settings.maxPitch);

    // Exponential approach: the same result whatever the number of steps a duration is cut into.
    const float follow = settings.followTime > 0.0f
                             ? 1.0f - std::exp(-static_cast<float>(deltaSeconds) / settings.followTime)
                             : 1.0f;
    next.target = state.target + (wantedTarget - state.target) * follow;
    return next;
}

float orbitCameraDistance(float currentDistance, std::optional<float> freeDistance, double deltaSeconds,
                          const OrbitCameraSettings& settings) noexcept {
    const float allowed = std::clamp(freeDistance.value_or(settings.distance), 0.0f, settings.distance);
    if (allowed <= currentDistance) {
        return allowed;
    }
    return std::min(allowed, currentDistance + settings.extendSpeed * static_cast<float>(deltaSeconds));
}

glm::vec3 orbitCameraForward(const OrbitCameraState& state) noexcept {
    return orbitCameraOrientation(state) * core::kWorldForward;
}

core::WorldPosition orbitCameraPosition(const OrbitCameraState& state) noexcept {
    return state.target - orbitCameraForward(state) * state.distance;
}

glm::quat orbitCameraOrientation(const OrbitCameraState& state) noexcept {
    return glm::angleAxis(state.yaw, core::kWorldUp) * glm::angleAxis(state.pitch, core::kWorldRight);
}

} // namespace apex::game
