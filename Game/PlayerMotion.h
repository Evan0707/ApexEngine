#pragma once

#include "Core/Math.h"

namespace apex::game {

// Spec 5.8. Speeds in meters per second, accelerations in meters per second squared.
struct PlayerSettings {
    float walkSpeed = 4.0f;
    float runSpeed = 7.0f;
    // Meters, feet above the ground at the top of a jump.
    float jumpHeight = 1.2f;
    // Reaching the walk speed takes 0.2 s, stopping from it 0.16 s.
    float acceleration = 20.0f;
    float braking = 25.0f;
    // Share of the two values above that applies in the air.
    float airControl = 0.35f;
    // Radians per second: how fast the character turns to face where it goes.
    float turnSpeed = glm::radians(540.0f);
};

// One fixed step of input, read from the simulation's action state.
struct PlayerInput {
    // Axes in [-1, 1], relative to the camera: forward is where the camera looks, flattened on the ground.
    float forward = 0.0f;
    float right = 0.0f;
    bool sprint = false;
    // True on the step where the jump action was pressed.
    bool jump = false;
    // Radians; yaw 0 looks towards -Z, a positive yaw turns left.
    float cameraYaw = 0.0f;
};

// What the physics reported after the previous step.
struct PlayerSupport {
    bool onGround = false;
    // Velocity of what the character stands on; zero on static ground.
    glm::vec3 groundVelocity{0.0f};
};

struct PlayerState {
    // The character's own velocity: x and z change progressively, y carries the jump and the fall.
    glm::vec3 velocity{0.0f};
    // Radians, same convention as the camera's yaw.
    float facingYaw = 0.0f;
};

// The vertical speed that lifts the feet by exactly `height` meters when the motion advances by steps of
// `deltaSeconds` under a gravity of `gravity` m/s² (positive): a little more than sqrt(2 g h).
[[nodiscard]] float jumpSpeed(float height, float gravity, double deltaSeconds) noexcept;

// One fixed step (G-05) of the rules of spec 5.8: movement relative to the camera, progressive
// acceleration and braking, jump only on the ground, turn towards the movement. `gravity` is the world's,
// along Y. Diagonal moves are not faster.
[[nodiscard]] PlayerState stepPlayer(const PlayerState& state, const PlayerInput& input,
                                     const PlayerSupport& support, const glm::vec3& gravity,
                                     double deltaSeconds, const PlayerSettings& settings) noexcept;

// The velocity to give Physics for the next step: the character's own, on top of its ground's.
[[nodiscard]] glm::vec3 playerMotion(const PlayerState& state, const PlayerSupport& support) noexcept;

// Rotation of a character facing `yaw`: its -Z points where it goes.
[[nodiscard]] glm::quat playerOrientation(float yaw) noexcept;

} // namespace apex::game
