#include "GameSystems.h"

#include "World/Components.h"
#include "World/Rotation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>

using namespace apex;
using namespace apex::game;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kStep = 1.0 / 60.0;
constexpr float kTolerance = 1e-4f;

// A registry with the scene entity "camera" at the given pose.
[[nodiscard]] entt::entity addCamera(entt::registry& registry, const glm::vec3& position,
                                     const glm::vec3& degrees) {
    const entt::entity camera = registry.create();
    registry.emplace<world::SceneId>(camera, std::string(kCameraId));
    registry.emplace<world::Transform>(
        camera, world::Transform{.position = position, .rotation = world::rotationFromEulerDegrees(degrees)});
    registry.emplace<world::Camera>(camera);
    return camera;
}

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

} // namespace

TEST_CASE("The camera entity gets a controller that starts at its orientation", "[game]") {
    entt::registry registry;
    const entt::entity camera = addCamera(registry, {0.0f, 2.5f, 7.0f}, {-15.0f, 30.0f, 0.0f});

    const auto attached = attachGameComponents(registry);
    REQUIRE(attached.has_value());
    REQUIRE(*attached == camera);
    const FreeCameraController& controller = registry.get<FreeCameraController>(camera);
    REQUIRE_THAT(controller.yaw, WithinAbs(glm::radians(30.0f), kTolerance));
    REQUIRE_THAT(controller.pitch, WithinAbs(glm::radians(-15.0f), kTolerance));
}

TEST_CASE("A scene without a usable camera entity is refused", "[game]") {
    entt::registry registry;
    const auto missing = attachGameComponents(registry);
    REQUIRE_FALSE(missing.has_value());
    REQUIRE_THAT(missing.error().message, ContainsSubstring("'camera'"));

    // The id alone is not enough: it must be a camera with a place.
    const entt::entity named = registry.create();
    registry.emplace<world::SceneId>(named, std::string(kCameraId));
    REQUIRE_FALSE(attachGameComponents(registry).has_value());
}

TEST_CASE("A step of the free camera moves and turns the entity's transform", "[game]") {
    entt::registry registry;
    const entt::entity camera = addCamera(registry, {0.0f, 2.0f, 0.0f}, {0.0f, 90.0f, 0.0f});
    REQUIRE(attachGameComponents(registry).has_value());
    const FreeCameraSettings settings;

    // Facing -X after a quarter turn to the left: forward for one second covers 5 m along -X.
    for (int i = 0; i < 60; ++i) {
        stepFreeCameraEntity(registry, camera, {.forward = 1.0f}, kStep, settings);
    }
    requireNear(registry.get<world::Transform>(camera).position, {-5.0f, 2.0f, 0.0f});

    // Mouse to the right: the yaw decreases and the entity's rotation follows.
    stepFreeCameraEntity(registry, camera, {.lookX = 100.0f}, kStep, settings);
    const FreeCameraController& controller = registry.get<FreeCameraController>(camera);
    REQUIRE(controller.yaw < glm::radians(90.0f));
    const glm::quat expected = freeCameraOrientation({.yaw = controller.yaw, .pitch = controller.pitch});
    REQUIRE_THAT(std::abs(glm::dot(registry.get<world::Transform>(camera).rotation, expected)),
                 WithinAbs(1.0, kTolerance));
}

TEST_CASE("Placing the camera sets its transform and its controller", "[game]") {
    entt::registry registry;
    const entt::entity camera = addCamera(registry, {}, {});
    REQUIRE(attachGameComponents(registry).has_value());
    placeFreeCameraEntity(registry, camera, {.position = {1.0f, 2.0f, 3.0f}, .yaw = 0.5f, .pitch = -0.25f});
    REQUIRE(registry.get<world::Transform>(camera).position == glm::vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(registry.get<FreeCameraController>(camera).yaw == 0.5f);
    REQUIRE(registry.get<FreeCameraController>(camera).pitch == -0.25f);
}

TEST_CASE("The spinner turns 45 degrees per second around +Y, in place", "[game]") {
    entt::registry registry;
    (void)addCamera(registry, {}, {});
    const entt::entity spinner = registry.create();
    registry.emplace<world::SceneId>(spinner, std::string(kSpinnerId));
    registry.emplace<world::Transform>(spinner, world::Transform{.position = {1.8f, 0.4f, 2.2f}});
    REQUIRE(attachGameComponents(registry).has_value());
    REQUIRE(registry.all_of<Spin>(spinner));

    for (int i = 0; i < 120; ++i) {
        stepSpinners(registry, kStep);
    }
    const world::Transform& transform = registry.get<world::Transform>(spinner);
    REQUIRE(transform.position == glm::vec3(1.8f, 0.4f, 2.2f));
    // Two seconds: a quarter turn to the left.
    requireNear(transform.rotation * core::kWorldForward, {-1.0f, 0.0f, 0.0f});
    REQUIRE_THAT(glm::length(transform.rotation), WithinAbs(1.0, 1e-5));
}

TEST_CASE("A scene without a spinner is fine", "[game]") {
    entt::registry registry;
    (void)addCamera(registry, {}, {});
    REQUIRE(attachGameComponents(registry).has_value());
    REQUIRE(registry.view<Spin>().empty());
    stepSpinners(registry, kStep);
}
