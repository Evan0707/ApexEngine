#include "World/ColliderLines.h"

#include "../Physics/PhysicsTestWorld.h"
#include "World/PhysicsSync.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace apex;
using namespace apex::tests;
using Catch::Matchers::WithinAbs;
using graphics::RenderLine;

namespace {

constexpr float kTolerance = 1e-4f;

// Every end point of the lines.
[[nodiscard]] std::vector<glm::vec3> points(const std::vector<RenderLine>& lines) {
    std::vector<glm::vec3> result;
    for (const RenderLine& line : lines) {
        result.push_back(line.start);
        result.push_back(line.end);
    }
    return result;
}

[[nodiscard]] glm::vec3 lowest(const std::vector<RenderLine>& lines) {
    glm::vec3 low{1e9f};
    for (const glm::vec3& point : points(lines)) {
        low = glm::min(low, point);
    }
    return low;
}

[[nodiscard]] glm::vec3 highest(const std::vector<RenderLine>& lines) {
    glm::vec3 high{-1e9f};
    for (const glm::vec3& point : points(lines)) {
        high = glm::max(high, point);
    }
    return high;
}

entt::entity addBody(entt::registry& registry, const world::Transform& transform,
                     const world::Collider& collider, world::BodyType type) {
    const entt::entity entity = registry.create();
    registry.emplace<world::Transform>(entity, transform);
    registry.emplace<world::Collider>(entity, collider);
    registry.emplace<world::RigidBody>(entity, type, 10.0f);
    return entity;
}

} // namespace

TEST_CASE("A box's wireframe is its twelve edges between its corners", "[world]") {
    std::vector<RenderLine> lines;
    const physics::Shape box{.type = physics::ShapeType::Box, .halfExtents = {1.0f, 2.0f, 3.0f}};
    world::appendShapeLines(box, glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f)),
                            world::kStaticColliderColor, lines);
    REQUIRE(lines.size() == 12);
    for (const RenderLine& line : lines) {
        // Each edge runs along one axis, the length of the box on that axis.
        const glm::vec3 edge = glm::abs(line.end - line.start);
        const float length = edge.x + edge.y + edge.z;
        REQUIRE((length == 2.0f || length == 4.0f || length == 6.0f));
        REQUIRE(line.color == world::kStaticColliderColor);
    }
    REQUIRE(lowest(lines) == glm::vec3(9.0f, -2.0f, -3.0f));
    REQUIRE(highest(lines) == glm::vec3(11.0f, 2.0f, 3.0f));
}

TEST_CASE("A sphere's wireframe is three circles of its radius", "[world]") {
    std::vector<RenderLine> lines;
    world::appendShapeLines({.type = physics::ShapeType::Sphere, .radius = 0.5f}, glm::mat4(1.0f),
                            world::kAwakeColliderColor, lines);
    REQUIRE(lines.size() == 3 * world::kColliderCircleSegments);
    for (const glm::vec3& point : points(lines)) {
        REQUIRE_THAT(glm::length(point), WithinAbs(0.5, kTolerance));
    }
}

TEST_CASE("A capsule's wireframe spans its height and its radius", "[world]") {
    std::vector<RenderLine> lines;
    world::appendShapeLines({.type = physics::ShapeType::Capsule, .radius = 0.3f, .halfHeight = 0.6f},
                            glm::mat4(1.0f), world::kCharacterColliderColor, lines);
    REQUIRE_THAT(highest(lines).y, WithinAbs(0.9, kTolerance));
    REQUIRE_THAT(lowest(lines).y, WithinAbs(-0.9, kTolerance));
    REQUIRE_THAT(highest(lines).x, WithinAbs(0.3, kTolerance));
    REQUIRE_THAT(lowest(lines).z, WithinAbs(-0.3, kTolerance));
}

TEST_CASE("The extraction draws what Physics simulates, scale included, relative to the camera", "[world]") {
    PhysicsTestWorld physics;
    entt::registry registry;
    const entt::entity camera = registry.create();
    registry.emplace<world::Transform>(camera, world::Transform{.position = {0.0f, 2.0f, 10.0f}});
    registry.emplace<world::Camera>(camera);
    // A unit collider stretched by the entity's scale, as the demo's floor.
    addBody(registry, {.position = {4.0f, -0.1f, 0.0f}, .scale = {20.0f, 0.2f, 10.0f}},
            {.shape = world::ColliderShape::Box, .halfExtents = glm::vec3(0.5f)}, world::BodyType::Static);
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());

    graphics::RenderScene scene;
    world::extractColliderLines(registry, *physics, camera, 1.0f, scene);
    REQUIRE(scene.lines.size() == 12);
    // World x from -6 to 14, y from -0.2 to 0, z from -5 to 5; minus the camera's position.
    const glm::vec3 low = lowest(scene.lines);
    const glm::vec3 high = highest(scene.lines);
    REQUIRE_THAT(low.x, WithinAbs(-6.0, kTolerance));
    REQUIRE_THAT(high.x, WithinAbs(14.0, kTolerance));
    REQUIRE_THAT(low.y, WithinAbs(-2.2, kTolerance));
    REQUIRE_THAT(high.y, WithinAbs(-2.0, kTolerance));
    REQUIRE_THAT(low.z, WithinAbs(-15.0, kTolerance));
    REQUIRE_THAT(high.z, WithinAbs(-5.0, kTolerance));
}

TEST_CASE("Collider colors tell static, awake, asleep and the character apart", "[world]") {
    PhysicsTestWorld physics;
    entt::registry registry;
    addBody(registry, {.position = {0.0f, -0.5f, 0.0f}, .scale = {40.0f, 1.0f, 40.0f}},
            {.shape = world::ColliderShape::Box, .halfExtents = glm::vec3(0.5f)}, world::BodyType::Static);
    addBody(registry, {.position = {0.0f, 3.0f, 0.0f}},
            {.shape = world::ColliderShape::Sphere, .radius = 0.5f}, world::BodyType::Dynamic);
    const entt::entity player = registry.create();
    registry.emplace<world::Transform>(player, world::Transform{.position = {5.0f, 0.0f, 0.0f}});
    registry.emplace<world::CharacterBody>(player);
    REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());

    const auto countColor = [](const graphics::RenderScene& scene, const glm::vec4& color) {
        return std::ranges::count_if(scene.lines,
                                     [&color](const RenderLine& line) { return line.color == color; });
    };
    graphics::RenderScene scene;
    world::extractColliderLines(registry, *physics, entt::null, 1.0f, scene);
    REQUIRE(countColor(scene, world::kStaticColliderColor) == 12);
    REQUIRE(countColor(scene, world::kAwakeColliderColor) == 3 * world::kColliderCircleSegments);
    REQUIRE(countColor(scene, world::kCharacterColliderColor) > 0);
    // The character's capsule stands on its feet: from y = 0 to its height.
    std::vector<RenderLine> character;
    std::ranges::copy_if(scene.lines, std::back_inserter(character),
                         [](const RenderLine& line) { return line.color == world::kCharacterColliderColor; });
    REQUIRE_THAT(lowest(character).y, WithinAbs(0.0, kTolerance));
    REQUIRE_THAT(highest(character).y, WithinAbs(1.8, kTolerance));

    // The ball falls, lands and goes to sleep.
    for (int step = 0; step < 600; ++step) {
        world::stepPhysics(registry, *physics, kPhysicsStep);
    }
    world::extractColliderLines(registry, *physics, entt::null, 1.0f, scene);
    REQUIRE(countColor(scene, world::kAsleepColliderColor) == 3 * world::kColliderCircleSegments);
}
