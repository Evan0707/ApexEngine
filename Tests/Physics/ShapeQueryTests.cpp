#include "PhysicsTestWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace apex;
using Catch::Matchers::WithinAbs;

TEST_CASE("Physics gives back the shape it simulates for each body", "[physics]") {
    tests::PhysicsTestWorld world;
    const physics::BodyHandle box =
        world.addBox({0.0f, 1.0f, 0.0f}, {1.0f, 0.25f, 2.0f}, physics::MotionType::Dynamic);
    const auto boxShape = world->shape(box);
    REQUIRE(boxShape.has_value());
    REQUIRE(boxShape->type == physics::ShapeType::Box);
    REQUIRE_THAT(boxShape->halfExtents.x, WithinAbs(1.0, 1e-5));
    REQUIRE_THAT(boxShape->halfExtents.y, WithinAbs(0.25, 1e-5));
    REQUIRE_THAT(boxShape->halfExtents.z, WithinAbs(2.0, 1e-5));

    const auto sphere = world->createBody({.shape = {.type = physics::ShapeType::Sphere, .radius = 0.4f},
                                           .motion = physics::MotionType::Static});
    REQUIRE(sphere.has_value());
    REQUIRE(world->shape(*sphere)->type == physics::ShapeType::Sphere);
    REQUIRE_THAT(world->shape(*sphere)->radius, WithinAbs(0.4, 1e-5));

    const auto capsule =
        world->createBody({.shape = {.type = physics::ShapeType::Capsule, .radius = 0.3f, .halfHeight = 0.6f},
                           .motion = physics::MotionType::Dynamic,
                           .transform = {.position = {3.0f, 2.0f, 0.0f}}});
    REQUIRE(capsule.has_value());
    const auto capsuleShape = world->shape(*capsule);
    REQUIRE(capsuleShape->type == physics::ShapeType::Capsule);
    REQUIRE_THAT(capsuleShape->radius, WithinAbs(0.3, 1e-5));
    REQUIRE_THAT(capsuleShape->halfHeight, WithinAbs(0.6, 1e-5));

    world->destroyBody(box);
    REQUIRE_FALSE(world->shape(box).has_value());
    REQUIRE_FALSE(world->shape(physics::BodyHandle{}).has_value());
}

TEST_CASE("Physics gives back the character's capsule", "[physics]") {
    tests::PhysicsTestWorld world;
    const auto character = world->createCharacter({.height = 1.8f, .radius = 0.3f});
    REQUIRE(character.has_value());
    const auto shape = world->characterShape(*character);
    REQUIRE(shape.has_value());
    REQUIRE(shape->type == physics::ShapeType::Capsule);
    REQUIRE_THAT(shape->radius, WithinAbs(0.3, 1e-5));
    // Its whole height: the cylinder and the two half spheres.
    REQUIRE_THAT(2.0f * (shape->halfHeight + shape->radius), WithinAbs(1.8, 1e-5));

    world->destroyCharacter(*character);
    REQUIRE_FALSE(world->characterShape(*character).has_value());
}
