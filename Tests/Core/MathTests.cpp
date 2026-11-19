#include "Core/Math.h"
#include "Core/WorldPosition.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <type_traits>

using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-6f;

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

} // namespace

TEST_CASE("The world axes form a right-handed frame with +Y up", "[core]") {
    using namespace apex::core;
    requireNear(glm::cross(kWorldRight, kWorldUp), glm::vec3(0.0f, 0.0f, 1.0f));
    requireNear(glm::cross(kWorldUp, -kWorldForward), kWorldRight);
}

TEST_CASE("A positive rotation around +Y turns forward to the left", "[core]") {
    // Counter-clockwise seen from above, as in glTF and Jolt (spec 3.3).
    const glm::quat turn = glm::angleAxis(glm::half_pi<float>(), apex::core::kWorldUp);
    requireNear(turn * apex::core::kWorldForward, -apex::core::kWorldRight);
}

TEST_CASE("Angles are in radians", "[core]") {
    REQUIRE_THAT(glm::radians(180.0f), WithinAbs(glm::pi<float>(), kTolerance));
    REQUIRE_THAT(glm::cos(glm::pi<float>()), WithinAbs(-1.0f, kTolerance));
}

TEST_CASE("WorldPosition is a vector of three floats in meters", "[core]") {
    STATIC_REQUIRE(std::is_same_v<apex::core::WorldPosition, glm::vec3>);
    const apex::core::WorldPosition position{1.0f, 2.0f, 3.0f};
    REQUIRE(glm::length(position - apex::core::WorldPosition{1.0f, 2.0f, 0.0f}) == 3.0f);
}
