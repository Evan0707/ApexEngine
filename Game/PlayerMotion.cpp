#include "PlayerMotion.h"

#include <algorithm>
#include <cmath>

namespace apex::game {

namespace {

// Under this speed, in meters per second, the character has no direction to face.
constexpr float kFacingSpeed = 0.1f;

// Moves `current` towards `target` by at most `maxChange`.
[[nodiscard]] glm::vec2 approach(const glm::vec2& current, const glm::vec2& target,
                                 float maxChange) noexcept {
    const glm::vec2 difference = target - current;
    const float distance = glm::length(difference);
    if (distance <= maxChange || distance < 1e-6f) {
        return target;
    }
    return current + difference * (maxChange / distance);
}

// The shortest signed angle from `from` to `to`, in (-pi, pi].
[[nodiscard]] float shortestAngle(float from, float to) noexcept {
    float angle = std::fmod(to - from, glm::two_pi<float>());
    if (angle > glm::pi<float>()) {
        angle -= glm::two_pi<float>();
    } else if (angle <= -glm::pi<float>()) {
        angle += glm::two_pi<float>();
    }
    return angle;
}

} // namespace

float jumpSpeed(float height, float gravity, double deltaSeconds) noexcept {
    // Step k lifts the feet by (v - k g dt) dt: the top is at v²/(2g) - v dt/2, solved here for v.
    const float halfStep = 0.5f * gravity * static_cast<float>(deltaSeconds);
    return halfStep + std::sqrt(halfStep * halfStep + 2.0f * gravity * height);
}

PlayerState stepPlayer(const PlayerState& state, const PlayerInput& input, const PlayerSupport& support,
                       const glm::vec3& gravity, double deltaSeconds,
                       const PlayerSettings& settings) noexcept {
    const auto dt = static_cast<float>(deltaSeconds);

    // Camera-relative direction on the ground plane; x and z of the world in a vec2.
    const glm::vec2 forward{-std::sin(input.cameraYaw), -std::cos(input.cameraYaw)};
    const glm::vec2 right{std::cos(input.cameraYaw), -std::sin(input.cameraYaw)};
    glm::vec2 direction = forward * input.forward + right * input.right;
    if (const float length = glm::length(direction); length > 1.0f) {
        direction /= length;
    }
    const glm::vec2 target = direction * (input.sprint ? settings.runSpeed : settings.walkSpeed);

    const bool moving = glm::length(target) > 1e-4f;
    const float control = support.onGround ? 1.0f : settings.airControl;
    const float rate = (moving ? settings.acceleration : settings.braking) * control;
    const glm::vec2 horizontal = approach({state.velocity.x, state.velocity.z}, target, rate * dt);

    // On the ground the fall restarts from zero each step; in the air it accumulates.
    float vertical = support.onGround ? 0.0f : state.velocity.y;
    if (support.onGround && input.jump) {
        vertical = jumpSpeed(settings.jumpHeight, -gravity.y, deltaSeconds);
    }
    vertical += gravity.y * dt;

    PlayerState next{.velocity = {horizontal.x, vertical, horizontal.y}, .facingYaw = state.facingYaw};
    if (glm::length(horizontal) > kFacingSpeed) {
        // Yaw a faces (-sin a, -cos a).
        const float wanted = std::atan2(-horizontal.x, -horizontal.y);
        const float turn = shortestAngle(state.facingYaw, wanted);
        const float maxTurn = settings.turnSpeed * dt;
        next.facingYaw = state.facingYaw + std::clamp(turn, -maxTurn, maxTurn);
    }
    return next;
}

glm::vec3 playerMotion(const PlayerState& state, const PlayerSupport& support) noexcept {
    return support.onGround ? state.velocity + support.groundVelocity : state.velocity;
}

glm::quat playerOrientation(float yaw) noexcept {
    return glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
}

} // namespace apex::game
