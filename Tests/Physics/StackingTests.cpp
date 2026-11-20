#include "PhysicsTestWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>

using namespace apex;
using namespace apex::tests;
using Catch::Matchers::WithinAbs;
using physics::MotionType;
using physics::ShapeType;

namespace {

// Jolt lets resting bodies overlap by up to 2 cm (its penetration slop).
constexpr double kRestTolerance = 0.03;

// Steps the world and returns the lowest height the body reached.
[[nodiscard]] float lowestHeight(const PhysicsTestWorld& world, physics::BodyHandle body, double seconds) {
    float lowest = world.position(body).y;
    const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
    for (int i = 0; i < steps; ++i) {
        world->step(kPhysicsStep);
        lowest = std::min(lowest, world.position(body).y);
    }
    return lowest;
}

} // namespace

// AC-08.
TEST_CASE("A crate dropped from 10 m does not go through the floor", "[physics]") {
    const PhysicsTestWorld world;
    // As thick as the demo's floor.
    (void)world.addFloor(50.0f, 0.2f);
    const physics::BodyHandle crate = world.addBox({0.0f, 10.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);

    // At 14 m/s the crate sinks into the floor for a step before Jolt pushes it back: its center stays well
    // above the floor's top, and it comes to rest on it.
    REQUIRE(lowestHeight(world, crate, 5.0) > 0.25f);
    REQUIRE_THAT(world.position(crate).y, WithinAbs(0.5, kRestTolerance));
    REQUIRE_THAT(glm::length(world->linearVelocity(crate)), WithinAbs(0.0, 0.01));
}

// AC-08, worst case: 14 m/s is 23 cm per step, more than the body and the floor together.
TEST_CASE("A small ball dropped from 10 m does not go through a thin floor", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor(50.0f, 0.02f);
    const auto ball = world->createBody({.shape = {.type = ShapeType::Sphere, .radius = 0.05f},
                                         .motion = MotionType::Dynamic,
                                         .transform = {.position = {0.0f, 10.0f, 0.0f}},
                                         .mass = 0.5f});
    REQUIRE(ball.has_value());

    REQUIRE(lowestHeight(world, *ball, 5.0) > 0.0f);
    REQUIRE_THAT(world.position(*ball).y, WithinAbs(0.05, kRestTolerance));
}

// AC-09.
TEST_CASE("A stack of three crates stays in place for 10 s", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    std::array<physics::BodyHandle, 3> crates{};
    for (std::size_t i = 0; i < crates.size(); ++i) {
        crates[i] = world.addBox({2.0f, 0.5f + static_cast<float>(i), -3.0f}, glm::vec3(0.5f),
                                 MotionType::Dynamic, 20.0f);
    }

    world.stepSeconds(10.0);
    for (std::size_t i = 0; i < crates.size(); ++i) {
        const auto transform = world->transform(crates[i]);
        REQUIRE(transform.has_value());
        REQUIRE_THAT(transform->position.x, WithinAbs(2.0, 0.01));
        REQUIRE_THAT(transform->position.y, WithinAbs(0.5 + static_cast<double>(i), kRestTolerance));
        REQUIRE_THAT(transform->position.z, WithinAbs(-3.0, 0.01));
        // Still upright: the rotation is the identity, or its opposite quaternion.
        REQUIRE_THAT(std::abs(transform->rotation.w), WithinAbs(1.0, 1e-3));
    }
    // At rest, the whole stack sleeps.
    REQUIRE(world->activeBodyCount() == 0);
}

TEST_CASE("A stack built slightly askew and from a small height settles and stays", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    std::array<physics::BodyHandle, 3> crates{};
    for (std::size_t i = 0; i < crates.size(); ++i) {
        const float offset = 0.05f * static_cast<float>(i);
        const auto crate = world->createBody(
            {.shape = {.type = ShapeType::Box, .halfExtents = glm::vec3(0.5f)},
             .motion = MotionType::Dynamic,
             .transform = {.position = {offset, 0.55f + 1.05f * static_cast<float>(i), -offset},
                           .rotation =
                               glm::angleAxis(glm::radians(10.0f * static_cast<float>(i)), core::kWorldUp)},
             .mass = 20.0f});
        REQUIRE(crate.has_value());
        crates[i] = *crate;
    }

    world.stepSeconds(2.0);
    std::array<glm::vec3, 3> settled{};
    for (std::size_t i = 0; i < crates.size(); ++i) {
        settled[i] = world.position(crates[i]);
        REQUIRE_THAT(settled[i].y, WithinAbs(0.5 + static_cast<double>(i), 2.0 * kRestTolerance));
    }
    world.stepSeconds(10.0);
    for (std::size_t i = 0; i < crates.size(); ++i) {
        REQUIRE_THAT(glm::distance(world.position(crates[i]), settled[i]), WithinAbs(0.0, 0.01));
    }
}

TEST_CASE("A moving crate pushes another one and is stopped by a wall", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    // 10 cm thick, as the wall of the demo scene.
    const physics::BodyHandle wall =
        world.addBox({5.0f, 1.0f, 0.0f}, {0.05f, 1.0f, 3.0f}, MotionType::Static);
    const physics::BodyHandle pusher = world.addBox({0.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    const physics::BodyHandle pushed = world.addBox({2.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);

    world->setLinearVelocity(pusher, {12.0f, 0.0f, 0.0f});
    world.stepSeconds(4.0);

    REQUIRE(world.position(pushed).x > 2.2f);
    // Both stay on the near side of the wall.
    REQUIRE(world.position(pushed).x < 4.95f - 0.45f);
    REQUIRE(world.position(pusher).x < world.position(pushed).x);
    REQUIRE(world.position(wall) == glm::vec3(5.0f, 1.0f, 0.0f));
}
