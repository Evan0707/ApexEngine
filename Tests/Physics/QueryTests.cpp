#include "PhysicsTestWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>

using namespace apex;
using namespace apex::tests;
using Catch::Matchers::WithinAbs;
using physics::MotionType;

namespace {

constexpr float kTolerance = 1e-3f;

void requireNear(const glm::vec3& actual, const glm::vec3& expected, float tolerance = kTolerance) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, tolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, tolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, tolerance));
}

} // namespace

TEST_CASE("A ray hits the closest body and reports where, how far and the surface normal", "[physics]") {
    const PhysicsTestWorld world;
    const physics::BodyHandle floor = world.addFloor();
    const physics::BodyHandle crate = world.addBox({0.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);

    const auto down = world->raycast({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f);
    REQUIRE(down.has_value());
    REQUIRE(down->body == crate);
    REQUIRE_THAT(down->distance, WithinAbs(4.0, kTolerance));
    requireNear(down->position, {0.0f, 1.0f, 0.0f});
    requireNear(down->contact, down->position);
    requireNear(down->normal, {0.0f, 1.0f, 0.0f});

    // Beside the crate, the same ray reaches the floor.
    const auto beside = world->raycast({3.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f);
    REQUIRE(beside.has_value());
    REQUIRE(beside->body == floor);
    REQUIRE_THAT(beside->distance, WithinAbs(5.0, kTolerance));

    // From the side, along -X: the normal faces the ray's origin.
    const auto side = world->raycast({5.0f, 0.5f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 100.0f);
    REQUIRE(side.has_value());
    REQUIRE(side->body == crate);
    requireNear(side->position, {0.5f, 0.5f, 0.0f});
    requireNear(side->normal, {1.0f, 0.0f, 0.0f});
}

TEST_CASE("A ray that is too short, points away or has no direction hits nothing", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const glm::vec3 origin{0.0f, 5.0f, 0.0f};
    const float nan = std::numeric_limits<float>::quiet_NaN();

    REQUIRE_FALSE(world->raycast(origin, {0.0f, -1.0f, 0.0f}, 4.9f).has_value());
    REQUIRE(world->raycast(origin, {0.0f, -1.0f, 0.0f}, 5.1f).has_value());
    REQUIRE_FALSE(world->raycast(origin, {0.0f, 1.0f, 0.0f}, 100.0f).has_value());
    REQUIRE_FALSE(world->raycast(origin, {0.0f, 0.0f, 0.0f}, 100.0f).has_value());
    REQUIRE_FALSE(world->raycast(origin, {0.0f, -1.0f, 0.0f}, 0.0f).has_value());
    REQUIRE_FALSE(world->raycast(origin, {0.0f, -1.0f, 0.0f}, -1.0f).has_value());
    REQUIRE_FALSE(world->raycast({nan, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f).has_value());
    REQUIRE_FALSE(world->raycast(origin, {0.0f, nan, 0.0f}, 100.0f).has_value());
}

TEST_CASE("The direction of a query does not have to be a unit vector", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const auto hit = world->raycast({0.0f, 5.0f, 0.0f}, {0.0f, -20.0f, 0.0f}, 100.0f);
    REQUIRE(hit.has_value());
    REQUIRE_THAT(hit->distance, WithinAbs(5.0, kTolerance));
    // The distance limit is in meters, whatever the length of the direction.
    REQUIRE_FALSE(world->raycast({0.0f, 5.0f, 0.0f}, {0.0f, -20.0f, 0.0f}, 4.0f).has_value());
}

TEST_CASE("A query keeps only the layers it asks for", "[physics]") {
    const PhysicsTestWorld world;
    const physics::BodyHandle floor = world.addFloor();
    const physics::BodyHandle crate = world.addBox({0.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    const glm::vec3 origin{0.0f, 5.0f, 0.0f};
    const glm::vec3 down{0.0f, -1.0f, 0.0f};

    const auto all = world->raycast(origin, down, 100.0f);
    const auto staticOnly = world->raycast(origin, down, 100.0f, {.dynamicBodies = false});
    const auto dynamicOnly = world->raycast(origin, down, 100.0f, {.staticBodies = false});
    REQUIRE(all.has_value());
    REQUIRE(staticOnly.has_value());
    REQUIRE(dynamicOnly.has_value());
    REQUIRE(all->body == crate);
    REQUIRE(staticOnly->body == floor);
    REQUIRE_THAT(staticOnly->distance, WithinAbs(5.0, kTolerance));
    REQUIRE(dynamicOnly->body == crate);

    REQUIRE_FALSE(
        world->raycast(origin, down, 100.0f, {.staticBodies = false, .dynamicBodies = false}).has_value());
    REQUIRE_FALSE(
        world->spherecast(origin, 0.2f, down, 100.0f, {.staticBodies = false, .dynamicBodies = false})
            .has_value());
    const auto sphereStatic = world->spherecast(origin, 0.2f, down, 100.0f, {.dynamicBodies = false});
    REQUIRE(sphereStatic.has_value());
    REQUIRE(sphereStatic->body == floor);
}

TEST_CASE("A swept sphere stops when its surface touches, not its center", "[physics]") {
    const PhysicsTestWorld world;
    const physics::BodyHandle floor = world.addFloor();

    const auto hit = world->spherecast({0.0f, 5.0f, 0.0f}, 0.25f, {0.0f, -1.0f, 0.0f}, 100.0f);
    REQUIRE(hit.has_value());
    REQUIRE(hit->body == floor);
    // The center stops one radius above the floor.
    REQUIRE_THAT(hit->distance, WithinAbs(4.75, 0.01));
    requireNear(hit->position, {0.0f, 0.25f, 0.0f}, 0.01f);
    requireNear(hit->contact, {0.0f, 0.0f, 0.0f}, 0.01f);
    requireNear(hit->normal, {0.0f, 1.0f, 0.0f}, 0.01f);

    REQUIRE_FALSE(world->spherecast({0.0f, 5.0f, 0.0f}, 0.25f, {0.0f, -1.0f, 0.0f}, 4.5f).has_value());
    REQUIRE_FALSE(world->spherecast({0.0f, 5.0f, 0.0f}, 0.0f, {0.0f, -1.0f, 0.0f}, 100.0f).has_value());
    REQUIRE_FALSE(world->spherecast({0.0f, 5.0f, 0.0f}, -1.0f, {0.0f, -1.0f, 0.0f}, 100.0f).has_value());
}

TEST_CASE("A swept sphere is stopped by a wall that a ray along its center misses", "[physics]") {
    const PhysicsTestWorld world;
    // A wall whose top edge is 10 cm below the path of the center.
    const physics::BodyHandle wall =
        world.addBox({3.0f, 0.5f, 0.0f}, {0.05f, 0.5f, 2.0f}, MotionType::Static);
    const glm::vec3 origin{0.0f, 1.1f, 0.0f};
    const glm::vec3 forward{1.0f, 0.0f, 0.0f};

    REQUIRE_FALSE(world->raycast(origin, forward, 10.0f).has_value());
    const auto hit = world->spherecast(origin, 0.3f, forward, 10.0f);
    REQUIRE(hit.has_value());
    REQUIRE(hit->body == wall);
    REQUIRE(hit->distance > 2.6f);
    REQUIRE(hit->distance < 2.95f);
    // The contact is on the wall's top edge, and the normal pushes the sphere back and up.
    REQUIRE(hit->normal.x < 0.0f);
    REQUIRE(hit->normal.y > 0.0f);
}

TEST_CASE("A swept sphere that starts inside a body reports a hit at distance zero", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const auto hit = world->spherecast({0.0f, 0.1f, 0.0f}, 0.25f, {1.0f, 0.0f, 0.0f}, 5.0f);
    REQUIRE(hit.has_value());
    REQUIRE(hit->distance == 0.0f);
}

TEST_CASE("Queries see a body where it is after the simulation moved it", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::BodyHandle crate = world.addBox({0.0f, 4.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    const glm::vec3 origin{-5.0f, 0.5f, 0.0f};
    const glm::vec3 forward{1.0f, 0.0f, 0.0f};

    REQUIRE_FALSE(world->raycast(origin, forward, 10.0f).has_value());
    world.stepSeconds(3.0);
    const auto hit = world->raycast(origin, forward, 10.0f);
    REQUIRE(hit.has_value());
    REQUIRE(hit->body == crate);
    REQUIRE_THAT(hit->distance, WithinAbs(4.5, 0.01));

    world->destroyBody(crate);
    REQUIRE_FALSE(world->raycast(origin, forward, 10.0f).has_value());
}
