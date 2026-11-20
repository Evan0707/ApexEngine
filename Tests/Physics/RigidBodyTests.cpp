#include "PhysicsTestWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <limits>

using namespace apex;
using namespace apex::tests;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using physics::MotionType;
using physics::ShapeType;

TEST_CASE("Bodies are created, counted and destroyed", "[physics]") {
    const PhysicsTestWorld world;
    const physics::BodyHandle floor = world.addFloor();
    const physics::BodyHandle box = world.addBox({1.0f, 2.0f, 3.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    REQUIRE(floor.valid());
    REQUIRE(box.valid());
    REQUIRE(floor != box);
    REQUIRE(world->bodyCount() == 2);
    REQUIRE(world->contains(box));
    REQUIRE(world.position(box) == glm::vec3(1.0f, 2.0f, 3.0f));

    world->destroyBody(box);
    REQUIRE(world->bodyCount() == 1);
    REQUIRE_FALSE(world->contains(box));
    REQUIRE(world->contains(floor));
}

TEST_CASE("A stale or invalid body handle is harmless", "[physics]") {
    const PhysicsTestWorld world;
    const physics::BodyHandle box = world.addBox({0.0f, 5.0f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    world->destroyBody(box);

    for (const physics::BodyHandle handle : {box, physics::BodyHandle{}}) {
        world->destroyBody(handle);
        world->setTransform(handle, {});
        world->setLinearVelocity(handle, {1.0f, 0.0f, 0.0f});
        world->applyImpulse(handle, {1.0f, 0.0f, 0.0f});
        REQUIRE_FALSE(world->contains(handle));
        REQUIRE_FALSE(world->transform(handle).has_value());
        REQUIRE(world->linearVelocity(handle) == glm::vec3(0.0f));
        REQUIRE_FALSE(world->isActive(handle));
    }
    world.stepSeconds(0.1);
    REQUIRE(world->bodyCount() == 0);
}

TEST_CASE("An invalid body description is refused with its reason", "[physics]") {
    const PhysicsTestWorld world;
    const float nan = std::numeric_limits<float>::quiet_NaN();

    const auto flatBox =
        world->createBody({.shape = {.type = ShapeType::Box, .halfExtents = {1.0f, 0.0f, 1.0f}}});
    REQUIRE_FALSE(flatBox.has_value());
    REQUIRE(flatBox.error().code == core::ErrorCode::InvalidArgument);
    REQUIRE_THAT(flatBox.error().message, ContainsSubstring("box"));

    REQUIRE_FALSE(world->createBody({.shape = {.type = ShapeType::Sphere, .radius = -1.0f}}).has_value());
    REQUIRE_FALSE(world->createBody({.shape = {.type = ShapeType::Sphere, .radius = nan}}).has_value());
    REQUIRE_FALSE(
        world->createBody({.shape = {.type = ShapeType::Capsule, .radius = 0.3f, .halfHeight = 0.0f}})
            .has_value());

    const auto massless = world->createBody({.motion = MotionType::Dynamic, .mass = 0.0f});
    REQUIRE_FALSE(massless.has_value());
    REQUIRE_THAT(massless.error().message, ContainsSubstring("mass"));
    // The mass of a static body is not read.
    REQUIRE(world->createBody({.motion = MotionType::Static, .mass = 0.0f}).has_value());

    REQUIRE_FALSE(world->createBody({.transform = {.position = {nan, 0.0f, 0.0f}}}).has_value());
    REQUIRE_FALSE(world->createBody({.transform = {.rotation = {0.0f, 0.0f, 0.0f, 0.0f}}}).has_value());
    REQUIRE_FALSE(world->createBody({.friction = -0.1f}).has_value());
    REQUIRE(world->bodyCount() == 1);
}

TEST_CASE("A full physics world refuses one more body", "[physics]") {
    const PhysicsTestWorld world({.maxBodies = 2});
    REQUIRE(world->createBody({}).has_value());
    REQUIRE(world->createBody({.transform = {.position = {5.0f, 0.0f, 0.0f}}}).has_value());
    const auto third = world->createBody({.transform = {.position = {10.0f, 0.0f, 0.0f}}});
    REQUIRE_FALSE(third.has_value());
    REQUIRE(third.error().code == core::ErrorCode::PhysicsFailure);
    REQUIRE(world->bodyCount() == 2);
}

TEST_CASE("The three shapes rest on the floor at the height of their size", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const auto create = [&](const physics::Shape& shape, float x) {
        const auto body = world->createBody(
            {.shape = shape, .motion = MotionType::Dynamic, .transform = {.position = {x, 2.0f, 0.0f}}});
        REQUIRE(body.has_value());
        return *body;
    };
    const physics::BodyHandle box =
        create({.type = ShapeType::Box, .halfExtents = {0.5f, 0.25f, 0.5f}}, -3.0f);
    const physics::BodyHandle sphere = create({.type = ShapeType::Sphere, .radius = 0.4f}, 0.0f);
    // Upright: 0.3 m of radius below the lower sphere's center, itself 0.6 m below the body's center.
    const physics::BodyHandle capsule =
        create({.type = ShapeType::Capsule, .radius = 0.3f, .halfHeight = 0.6f}, 3.0f);

    world.stepSeconds(1.5);
    // 3 cm: Jolt lets resting bodies overlap by up to 2 cm.
    REQUIRE_THAT(world.position(box).y, WithinAbs(0.25, 0.03));
    REQUIRE_THAT(world.position(sphere).y, WithinAbs(0.4, 0.03));
    REQUIRE_THAT(world.position(capsule).y, WithinAbs(0.9, 0.03));
}

TEST_CASE("A dynamic body falls with the world's gravity; a static one stays", "[physics]") {
    const PhysicsTestWorld world;
    const physics::BodyHandle falling =
        world.addBox({0.0f, 100.0f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    const physics::BodyHandle fixed = world.addBox({5.0f, 100.0f, 0.0f}, glm::vec3(0.5f), MotionType::Static);

    world.stepSeconds(1.0);
    // 4.9 m after one second, a little less with Jolt's default damping.
    REQUIRE_THAT(100.0 - world.position(falling).y, WithinAbs(4.9, 0.25));
    REQUIRE_THAT(world->linearVelocity(falling).y, WithinAbs(-9.81, 0.5));
    REQUIRE(world.position(fixed) == glm::vec3(5.0f, 100.0f, 0.0f));
    REQUIRE(world->linearVelocity(fixed) == glm::vec3(0.0f));
}

TEST_CASE("A body created rotated keeps its rotation", "[physics]") {
    const PhysicsTestWorld world({.gravity = glm::vec3(0.0f)});
    const glm::quat quarterTurn = glm::angleAxis(glm::radians(90.0f), core::kWorldUp);
    const auto body =
        world->createBody({.motion = MotionType::Dynamic,
                           .transform = {.position = {1.0f, 2.0f, 3.0f}, .rotation = quarterTurn}});
    REQUIRE(body.has_value());
    world.stepSeconds(0.5);
    const auto transform = world->transform(*body);
    REQUIRE(transform.has_value());
    REQUIRE_THAT(std::abs(glm::dot(transform->rotation, quarterTurn)), WithinAbs(1.0, 1e-5));
    REQUIRE_THAT(transform->position.y, WithinAbs(2.0, 1e-5));
}

TEST_CASE("An impulse changes the velocity by impulse over mass", "[physics]") {
    const PhysicsTestWorld world({.gravity = glm::vec3(0.0f)});
    const physics::BodyHandle light =
        world.addBox({0.0f, 0.0f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic, 2.0f);
    const physics::BodyHandle heavy =
        world.addBox({0.0f, 5.0f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic, 20.0f);
    const physics::BodyHandle fixed = world.addBox({0.0f, 10.0f, 0.0f}, glm::vec3(0.5f), MotionType::Static);

    for (const physics::BodyHandle body : {light, heavy, fixed}) {
        world->applyImpulse(body, {4.0f, 0.0f, 0.0f});
    }
    REQUIRE_THAT(world->linearVelocity(light).x, WithinAbs(2.0, 1e-4));
    REQUIRE_THAT(world->linearVelocity(heavy).x, WithinAbs(0.2, 1e-4));
    REQUIRE(world->linearVelocity(fixed) == glm::vec3(0.0f));

    world.stepSeconds(1.0);
    REQUIRE_THAT(world.position(light).x, WithinAbs(2.0, 0.1));
    REQUIRE(world.position(fixed) == glm::vec3(0.0f, 10.0f, 0.0f));
}

TEST_CASE("A resting body falls asleep, and an impulse wakes it", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::BodyHandle box = world.addBox({0.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
    REQUIRE(world->isActive(box));
    REQUIRE(world->activeBodyCount() == 1);

    world.stepSeconds(3.0);
    REQUIRE_FALSE(world->isActive(box));
    REQUIRE(world->activeBodyCount() == 0);

    world->applyImpulse(box, {0.0f, 100.0f, 0.0f});
    REQUIRE(world->isActive(box));
    world.stepSeconds(0.2);
    REQUIRE(world.position(box).y > 0.6f);
}

TEST_CASE("Setting a transform teleports a body, setting a velocity moves it", "[physics]") {
    const PhysicsTestWorld world({.gravity = glm::vec3(0.0f)});
    const physics::BodyHandle box = world.addBox({0.0f, 0.0f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);

    world->setTransform(box, {.position = {10.0f, 20.0f, 30.0f}});
    REQUIRE(world.position(box) == glm::vec3(10.0f, 20.0f, 30.0f));

    world->setLinearVelocity(box, {0.0f, 0.0f, -3.0f});
    world.stepSeconds(1.0);
    REQUIRE_THAT(world.position(box).z, WithinAbs(27.0, 0.1));
    REQUIRE_THAT(world.position(box).x, WithinAbs(10.0, 1e-4));
}

TEST_CASE("The simulation gives the same result every time", "[physics]") {
    const auto run = [] {
        const PhysicsTestWorld world;
        (void)world.addFloor();
        const physics::BodyHandle first =
            world.addBox({0.0f, 3.0f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic);
        const physics::BodyHandle second =
            world.addBox({0.3f, 5.0f, 0.2f}, glm::vec3(0.5f), MotionType::Dynamic);
        world.stepSeconds(3.0);
        return std::pair(world.position(first), world.position(second));
    };
    const auto a = run();
    const auto b = run();
    REQUIRE(a.first == b.first);
    REQUIRE(a.second == b.second);
}
