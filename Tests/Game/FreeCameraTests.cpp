#include "FreeCamera.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace apex::game;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kStep = 1.0 / 60.0;
constexpr float kTolerance = 1e-4f;

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

[[nodiscard]] FreeCameraState runSteps(FreeCameraState state, const FreeCameraInput& input, int steps) {
    const FreeCameraSettings settings;
    for (int i = 0; i < steps; ++i) {
        state = stepFreeCamera(state, input, kStep, settings);
    }
    return state;
}

} // namespace

TEST_CASE("An unrotated free camera looks towards -Z", "[game]") {
    const glm::quat orientation = freeCameraOrientation(FreeCameraState{});
    requireNear(orientation * apex::core::kWorldForward, {0.0f, 0.0f, -1.0f});
}

TEST_CASE("The free camera moves 5 m in one second, 20 m when sprinting", "[game]") {
    const FreeCameraState walked = runSteps({}, {.forward = 1.0f}, 60);
    requireNear(walked.position, {0.0f, 0.0f, -5.0f});
    const FreeCameraState sprinted = runSteps({}, {.forward = 1.0f, .sprint = true}, 60);
    requireNear(sprinted.position, {0.0f, 0.0f, -20.0f});
}

TEST_CASE("Moving diagonally is not faster", "[game]") {
    const FreeCameraState moved = runSteps({}, {.forward = 1.0f, .right = 1.0f, .up = 1.0f}, 60);
    REQUIRE_THAT(glm::length(moved.position), WithinAbs(5.0, kTolerance));
}

TEST_CASE("Right and up follow the camera's right and the world's up", "[game]") {
    FreeCameraState looking;
    looking.pitch = glm::radians(45.0f);
    const FreeCameraState moved = runSteps(looking, {.right = 1.0f}, 60);
    requireNear(moved.position, {5.0f, 0.0f, 0.0f});
    const FreeCameraState climbed = runSteps(looking, {.up = 1.0f}, 60);
    requireNear(climbed.position, {0.0f, 5.0f, 0.0f});
}

TEST_CASE("Forward follows the view direction, pitch included", "[game]") {
    FreeCameraState state;
    state.pitch = glm::radians(90.0f) - 1e-3f;
    state = runSteps(state, {.forward = 1.0f}, 60);
    REQUIRE(state.position.y > 4.99f);
}

TEST_CASE("Mouse right turns right, mouse down looks down", "[game]") {
    const FreeCameraSettings settings;
    const FreeCameraState turned = stepFreeCamera({}, {.lookX = 100.0f, .lookY = 40.0f}, kStep, settings);
    REQUIRE(turned.yaw < 0.0f);
    REQUIRE(turned.pitch < 0.0f);
    const glm::vec3 facing = freeCameraOrientation(turned) * apex::core::kWorldForward;
    REQUIRE(facing.x > 0.0f);
    REQUIRE(facing.y < 0.0f);
}

TEST_CASE("Pitch is clamped short of the vertical", "[game]") {
    const FreeCameraSettings settings;
    const FreeCameraState up = stepFreeCamera({}, {.lookY = -1e6f}, kStep, settings);
    REQUIRE(up.pitch == kMaxCameraPitch);
    const FreeCameraState down = stepFreeCamera({}, {.lookY = 1e6f}, kStep, settings);
    REQUIRE(down.pitch == -kMaxCameraPitch);
}

TEST_CASE("The field of view stays between 50 and 100 degrees", "[game]") {
    const FreeCameraSettings settings;
    REQUIRE(clampVerticalFov(30.0f, settings) == 50.0f);
    REQUIRE(clampVerticalFov(70.0f, settings) == 70.0f);
    REQUIRE(clampVerticalFov(120.0f, settings) == 100.0f);
}
