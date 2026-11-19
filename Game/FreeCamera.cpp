#include "FreeCamera.h"

#include <algorithm>

namespace apex::game {

FreeCameraState stepFreeCamera(const FreeCameraState& state, const FreeCameraInput& input,
                               double deltaSeconds, const FreeCameraSettings& settings) noexcept {
    FreeCameraState next = state;
    // Mouse right turns right (negative yaw); mouse down looks down.
    next.yaw -= input.lookX * settings.lookSensitivity;
    next.pitch =
        std::clamp(next.pitch - input.lookY * settings.lookSensitivity, -kMaxCameraPitch, kMaxCameraPitch);

    const glm::quat orientation = freeCameraOrientation(next);
    glm::vec3 move = orientation * core::kWorldForward * input.forward +
                     orientation * core::kWorldRight * input.right + core::kWorldUp * input.up;
    const float length = glm::length(move);
    if (length > 1.0f) {
        move /= length;
    }
    const float speed = settings.speed * (input.sprint ? settings.sprintMultiplier : 1.0f);
    next.position += move * speed * static_cast<float>(deltaSeconds);
    return next;
}

glm::quat freeCameraOrientation(const FreeCameraState& state) noexcept {
    return glm::angleAxis(state.yaw, core::kWorldUp) * glm::angleAxis(state.pitch, core::kWorldRight);
}

FreeCameraState interpolate(const FreeCameraState& previous, const FreeCameraState& current,
                            float alpha) noexcept {
    return FreeCameraState{
        .position = glm::mix(previous.position, current.position, alpha),
        .yaw = glm::mix(previous.yaw, current.yaw, alpha),
        .pitch = glm::mix(previous.pitch, current.pitch, alpha),
    };
}

float clampVerticalFov(float degrees, const FreeCameraSettings& settings) noexcept {
    return std::clamp(degrees, settings.minVerticalFovDegrees, settings.maxVerticalFovDegrees);
}

} // namespace apex::game
