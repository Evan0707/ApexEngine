#include "World/PhysicsSync.h"

#include "../Physics/PhysicsTestWorld.h"
#include "World/RenderExtraction.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace apex;
using namespace apex::tests;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using world::BodyType;
using world::ColliderShape;

namespace {

entt::entity addEntity(entt::registry& registry, std::string id, const world::Transform& transform,
                       const world::Collider& collider) {
    const entt::entity entity = registry.create();
    registry.emplace<world::SceneId>(entity, std::move(id));
    registry.emplace<world::Transform>(entity, transform);
    registry.emplace<world::Collider>(entity, collider);
    return entity;
}

// A unit cube collider scaled into a floor whose top is at y = 0, as in the demo scene.
entt::entity addFloor(entt::registry& registry) {
    const entt::entity floor =
        addEntity(registry, "floor", {.position = {0.0f, -0.1f, 0.0f}, .scale = {30.0f, 0.2f, 30.0f}},
                  {.shape = ColliderShape::Box, .halfExtents = glm::vec3(0.5f)});
    registry.emplace<world::RigidBody>(floor, BodyType::Static);
    return floor;
}

entt::entity addCrate(entt::registry& registry, std::string id, const glm::vec3& position) {
    const entt::entity crate = addEntity(registry, std::move(id), {.position = position},
                                         {.shape = ColliderShape::Box, .halfExtents = glm::vec3(0.5f)});
    registry.emplace<world::RigidBody>(crate, BodyType::Dynamic, 20.0f);
    return crate;
}

void stepSeconds(entt::registry& registry, physics::PhysicsWorld& physics, double seconds) {
    const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
    for (int i = 0; i < steps; ++i) {
        world::stepPhysics(registry, physics, kPhysicsStep);
    }
}

} // namespace

TEST_CASE("A collider's dimensions are multiplied by the entity's scale", "[world]") {
    const auto box = world::scaledShape({.shape = ColliderShape::Box, .halfExtents = {0.5f, 0.5f, 0.5f}},
                                        {30.0f, 0.2f, -4.0f});
    REQUIRE(box.has_value());
    REQUIRE(box->type == physics::ShapeType::Box);
    REQUIRE(box->halfExtents == glm::vec3(15.0f, 0.1f, 2.0f));

    const auto sphere = world::scaledShape({.shape = ColliderShape::Sphere, .radius = 0.5f}, glm::vec3(3.0f));
    REQUIRE(sphere.has_value());
    REQUIRE(sphere->type == physics::ShapeType::Sphere);
    REQUIRE(sphere->radius == 1.5f);

    const auto capsule = world::scaledShape(
        {.shape = ColliderShape::Capsule, .radius = 0.3f, .halfHeight = 0.6f}, {2.0f, 3.0f, 2.0f});
    REQUIRE(capsule.has_value());
    REQUIRE(capsule->type == physics::ShapeType::Capsule);
    REQUIRE_THAT(capsule->radius, WithinAbs(0.6, 1e-6));
    REQUIRE_THAT(capsule->halfHeight, WithinAbs(1.8, 1e-6));
}

TEST_CASE("A scale that would deform a sphere or a capsule is refused", "[world]") {
    const auto sphere = world::scaledShape({.shape = ColliderShape::Sphere}, {1.0f, 2.0f, 1.0f});
    REQUIRE_FALSE(sphere.has_value());
    REQUIRE_THAT(sphere.error().message, ContainsSubstring("sphere"));

    const auto capsule = world::scaledShape({.shape = ColliderShape::Capsule}, {1.0f, 2.0f, 1.5f});
    REQUIRE_FALSE(capsule.has_value());
    REQUIRE_THAT(capsule.error().message, ContainsSubstring("capsule"));
    // A capsule may be stretched along its axis.
    REQUIRE(world::scaledShape({.shape = ColliderShape::Capsule}, {1.0f, 2.0f, 1.0f}).has_value());

    REQUIRE_FALSE(world::scaledShape({.shape = ColliderShape::Box}, {1.0f, 0.0f, 1.0f}).has_value());
}

TEST_CASE("Entities with a collider get a body, and a dynamic one moves its transform", "[world]") {
    const PhysicsTestWorld physics;
    entt::registry registry;
    const entt::entity floor = addFloor(registry);
    const entt::entity crate = addCrate(registry, "crate", {2.0f, 3.0f, -1.0f});
    registry.get<world::Transform>(crate).scale = glm::vec3(1.0f);
    // Without a RigidBody, a collider is a static obstacle.
    const entt::entity platform =
        addEntity(registry, "platform", {.position = {2.0f, 0.5f, -1.0f}, .scale = {3.0f, 1.0f, 3.0f}},
                  {.shape = ColliderShape::Box, .halfExtents = glm::vec3(0.5f)});
    // No collider: nothing for Physics.
    const entt::entity decor = registry.create();
    registry.emplace<world::Transform>(decor);

    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(physics->bodyCount() == 3);
    REQUIRE(registry.all_of<world::PhysicsBody>(floor));
    REQUIRE(registry.all_of<world::PhysicsBody>(crate));
    REQUIRE(registry.all_of<world::PhysicsBody>(platform));
    REQUIRE_FALSE(registry.all_of<world::PhysicsBody>(decor));

    stepSeconds(registry, *physics, 3.0);
    // The crate fell on the platform's top (y = 1) and rests there, half a meter higher.
    const world::Transform& moved = registry.get<world::Transform>(crate);
    REQUIRE_THAT(moved.position.y, WithinAbs(1.5, 0.03));
    REQUIRE_THAT(moved.position.x, WithinAbs(2.0, 0.01));
    REQUIRE(moved.scale == glm::vec3(1.0f));
    // Static entities keep their transform exactly.
    REQUIRE(registry.get<world::Transform>(floor).position == glm::vec3(0.0f, -0.1f, 0.0f));
    REQUIRE(registry.get<world::Transform>(platform).position == glm::vec3(2.0f, 0.5f, -1.0f));
}

TEST_CASE("A body starts with the entity's rotation", "[world]") {
    const PhysicsTestWorld physics({.gravity = glm::vec3(0.0f)});
    entt::registry registry;
    const glm::quat tilted = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const entt::entity ramp =
        addEntity(registry, "ramp", {.position = {0.0f, 1.0f, 0.0f}, .rotation = tilted},
                  {.shape = ColliderShape::Box});
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    const auto transform = physics->transform(registry.get<world::PhysicsBody>(ramp).handle);
    REQUIRE(transform.has_value());
    REQUIRE_THAT(std::abs(glm::dot(transform->rotation, tilted)), WithinAbs(1.0, 1e-5));
}

TEST_CASE("Creating physics objects twice does not duplicate them, and picks up new entities", "[world]") {
    const PhysicsTestWorld physics;
    entt::registry registry;
    (void)addFloor(registry);
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(physics->bodyCount() == 1);

    (void)addCrate(registry, "late", {0.0f, 2.0f, 0.0f});
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(physics->bodyCount() == 2);
}

TEST_CASE("A collider that Physics cannot build is reported with the entity's id", "[world]") {
    const PhysicsTestWorld physics;
    entt::registry registry;
    (void)addEntity(registry, "egg", {.scale = {1.0f, 2.0f, 1.0f}}, {.shape = ColliderShape::Sphere});
    const auto created = world::createPhysicsObjects(registry, *physics);
    REQUIRE_FALSE(created.has_value());
    REQUIRE(created.error().code == core::ErrorCode::InvalidArgument);
    REQUIRE_THAT(created.error().message, ContainsSubstring("'egg'"));
    REQUIRE_THAT(created.error().message, ContainsSubstring("sphere"));
    REQUIRE(physics->bodyCount() == 0);
}

TEST_CASE("Destroying physics objects empties the physics world and removes the handles", "[world]") {
    const PhysicsTestWorld physics;
    entt::registry registry;
    const entt::entity floor = addFloor(registry);
    const entt::entity crate = addCrate(registry, "crate", {0.0f, 2.0f, 0.0f});
    const entt::entity player = registry.create();
    registry.emplace<world::Transform>(player);
    registry.emplace<world::CharacterBody>(player);
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(physics->bodyCount() == 2);
    REQUIRE(physics->characterCount() == 1);

    world::destroyPhysicsObjects(registry, *physics);
    REQUIRE(physics->bodyCount() == 0);
    REQUIRE(physics->characterCount() == 0);
    REQUIRE_FALSE(registry.any_of<world::PhysicsBody>(floor));
    REQUIRE_FALSE(registry.any_of<world::PhysicsBody>(crate));
    REQUIRE_FALSE(registry.any_of<world::PhysicsCharacter>(player));

    // The scene can be given to Physics again (AC-15 loads and unloads it 20 times).
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(physics->bodyCount() == 2);
    REQUIRE(physics->characterCount() == 1);
}

TEST_CASE("A character entity follows its motion and reports its ground", "[world]") {
    const PhysicsTestWorld physics;
    entt::registry registry;
    (void)addFloor(registry);
    const glm::quat facing = glm::angleAxis(glm::radians(90.0f), core::kWorldUp);
    const entt::entity player = registry.create();
    registry.emplace<world::Transform>(player,
                                       world::Transform{.position = {0.0f, 0.0f, 0.0f}, .rotation = facing});
    registry.emplace<world::CharacterBody>(player);
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    REQUIRE(registry.all_of<world::PhysicsCharacter, world::CharacterMotion, world::CharacterGround>(player));

    // Gravity only: the character settles on the floor.
    const glm::vec3 fall = physics->gravity() * static_cast<float>(kPhysicsStep);
    registry.get<world::CharacterMotion>(player).velocity = fall;
    stepSeconds(registry, *physics, 0.5);
    REQUIRE(registry.get<world::CharacterGround>(player).state == physics::GroundState::OnGround);
    REQUIRE_THAT(registry.get<world::CharacterGround>(player).normal.y, WithinAbs(1.0, 1e-3));

    registry.get<world::CharacterMotion>(player).velocity = glm::vec3(4.0f, 0.0f, 0.0f) + fall;
    stepSeconds(registry, *physics, 1.0);
    const world::Transform& transform = registry.get<world::Transform>(player);
    REQUIRE_THAT(transform.position.x, WithinAbs(4.0, 0.1));
    REQUIRE_THAT(transform.position.y, WithinAbs(0.0, 0.05));
    // Physics moves the character; its facing stays the game's.
    REQUIRE(transform.rotation == facing);
    REQUIRE(registry.get<world::CharacterGround>(player).velocity.x == 4.0f);
}

TEST_CASE("Bodies moved by physics are interpolated between two fixed steps", "[world]") {
    const PhysicsTestWorld physics;
    entt::registry registry;
    const entt::entity crate = addCrate(registry, "crate", {0.0f, 50.0f, 0.0f});
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
    stepSeconds(registry, *physics, 0.5);

    world::rememberTransforms(registry);
    const float before = registry.get<world::Transform>(crate).position.y;
    world::stepPhysics(registry, *physics, kPhysicsStep);
    const float after = registry.get<world::Transform>(crate).position.y;
    REQUIRE(after < before);

    const world::Transform halfway = world::interpolateTransform(
        registry.get<world::PreviousTransform>(crate).value, registry.get<world::Transform>(crate), 0.5f);
    REQUIRE_THAT(halfway.position.y, WithinAbs(0.5 * (before + after), 1e-4));
}
