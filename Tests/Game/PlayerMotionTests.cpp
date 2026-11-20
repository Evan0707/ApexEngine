#include "PlayerMotion.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using namespace apex;
using namespace apex::game;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kStep = 1.0 / 60.0;
constexpr glm::vec3 kGravity{0.0f, -9.81f, 0.0f};
constexpr PlayerSupport kOnGround{.onGround = true};
constexpr PlayerSupport kInAir{.onGround = false};

[[nodiscard]] PlayerState run(PlayerState state, const PlayerInput& input, const PlayerSupport& support,
                              double seconds, const PlayerSettings& settings = {}) {
    const int steps = static_cast<int>(seconds / kStep + 0.5);
    for (int i = 0; i < steps; ++i) {
        state = stepPlayer(state, input, support, kGravity, kStep, settings);
    }
    return state;
}

[[nodiscard]] float horizontalSpeed(const PlayerState& state) {
    return glm::length(glm::vec2(state.velocity.x, state.velocity.z));
}

} // namespace

TEST_CASE("The jump speed lifts the feet by the jump height, step by step", "[game]") {
    const float speed = jumpSpeed(1.2f, 9.81f, kStep);
    // A little more than the continuous sqrt(2 g h) = 4.852.
    REQUIRE(speed > 4.852f);
    REQUIRE(speed < 5.0f);

    // The same integration as the character's: velocity first, then position.
    float velocity = speed;
    float height = 0.0f;
    float highest = 0.0f;
    for (int i = 0; i < 120; ++i) {
        velocity += kGravity.y * static_cast<float>(kStep);
        height += velocity * static_cast<float>(kStep);
        highest = std::max(highest, height);
    }
    REQUIRE_THAT(highest, WithinAbs(1.2, 0.002));
}

TEST_CASE("The player accelerates progressively up to the walk speed, or the run speed", "[game]") {
    const PlayerInput forward{.forward = 1.0f};
    // 20 m/s² : 2 m/s after a tenth of a second.
    REQUIRE_THAT(horizontalSpeed(run({}, forward, kOnGround, 0.1)), WithinAbs(2.0, 0.05));
    REQUIRE_THAT(horizontalSpeed(run({}, forward, kOnGround, 1.0)), WithinAbs(4.0, 1e-4));
    REQUIRE_THAT(horizontalSpeed(run({}, {.forward = 1.0f, .sprint = true}, kOnGround, 1.0)),
                 WithinAbs(7.0, 1e-4));
}

TEST_CASE("The player brakes progressively to a stop, without going backwards", "[game]") {
    const PlayerState walking = run({}, {.forward = 1.0f}, kOnGround, 1.0);
    // 25 m/s² : 4 m/s are lost in 0.16 s.
    const PlayerState slowing = run(walking, {}, kOnGround, 0.08);
    REQUIRE_THAT(horizontalSpeed(slowing), WithinAbs(2.0, 0.15));
    REQUIRE(slowing.velocity.z < 0.0f);
    const PlayerState stopped = run(walking, {}, kOnGround, 0.3);
    REQUIRE(stopped.velocity.x == 0.0f);
    REQUIRE(stopped.velocity.z == 0.0f);
}

TEST_CASE("The player moves relative to the camera", "[game]") {
    // Camera looking towards -Z.
    const PlayerState ahead = run({}, {.forward = 1.0f}, kOnGround, 1.0);
    REQUIRE_THAT(ahead.velocity.z, WithinAbs(-4.0, 1e-4));
    REQUIRE_THAT(ahead.velocity.x, WithinAbs(0.0, 1e-4));
    const PlayerState toTheRight = run({}, {.right = 1.0f}, kOnGround, 1.0);
    REQUIRE_THAT(toTheRight.velocity.x, WithinAbs(4.0, 1e-4));

    // Camera turned a quarter turn to the left: it looks towards -X, and its right is -Z.
    const float quarter = glm::radians(90.0f);
    const PlayerState left = run({}, {.forward = 1.0f, .cameraYaw = quarter}, kOnGround, 1.0);
    REQUIRE_THAT(left.velocity.x, WithinAbs(-4.0, 1e-4));
    REQUIRE_THAT(left.velocity.z, WithinAbs(0.0, 1e-4));
    const PlayerState strafing = run({}, {.right = 1.0f, .cameraYaw = quarter}, kOnGround, 1.0);
    REQUIRE_THAT(strafing.velocity.z, WithinAbs(-4.0, 1e-4));
}

TEST_CASE("The player is not faster when it moves diagonally", "[game]") {
    const PlayerState diagonal = run({}, {.forward = 1.0f, .right = 1.0f}, kOnGround, 1.0);
    REQUIRE_THAT(horizontalSpeed(diagonal), WithinAbs(4.0, 1e-4));
    // A half-pressed axis walks slower.
    REQUIRE_THAT(horizontalSpeed(run({}, {.forward = 0.5f}, kOnGround, 1.0)), WithinAbs(2.0, 1e-4));
}

TEST_CASE("The player jumps only from the ground", "[game]") {
    const float expected = jumpSpeed(1.2f, 9.81f, kStep) + kGravity.y * static_cast<float>(kStep);
    const PlayerState jumped = stepPlayer({}, {.jump = true}, kOnGround, kGravity, kStep, {});
    REQUIRE_THAT(jumped.velocity.y, WithinAbs(expected, 1e-5));

    // In the air the jump is ignored: only gravity changes the vertical speed.
    const PlayerState falling{.velocity = {0.0f, -2.0f, 0.0f}};
    const PlayerState ignored = stepPlayer(falling, {.jump = true}, kInAir, kGravity, kStep, {});
    REQUIRE_THAT(ignored.velocity.y, WithinAbs(-2.0 + kGravity.y * kStep, 1e-5));
}

TEST_CASE("The fall accumulates in the air and restarts on the ground", "[game]") {
    const PlayerState falling = run({}, {}, kInAir, 1.0);
    REQUIRE_THAT(falling.velocity.y, WithinAbs(-9.81, 1e-3));
    // Landed: one step of gravity only, which keeps the character pressed on the ground.
    const PlayerState landed = stepPlayer(falling, {}, kOnGround, kGravity, kStep, {});
    REQUIRE_THAT(landed.velocity.y, WithinAbs(kGravity.y * kStep, 1e-5));
}

TEST_CASE("The player has less control in the air", "[game]") {
    const PlayerState onGround = run({}, {.forward = 1.0f}, kOnGround, 0.1);
    const PlayerState inAir = run({}, {.forward = 1.0f}, kInAir, 0.1);
    REQUIRE(horizontalSpeed(inAir) > 0.3f);
    REQUIRE_THAT(horizontalSpeed(inAir), WithinAbs(0.35 * horizontalSpeed(onGround), 0.05));
    // The speed it had when leaving the ground is kept.
    const PlayerState walking = run({}, {.forward = 1.0f}, kOnGround, 1.0);
    REQUIRE_THAT(horizontalSpeed(run(walking, {.forward = 1.0f}, kInAir, 0.5)), WithinAbs(4.0, 1e-4));
}

TEST_CASE("The player turns to face where it goes, at a limited rate and by the shortest way", "[game]") {
    // Facing -Z, asked to go towards +X (to the right of the camera): the yaw goes down to -90 degrees.
    const PlayerState early = run({}, {.right = 1.0f}, kOnGround, 0.05);
    REQUIRE(early.facingYaw < 0.0f);
    REQUIRE(early.facingYaw > glm::radians(-90.0f));
    const PlayerState turned = run({}, {.right = 1.0f}, kOnGround, 1.0);
    REQUIRE_THAT(turned.facingYaw, WithinAbs(glm::radians(-90.0f), 1e-3));

    // From 170 degrees to -170 degrees: 20 degrees through the back, not 340 through the front.
    PlayerState state{.velocity = {0.0f, 0.0f, 0.0f}, .facingYaw = glm::radians(170.0f)};
    state = run(state, {.forward = 1.0f, .cameraYaw = glm::radians(-170.0f)}, kOnGround, 1.0);
    REQUIRE_THAT(state.facingYaw, WithinAbs(glm::radians(190.0f), 1e-2));

    // Stopped: the facing stays.
    const PlayerState resting = run(turned, {}, kOnGround, 1.0);
    REQUIRE(resting.facingYaw == run(turned, {}, kOnGround, 0.3).facingYaw);
}

TEST_CASE("The motion given to the physics adds the velocity of the ground under the player", "[game]") {
    const PlayerState state{.velocity = {1.0f, -0.1f, 2.0f}};
    const PlayerSupport onPlatform{.onGround = true, .groundVelocity = {0.5f, 0.0f, 0.0f}};
    REQUIRE(playerMotion(state, onPlatform) == glm::vec3(1.5f, -0.1f, 2.0f));
    REQUIRE(playerMotion(state, {.onGround = false, .groundVelocity = {0.5f, 0.0f, 0.0f}}) == state.velocity);
}

TEST_CASE("The player's orientation points its -Z where it faces", "[game]") {
    const glm::vec3 forward = playerOrientation(glm::radians(90.0f)) * core::kWorldForward;
    REQUIRE_THAT(forward.x, WithinAbs(-1.0, 1e-5));
    REQUIRE_THAT(forward.z, WithinAbs(0.0, 1e-5));
}
