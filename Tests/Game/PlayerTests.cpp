#include "GameSystems.h"

#include "../Physics/PhysicsTestWorld.h"
#include "World/PhysicsSync.h"
#include "World/Rotation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <string>

using namespace apex;
using namespace apex::game;
using namespace apex::tests;
using Catch::Matchers::WithinAbs;

namespace {

// The player and what it walks on, simulated as the demo does: the game's step, then the physics.
struct PlayerWorld {
    PlayerWorld() {
        addStatic({0.0f, -0.5f, 0.0f}, {100.0f, 1.0f, 100.0f});
        const entt::entity camera = registry.create();
        registry.emplace<world::SceneId>(camera, std::string(kCameraId));
        registry.emplace<world::Transform>(camera);
        registry.emplace<world::Camera>(camera);
        player = registry.create();
        registry.emplace<world::SceneId>(player, std::string(kPlayerId));
        registry.emplace<world::Transform>(player);
    }

    // A static box given by its center and its size.
    void addStatic(const glm::vec3& center, const glm::vec3& size) {
        const entt::entity entity = registry.create();
        registry.emplace<world::Transform>(entity, world::Transform{.position = center, .scale = size});
        registry.emplace<world::Collider>(entity);
    }

    [[nodiscard]] entt::entity addCrate(const glm::vec3& center) {
        const entt::entity crate = registry.create();
        registry.emplace<world::Transform>(crate, world::Transform{.position = center});
        registry.emplace<world::Collider>(crate);
        registry.emplace<world::RigidBody>(crate, world::BodyType::Dynamic, 20.0f);
        return crate;
    }

    void start() {
        const auto entities = attachGameComponents(registry, {});
        REQUIRE(entities.has_value());
        REQUIRE(entities->player == player);
        REQUIRE(world::createPhysicsObjects(registry, *physics).has_value());
        // Lets the character find the ground.
        simulate({}, 0.25);
    }

    // Returns the highest the feet went. A jump is asked on the first step only.
    float simulate(PlayerInput input, double seconds) {
        float highest = position().y;
        const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
        for (int i = 0; i < steps; ++i) {
            stepPlayerEntity(registry, player, input, physics->gravity(), kPhysicsStep, settings);
            world::stepPhysics(registry, *physics, kPhysicsStep);
            input.jump = false;
            highest = std::max(highest, position().y);
        }
        return highest;
    }

    [[nodiscard]] glm::vec3 position() const { return registry.get<world::Transform>(player).position; }
    [[nodiscard]] bool onGround() const {
        return registry.get<world::CharacterGround>(player).state == physics::GroundState::OnGround;
    }

    PhysicsTestWorld physics;
    entt::registry registry;
    entt::entity player = entt::null;
    PlayerSettings settings;
};

} // namespace

TEST_CASE("The entity 'player' gets a character body and a controller facing where it does", "[game]") {
    entt::registry registry;
    const entt::entity camera = registry.create();
    registry.emplace<world::SceneId>(camera, std::string(kCameraId));
    registry.emplace<world::Transform>(camera);
    registry.emplace<world::Camera>(camera);
    const entt::entity player = registry.create();
    registry.emplace<world::SceneId>(player, std::string(kPlayerId));
    registry.emplace<world::Transform>(
        player, world::Transform{.rotation = world::rotationFromEulerDegrees({0.0f, 135.0f, 0.0f})});

    const auto entities = attachGameComponents(registry, {.height = 1.6f, .radius = 0.25f});
    REQUIRE(entities.has_value());
    REQUIRE(entities->camera == camera);
    REQUIRE(entities->player == player);
    REQUIRE(registry.get<world::CharacterBody>(player).height == 1.6f);
    REQUIRE(registry.get<world::CharacterBody>(player).radius == 0.25f);
    REQUIRE_THAT(registry.get<PlayerController>(player).state.facingYaw,
                 WithinAbs(glm::radians(135.0f), 1e-4));
}

TEST_CASE("The player walks at 4 m/s and runs at 7 m/s, away from the camera", "[game]") {
    PlayerWorld world;
    world.start();
    // The first fifth of a second is the acceleration: 0.4 m of the 4 m of one second are lost.
    world.simulate({.forward = 1.0f}, 1.0);
    REQUIRE_THAT(world.position().z, WithinAbs(-3.6, 0.1));
    REQUIRE_THAT(world.position().x, WithinAbs(0.0, 1e-3));
    REQUIRE(world.onGround());

    const float before = world.position().z;
    world.simulate({.forward = 1.0f, .sprint = true}, 1.0);
    REQUIRE_THAT(before - world.position().z, WithinAbs(6.8, 0.15));
    // It faces where it goes: -Z.
    const glm::vec3 facing =
        world.registry.get<world::Transform>(world.player).rotation * core::kWorldForward;
    REQUIRE_THAT(facing.z, WithinAbs(-1.0, 1e-3));
}

TEST_CASE("The player's jump rises 1.2 m", "[game]") {
    PlayerWorld world;
    world.start();
    const float ground = world.position().y;
    const float highest = world.simulate({.jump = true}, 1.5);
    REQUIRE_THAT(highest - ground, WithinAbs(1.2, 0.03));
    REQUIRE(world.onGround());
    REQUIRE_THAT(world.position().y, WithinAbs(ground, 0.01));
}

TEST_CASE("Holding nothing but a jump pressed in the air does not jump again", "[game]") {
    PlayerWorld world;
    world.start();
    (void)world.simulate({.jump = true}, 0.3);
    REQUIRE_FALSE(world.onGround());
    const float before = world.position().y;
    // Asked again near the top: the feet only follow the first jump's arc, below 1.2 m.
    const float highest = world.simulate({.jump = true}, 1.2);
    REQUIRE(highest < 1.25f);
    REQUIRE(highest >= before);
    REQUIRE(world.onGround());
}

// AC-07.
TEST_CASE("The player jumps onto the 1 m platform", "[game]") {
    PlayerWorld world;
    // Near face at z = -4, top at y = 1.
    world.addStatic({0.0f, 0.5f, -7.0f}, {6.0f, 1.0f, 6.0f});
    world.start();

    // Walks to the platform; it is too tall to step on, the character stops against it.
    world.simulate({.forward = 1.0f}, 1.5);
    REQUIRE_THAT(world.position().z, WithinAbs(-4.0 + 0.3, 0.05));
    REQUIRE_THAT(world.position().y, WithinAbs(0.0, 0.05));

    // Jumps while pushing forward: lands on the top.
    world.simulate({.forward = 1.0f, .jump = true}, 1.5);
    REQUIRE(world.onGround());
    REQUIRE_THAT(world.position().y, WithinAbs(1.0, 0.05));
    REQUIRE(world.position().z < -4.5f);
}

// AC-08.
TEST_CASE("The running player is stopped by the 10 cm wall", "[game]") {
    PlayerWorld world;
    // Near face at z = -5.
    world.addStatic({0.0f, 1.5f, -5.05f}, {8.0f, 3.0f, 0.1f});
    world.start();
    world.simulate({.forward = 1.0f, .sprint = true}, 3.0);
    REQUIRE_THAT(world.position().z, WithinAbs(-5.0 + 0.3, 0.05));
    // Jumping against it does not help.
    world.simulate({.forward = 1.0f, .sprint = true, .jump = true}, 1.5);
    REQUIRE(world.position().z > -5.0f + 0.25f);
}

// AC-09.
TEST_CASE("The player pushes a crate by walking into it", "[game]") {
    PlayerWorld world;
    const entt::entity crate = world.addCrate({0.0f, 0.5f, -3.0f});
    world.start();
    world.simulate({.forward = 1.0f}, 3.0);
    const glm::vec3 moved = world.registry.get<world::Transform>(crate).position;
    REQUIRE(moved.z < -4.0f);
    REQUIRE_THAT(moved.y, WithinAbs(0.5, 0.05));
    // The player stays behind the crate.
    REQUIRE(world.position().z > moved.z + 0.5f + 0.3f - 0.05f);
}

TEST_CASE("A player that receives no input stays where it is", "[game]") {
    PlayerWorld world;
    world.start();
    const glm::vec3 before = world.position();
    world.simulate({}, 2.0);
    REQUIRE_THAT(glm::distance(world.position(), before), WithinAbs(0.0, 1e-3));
    REQUIRE(world.onGround());
}
