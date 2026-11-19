#include "World/Rotation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using apex::world::eulerDegreesFromRotation;
using apex::world::rotationFromEulerDegrees;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-4f;

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

// Two quaternions describe the same orientation when they are equal or opposite.
void requireSameOrientation(const glm::quat& actual, const glm::quat& expected) {
    REQUIRE_THAT(std::abs(glm::dot(actual, expected)), WithinAbs(1.0, kTolerance));
}

} // namespace

TEST_CASE("A positive yaw turns left, a positive pitch looks up", "[world]") {
    requireNear(rotationFromEulerDegrees({0.0f, 90.0f, 0.0f}) * apex::core::kWorldForward,
                {-1.0f, 0.0f, 0.0f});
    const glm::vec3 lookingUp = rotationFromEulerDegrees({45.0f, 0.0f, 0.0f}) * apex::core::kWorldForward;
    REQUIRE(lookingUp.y > 0.0f);
    REQUIRE(lookingUp.z < 0.0f);
}

TEST_CASE("A positive roll tilts the right side up", "[world]") {
    requireNear(rotationFromEulerDegrees({0.0f, 0.0f, 90.0f}) * apex::core::kWorldRight, {0.0f, 1.0f, 0.0f});
}

TEST_CASE("Yaw applies around the world's up, whatever the pitch", "[world]") {
    // Order Y, X, Z: pitched straight up first, the yaw then leaves the view pointing up.
    requireNear(rotationFromEulerDegrees({90.0f, 90.0f, 0.0f}) * apex::core::kWorldForward,
                {0.0f, 1.0f, 0.0f});
    // The same convention as the free camera: yaw around +Y, then pitch around the camera's right.
    const glm::quat camera = glm::angleAxis(glm::radians(30.0f), apex::core::kWorldUp) *
                             glm::angleAxis(glm::radians(-15.0f), apex::core::kWorldRight);
    requireSameOrientation(rotationFromEulerDegrees({-15.0f, 30.0f, 0.0f}), camera);
}

TEST_CASE("Angles survive a trip through a quaternion", "[world]") {
    for (const glm::vec3 degrees :
         {glm::vec3(0.0f), glm::vec3(-15.0f, 30.0f, 0.0f), glm::vec3(20.0f, -120.0f, 45.0f),
          glm::vec3(-80.0f, 170.0f, -60.0f), glm::vec3(0.0f, 45.0f, 0.0f)}) {
        CAPTURE(degrees.x, degrees.y, degrees.z);
        requireNear(eulerDegreesFromRotation(rotationFromEulerDegrees(degrees)), degrees);
    }
}

TEST_CASE("Looking straight up or down keeps the orientation, with no roll", "[world]") {
    for (const float pitch : {90.0f, -90.0f}) {
        const glm::quat original = rotationFromEulerDegrees({pitch, 30.0f, 20.0f});
        const glm::vec3 degrees = eulerDegreesFromRotation(original);
        REQUIRE_THAT(degrees.x, WithinAbs(pitch, 0.05));
        REQUIRE(degrees.z == 0.0f);
        // Close to the pole a small angle error is a large quaternion error: looser tolerance.
        REQUIRE_THAT(std::abs(glm::dot(rotationFromEulerDegrees(degrees), original)), WithinAbs(1.0, 1e-3));
    }
}
