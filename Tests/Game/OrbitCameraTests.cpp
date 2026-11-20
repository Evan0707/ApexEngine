#include "GameSystems.h"

#include "../Physics/PhysicsTestWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <string>

using namespace apex;
using namespace apex::game;
using namespace apex::tests;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-4f;

void requireNear(const glm::vec3& actual, const glm::vec3& expected, float tolerance = kTolerance) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, tolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, tolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, tolerance));
}

// A camera entity and a player entity standing at `feet`; the player is not simulated: the camera only
// reads its Transform.
struct OrbitWorld {
    explicit OrbitWorld(const glm::vec3& feet = glm::vec3(0.0f)) {
        (void)physics.addFloor();
        camera = registry.create();
        registry.emplace<world::Transform>(camera);
        registry.emplace<world::Camera>(camera);
        registry.emplace<FreeCameraController>(camera);
        player = registry.create();
        registry.emplace<world::Transform>(player, world::Transform{.position = feet});
    }

    void start(float yaw = 0.0f, float pitch = 0.0f) {
        registry.emplace_or_replace<FreeCameraController>(camera, yaw, pitch);
        startOrbitCameraEntity(registry, camera, player, settings);
    }

    void step(double seconds, const OrbitCameraInput& input = {}) {
        const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
        for (int i = 0; i < steps; ++i) {
            stepOrbitCameraEntity(registry, camera, player, input, *physics, kPhysicsStep, settings);
        }
    }

    [[nodiscard]] const world::Transform& view() const { return registry.get<world::Transform>(camera); }
    [[nodiscard]] const OrbitCameraState& state() const {
        return registry.get<OrbitCameraController>(camera).state;
    }

    PhysicsTestWorld physics;
    entt::registry registry;
    entt::entity camera = entt::null;
    entt::entity player = entt::null;
    OrbitCameraSettings settings;
};

} // namespace

TEST_CASE("The orbit camera sits behind its aim point and looks at it", "[game]") {
    const OrbitCameraState level{.target = {1.0f, 1.6f, 2.0f}, .distance = 4.0f};
    requireNear(orbitCameraPosition(level), {1.0f, 1.6f, 6.0f});
    requireNear(orbitCameraForward(level), {0.0f, 0.0f, -1.0f});

    // Looking down by 30 degrees: the camera is above and behind.
    const OrbitCameraState above{
        .pitch = glm::radians(-30.0f), .target = {0.0f, 1.6f, 0.0f}, .distance = 4.0f};
    requireNear(orbitCameraPosition(above), {0.0f, 1.6f + 2.0f, 4.0f * std::cos(glm::radians(30.0f))});

    // A quarter turn to the left: it looks towards -X, from +X.
    const OrbitCameraState side{.yaw = glm::radians(90.0f), .target = {0.0f, 1.6f, 0.0f}, .distance = 4.0f};
    requireNear(orbitCameraPosition(side), {4.0f, 1.6f, 0.0f});
    requireNear(orbitCameraOrientation(side) * core::kWorldForward, {-1.0f, 0.0f, 0.0f});
}

TEST_CASE("The mouse turns the orbit camera, with a limited pitch", "[game]") {
    const OrbitCameraSettings settings;
    const OrbitCameraState start{.target = {0.0f, 1.6f, 0.0f}};
    // Mouse to the right: turns right. Mouse down: looks down.
    const OrbitCameraState turned =
        aimOrbitCamera(start, {.lookX = 100.0f, .lookY = 40.0f}, start.target, kPhysicsStep, settings);
    REQUIRE_THAT(turned.yaw, WithinAbs(-0.25, 1e-6));
    REQUIRE_THAT(turned.pitch, WithinAbs(-0.1, 1e-6));

    const OrbitCameraState up =
        aimOrbitCamera(start, {.lookY = -100000.0f}, start.target, kPhysicsStep, settings);
    REQUIRE(up.pitch == settings.maxPitch);
    const OrbitCameraState down =
        aimOrbitCamera(start, {.lookY = 100000.0f}, start.target, kPhysicsStep, settings);
    REQUIRE(down.pitch == settings.minPitch);
}

TEST_CASE("The aim point follows the character smoothly, without overshooting", "[game]") {
    const OrbitCameraSettings settings;
    OrbitCameraState state{.target = {0.0f, 1.6f, 0.0f}};
    const glm::vec3 wanted{1.0f, 1.6f, 0.0f};

    // One time constant: 63 % of the way.
    float previous = 0.0f;
    const int steps = static_cast<int>(settings.followTime / kPhysicsStep + 0.5);
    for (int i = 0; i < steps; ++i) {
        state = aimOrbitCamera(state, {}, wanted, kPhysicsStep, settings);
        REQUIRE(state.target.x > previous);
        REQUIRE(state.target.x < 1.0f);
        previous = state.target.x;
    }
    REQUIRE_THAT(state.target.x, WithinAbs(1.0 - std::exp(-1.0), 0.05));

    for (int i = 0; i < 60; ++i) {
        state = aimOrbitCamera(state, {}, wanted, kPhysicsStep, settings);
    }
    requireNear(state.target, wanted, 1e-3f);
}

TEST_CASE("The orbit camera comes closer at once and backs away progressively", "[game]") {
    const OrbitCameraSettings settings;
    // An obstacle at 1.5 m: no delay.
    REQUIRE(orbitCameraDistance(4.0f, 1.5f, kPhysicsStep, settings) == 1.5f);
    // Gone: 6 m/s back towards 4 m.
    REQUIRE_THAT(orbitCameraDistance(1.5f, std::nullopt, kPhysicsStep, settings), WithinAbs(1.6, 1e-5));
    REQUIRE(orbitCameraDistance(3.95f, std::nullopt, kPhysicsStep, settings) == 4.0f);
    // A farther obstacle lets it back away, up to the obstacle only.
    REQUIRE_THAT(orbitCameraDistance(1.5f, 1.55f, kPhysicsStep, settings), WithinAbs(1.55, 1e-6));
    REQUIRE(orbitCameraDistance(4.0f, 9.0f, kPhysicsStep, settings) == 4.0f);
    REQUIRE(orbitCameraDistance(2.0f, -1.0f, kPhysicsStep, settings) == 0.0f);
}

TEST_CASE("The third-person camera starts with the free camera's direction, 4 m behind the character",
          "[game]") {
    OrbitWorld world({2.0f, 0.0f, -3.0f});
    world.start(glm::radians(90.0f), glm::radians(-20.0f));
    world.step(kPhysicsStep);

    // Aim point 1.6 m above the feet; the camera looks towards -X and 20 degrees down.
    const glm::vec3 target{2.0f, 1.6f, -3.0f};
    requireNear(world.state().target, target);
    const glm::vec3 forward = world.view().rotation * core::kWorldForward;
    requireNear(forward, {-std::cos(glm::radians(20.0f)), -std::sin(glm::radians(20.0f)), 0.0f});
    requireNear(world.view().position, target - forward * 4.0f, 1e-3f);
}

// AC-07.
TEST_CASE("The third-person camera does not go through the floor", "[game]") {
    OrbitWorld world;
    world.start();
    // The mouse pushed all the way up: the view looks up, from as low as it can.
    world.step(1.0, {.lookY = -2000.0f});
    REQUIRE(world.state().pitch == world.settings.maxPitch);
    // The probe rests on the floor: the camera is one probe radius above it, not below.
    REQUIRE(world.view().position.y > world.settings.probeRadius - 0.02f);
    REQUIRE(world.state().distance < world.settings.distance);
    // It still looks at the aim point.
    const glm::vec3 toTarget = glm::normalize(world.state().target - world.view().position);
    requireNear(world.view().rotation * core::kWorldForward, toTarget, 1e-3f);

    // Whatever the direction around the character.
    for (int i = 0; i < 120; ++i) {
        world.step(kPhysicsStep, {.lookX = 50.0f, .lookY = -100.0f});
        REQUIRE(world.view().position.y > world.settings.probeRadius - 0.02f);
    }
}

TEST_CASE("The third-person camera stays in front of a wall behind the character", "[game]") {
    OrbitWorld world;
    // 10 cm thick, its near face 1.5 m behind the character (the camera looks towards -Z, from +Z).
    (void)world.physics.addBox({0.0f, 2.0f, 1.55f}, {5.0f, 2.0f, 0.05f}, physics::MotionType::Static);
    world.start();
    world.step(0.5);
    REQUIRE_THAT(world.view().position.z, WithinAbs(1.5 - 0.2, 0.02));
    REQUIRE_THAT(world.state().distance, WithinAbs(1.3, 0.02));
}

TEST_CASE("The third-person camera is stopped by a crate, and backs away once it is gone", "[game]") {
    OrbitWorld world;
    const physics::BodyHandle crate =
        world.physics.addBox({0.0f, 1.6f, 2.5f}, glm::vec3(0.5f), physics::MotionType::Static);
    world.start();
    world.step(0.5);
    // Near face of the crate at z = 2.
    REQUIRE_THAT(world.state().distance, WithinAbs(1.8, 0.02));

    world.physics->destroyBody(crate);
    world.step(kPhysicsStep);
    // One step later it has moved back by 10 cm, not by 2.2 m.
    REQUIRE_THAT(world.state().distance, WithinAbs(1.9, 0.03));
    world.step(1.0);
    REQUIRE(world.state().distance == world.settings.distance);
}

TEST_CASE("The third-person camera follows the character with a short lag", "[game]") {
    OrbitWorld world;
    world.start();
    world.step(0.5);
    // The character runs at 7 m/s towards -Z.
    for (int i = 0; i < 60; ++i) {
        world.registry.get<world::Transform>(world.player).position.z -=
            7.0f * static_cast<float>(kPhysicsStep);
        world.step(kPhysicsStep);
    }
    const float lag = world.state().target.z - (-7.0f);
    // Speed times the time constant: about 0.56 m behind, and never ahead.
    REQUIRE(lag > 0.3f);
    REQUIRE(lag < 0.8f);
    world.step(1.0);
    REQUIRE_THAT(world.state().target.z, WithinAbs(-7.0, 1e-3));
}

TEST_CASE("Releasing the third-person camera gives its direction back to the free camera", "[game]") {
    OrbitWorld world;
    world.start(glm::radians(10.0f), glm::radians(-5.0f));
    world.step(0.5, {.lookX = 20.0f});
    const OrbitCameraState orbit = world.state();
    const world::Transform before = world.view();

    releaseOrbitCameraEntity(world.registry, world.camera);
    REQUIRE_FALSE(world.registry.any_of<OrbitCameraController>(world.camera));
    const FreeCameraController& free = world.registry.get<FreeCameraController>(world.camera);
    REQUIRE(free.yaw == orbit.yaw);
    REQUIRE(free.pitch == orbit.pitch);
    // The camera stays where it was.
    REQUIRE(world.view() == before);
    // Releasing twice is harmless.
    releaseOrbitCameraEntity(world.registry, world.camera);
}
