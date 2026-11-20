#include "Physics/PhysicsWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <utility>

using namespace apex;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kStep = 1.0 / 60.0;

} // namespace

TEST_CASE("A physics world starts empty, with the gravity of the spec", "[physics]") {
    auto runtime = physics::Runtime::create();
    REQUIRE(runtime.has_value());
    auto world = physics::PhysicsWorld::create(*runtime, {});
    REQUIRE(world.has_value());

    REQUIRE((*world)->bodyCount() == 0);
    const glm::vec3 gravity = (*world)->gravity();
    REQUIRE(gravity.x == 0.0f);
    REQUIRE_THAT(gravity.y, WithinAbs(-9.81, 1e-6));
    REQUIRE(gravity.z == 0.0f);

    // Stepping an empty world is fine.
    for (int i = 0; i < 10; ++i) {
        (*world)->step(kStep);
    }
    REQUIRE((*world)->bodyCount() == 0);
}

TEST_CASE("Gravity comes from the settings", "[physics]") {
    auto runtime = physics::Runtime::create();
    REQUIRE(runtime.has_value());
    auto world = physics::PhysicsWorld::create(*runtime, {.gravity = {0.0f, -1.62f, 0.0f}});
    REQUIRE(world.has_value());
    REQUIRE_THAT((*world)->gravity().y, WithinAbs(-1.62, 1e-6));
}

TEST_CASE("A physics world refuses empty capacities", "[physics]") {
    auto runtime = physics::Runtime::create();
    REQUIRE(runtime.has_value());
    const auto world = physics::PhysicsWorld::create(*runtime, {.maxBodies = 0});
    REQUIRE_FALSE(world.has_value());
    REQUIRE(world.error().code == core::ErrorCode::InvalidArgument);
}

TEST_CASE("The physics runtime and its worlds can be created again after being destroyed", "[physics]") {
    for (int round = 0; round < 3; ++round) {
        auto runtime = physics::Runtime::create();
        REQUIRE(runtime.has_value());
        auto first = physics::PhysicsWorld::create(*runtime, {});
        auto second = physics::PhysicsWorld::create(*runtime, {});
        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        (*first)->step(kStep);
        (*second)->step(kStep);
    }
}

TEST_CASE("A moved physics runtime keeps Jolt alive", "[physics]") {
    auto created = physics::Runtime::create();
    REQUIRE(created.has_value());
    physics::Runtime runtime = std::move(*created);
    auto world = physics::PhysicsWorld::create(runtime, {});
    REQUIRE(world.has_value());
    (*world)->step(kStep);
}
