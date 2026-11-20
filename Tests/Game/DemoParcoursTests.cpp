#include "GameSystems.h"

#include "../Physics/PhysicsTestWorld.h"
#include "Core/Logger.h"
#include "Resources/ResourceCache.h"
#include "Serialization/EngineConfig.h"
#include "Serialization/SceneLoader.h"
#include "World/PhysicsSync.h"
#include "World/SceneQueries.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string_view>

using namespace apex;
using namespace apex::game;
using namespace apex::tests;
using Catch::Matchers::WithinAbs;

namespace {

// Camera yaws that make "forward" point along a world axis.
constexpr float kTowardsMinusZ = 0.0f;
constexpr float kTowardsMinusX = glm::radians(90.0f);
constexpr float kTowardsPlusX = glm::radians(-90.0f);

// Assets/Scenes/demo.json with the settings of Config/engine.json, simulated the way the demo does it:
// the game's step, then the physics. The validation route of spec 6.2, without a window.
struct DemoParcours {
    // `start`: where the player's feet are put before the physics objects are created.
    explicit DemoParcours(const glm::vec3& start) {
        const auto config = serialization::loadEngineConfigFile(
            std::filesystem::path(APEX_CONFIG_DIR) / "engine.json", "Config/engine.json");
        REQUIRE(config.has_value());
        physics = std::make_unique<PhysicsTestWorld>(physics::PhysicsSettings{.gravity = config->gravity});
        settings.walkSpeed = config->characterWalkSpeed;
        settings.runSpeed = config->characterRunSpeed;
        settings.jumpHeight = config->characterJumpHeight;

        cache = std::make_unique<resources::ResourceCache>(std::filesystem::path(APEX_ASSETS_DIR), logger);
        const auto loaded = serialization::loadSceneFile(cache->root() / "Scenes/demo.json",
                                                         "Scenes/demo.json", registry, *cache);
        REQUIRE(loaded.has_value());
        const auto entities = attachGameComponents(
            registry, world::CharacterBody{.height = config->characterHeight,
                                           .radius = config->characterRadius,
                                           .maxSlopeRadians = glm::radians(config->characterMaxSlopeDegrees),
                                           .stepHeight = config->characterStepHeight,
                                           .maxPushForce = config->characterPushForce});
        REQUIRE(entities.has_value());
        REQUIRE(entities->player != entt::null);
        player = entities->player;
        registry.get<world::Transform>(player).position = start;
        const auto created = world::createPhysicsObjects(registry, **physics);
        if (!created) {
            FAIL(created.error().message);
        }
        simulate({}, 0.25);
    }

    // A jump is asked on the first step only.
    void simulate(PlayerInput input, double seconds) {
        const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
        for (int i = 0; i < steps; ++i) {
            stepPlayerEntity(registry, player, input, (*physics)->gravity(), kPhysicsStep, settings);
            world::stepPhysics(registry, **physics, kPhysicsStep);
            input.jump = false;
        }
    }

    [[nodiscard]] glm::vec3 position(std::string_view id) const {
        const entt::entity entity = world::findBySceneId(registry, id);
        REQUIRE(entity != entt::null);
        return registry.get<world::Transform>(entity).position;
    }
    [[nodiscard]] glm::vec3 feet() const { return registry.get<world::Transform>(player).position; }
    [[nodiscard]] bool onGround() const {
        return registry.get<world::CharacterGround>(player).state == physics::GroundState::OnGround;
    }

    core::Logger logger;
    std::unique_ptr<PhysicsTestWorld> physics;
    std::unique_ptr<resources::ResourceCache> cache;
    entt::registry registry;
    entt::entity player = entt::null;
    PlayerSettings settings;
};

constexpr glm::vec3 kSceneStart{0.0f, 0.0f, 4.0f};
constexpr std::array<std::string_view, 3> kStack{"crate_01", "crate_02", "crate_03"};

} // namespace

TEST_CASE("The demo scene gives Physics its colliders and its character", "[game]") {
    const DemoParcours demo(kSceneStart);
    // 12 static boxes, 10 spheres and 5 crates.
    REQUIRE((*demo.physics)->bodyCount() == 27);
    REQUIRE((*demo.physics)->characterCount() == 1);
    REQUIRE(demo.onGround());
    REQUIRE_THAT(demo.feet().y, WithinAbs(0.0, 0.05));
}

// AC-09, on the real scene.
TEST_CASE("The demo's stack of three crates stays in place for 10 s, and every crate rests", "[game]") {
    DemoParcours demo(kSceneStart);
    std::array<glm::vec3, 3> before{};
    for (std::size_t i = 0; i < kStack.size(); ++i) {
        before[i] = demo.position(kStack[i]);
    }
    demo.simulate({}, 10.0);
    for (std::size_t i = 0; i < kStack.size(); ++i) {
        const glm::vec3 after = demo.position(kStack[i]);
        REQUIRE_THAT(after.x, WithinAbs(before[i].x, 0.01));
        REQUIRE_THAT(after.z, WithinAbs(before[i].z, 0.01));
        REQUIRE_THAT(after.y, WithinAbs(0.5 + static_cast<double>(i), 0.03));
    }
    // Nothing fell through the floor, and nothing is still moving.
    for (const std::string_view crate : {"crate_04", "crate_05"}) {
        REQUIRE_THAT(demo.position(crate).y, WithinAbs(0.5, 0.03));
    }
    REQUIRE((*demo.physics)->activeBodyCount() == 0);
}

// Spec 6.2, step 4.
TEST_CASE("In the demo, the player walks up both ramps to the 3 m platform", "[game]") {
    DemoParcours demo({-3.0f, 0.0f, 6.0f});
    demo.simulate({.forward = 1.0f, .cameraYaw = kTowardsMinusX}, 3.0);
    // On the 1.5 m platform, or already on the second ramp.
    REQUIRE(demo.feet().y > 1.4f);
    // Walks on until it is well onto the top platform (x from -20 to -16), then stops.
    for (int i = 0; i < 40 && demo.feet().x > -17.5f; ++i) {
        demo.simulate({.forward = 1.0f, .cameraYaw = kTowardsMinusX}, 0.1);
    }
    demo.simulate({}, 0.5);
    REQUIRE(demo.onGround());
    REQUIRE_THAT(demo.feet().y, WithinAbs(3.0, 0.05));
    REQUIRE(demo.feet().x < -16.5f);
    REQUIRE_THAT(demo.feet().z, WithinAbs(6.0, 0.05));
}

// Spec 6.2, step 5; AC-07.
TEST_CASE("In the demo, the player jumps onto the 1 m platform", "[game]") {
    DemoParcours demo({-6.0f, 0.0f, 15.5f});
    // Too tall to walk on: the character stops against its face, at z = 13.
    demo.simulate({.forward = 1.0f, .cameraYaw = kTowardsMinusZ}, 1.5);
    REQUIRE_THAT(demo.feet().z, WithinAbs(13.0 + 0.3, 0.05));
    REQUIRE_THAT(demo.feet().y, WithinAbs(0.0, 0.05));

    // Jumps while pushing forward, then lets go: the platform is 4 m deep.
    demo.simulate({.forward = 1.0f, .jump = true, .cameraYaw = kTowardsMinusZ}, 1.0);
    demo.simulate({}, 0.5);
    REQUIRE(demo.onGround());
    REQUIRE_THAT(demo.feet().y, WithinAbs(1.0, 0.05));
    REQUIRE(demo.feet().z < 12.7f);
    REQUIRE(demo.feet().z > 9.0f);
}

// Spec 6.2, step 6; AC-08.
TEST_CASE("In the demo, the running player does not go through the 10 cm wall", "[game]") {
    DemoParcours demo({4.0f, 0.0f, 7.5f});
    demo.simulate({.forward = 1.0f, .sprint = true, .cameraYaw = kTowardsPlusX}, 3.0);
    // The wall's near face is at x = 11.95.
    REQUIRE_THAT(demo.feet().x, WithinAbs(11.95 - 0.3, 0.05));
    demo.simulate({.forward = 1.0f, .sprint = true, .jump = true, .cameraYaw = kTowardsPlusX}, 1.5);
    REQUIRE(demo.feet().x < 11.95f - 0.25f);
    REQUIRE(demo.position("wall") == glm::vec3(12.0f, 1.5f, 4.0f));
}

// Spec 6.2, step 7; AC-09.
TEST_CASE("In the demo, the player pushes a crate along the ground", "[game]") {
    DemoParcours demo({4.0f, 0.0f, 5.5f});
    const glm::vec3 before = demo.position("crate_05");
    demo.simulate({.forward = 1.0f, .cameraYaw = kTowardsPlusX}, 3.0);
    const glm::vec3 after = demo.position("crate_05");
    REQUIRE(after.x > before.x + 1.0f);
    REQUIRE_THAT(after.y, WithinAbs(0.5, 0.05));
    // Still behind the crate.
    REQUIRE(demo.feet().x < after.x - 0.5f);
}

// Spec 6.2, step 7.
TEST_CASE("In the demo, walking or running into the stack of crates makes it fall", "[game]") {
    const bool sprint = GENERATE(false, true);
    DemoParcours demo({5.5f, 0.0f, 4.0f});
    demo.simulate({.forward = 1.0f, .sprint = sprint, .cameraYaw = kTowardsMinusZ}, 3.0);
    demo.simulate({}, 3.0);
    // The top crate is no longer 2.5 m up: it is on the ground or leaning on the others.
    REQUIRE(demo.position("crate_03").y < 2.0f);
    for (const std::string_view crate : kStack) {
        REQUIRE(demo.position(crate).y > 0.4f);
    }
}
