#include "Core/Logger.h"
#include "Resources/Primitives.h"
#include "Resources/ResourceCache.h"
#include "World/RenderExtraction.h"
#include "World/Rotation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <filesystem>

using namespace apex::world;
using apex::graphics::RenderScene;
using apex::resources::ModelHandle;
using apex::resources::ModelPartRef;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;

// A cache without files, and a registry with a camera at a given place.
struct Fixture {
    Fixture() : cache(std::filesystem::path("unused"), logger) {
        cube = cache.addMesh("Cube", apex::resources::makeCube(1.0f));
        grey = cache.addMaterial("Grey", {});
        const ModelPartRef part{.mesh = cube, .material = grey};
        cubeModel = cache.addModel(std::span(&part, 1));
        camera = registry.create();
        registry.emplace<Transform>(camera);
        registry.emplace<Camera>(camera);
    }

    [[nodiscard]] entt::entity addObject(ModelHandle model, const Transform& transform) {
        const entt::entity entity = registry.create();
        registry.emplace<Transform>(entity, transform);
        registry.emplace<MeshRenderer>(entity, model);
        return entity;
    }

    [[nodiscard]] RenderScene extract(float alpha = 1.0f) const {
        RenderScene scene;
        extractRenderScene(registry, cache, camera, alpha, scene);
        return scene;
    }

    apex::core::Logger logger;
    apex::resources::ResourceCache cache;
    entt::registry registry;
    apex::resources::MeshHandle cube;
    apex::resources::MaterialHandle grey;
    ModelHandle cubeModel;
    entt::entity camera = entt::null;
};

[[nodiscard]] glm::vec3 origin(const glm::mat4& model) {
    return glm::vec3(model[3]);
}

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

} // namespace

TEST_CASE("Objects are extracted relative to the camera, exactly even far from the origin", "[world]") {
    Fixture fixture;
    fixture.registry.get<Transform>(fixture.camera).position = {10000.0f, 1.0f, 10005.0f};
    (void)fixture.addObject(fixture.cubeModel, {.position = {10000.0f, 0.0f, 10000.0f}});

    const RenderScene scene = fixture.extract();
    REQUIRE(scene.objects.size() == 1);
    REQUIRE(origin(scene.objects[0].modelRelativeToCamera) == glm::vec3(0.0f, -1.0f, -5.0f));
    REQUIRE(scene.objects[0].mesh == fixture.cube);
    REQUIRE(scene.objects[0].material == fixture.grey);
}

TEST_CASE("The extracted camera is a pure rotation with the camera's settings", "[world]") {
    Fixture fixture;
    fixture.registry.get<Transform>(fixture.camera) =
        Transform{.position = {5.0f, 6.0f, 7.0f}, .rotation = rotationFromEulerDegrees({0.0f, 90.0f, 0.0f})};
    fixture.registry.get<Camera>(fixture.camera) =
        Camera{.verticalFovRadians = 1.0f, .nearPlane = 0.5f, .farPlane = 50.0f};

    const RenderScene scene = fixture.extract();
    REQUIRE(scene.camera.view[3] == glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    // Turned a quarter to the left, the camera faces -X: that direction ends up straight ahead.
    requireNear(glm::vec3(scene.camera.view * glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f)), apex::core::kWorldForward);
    REQUIRE(scene.camera.verticalFovRadians == 1.0f);
    REQUIRE(scene.camera.nearPlane == 0.5f);
    REQUIRE(scene.camera.farPlane == 50.0f);
}

TEST_CASE("Without a camera entity the view is from the origin, with default settings", "[world]") {
    Fixture fixture;
    (void)fixture.addObject(fixture.cubeModel, {.position = {1.0f, 2.0f, 3.0f}});
    RenderScene scene;
    extractRenderScene(fixture.registry, fixture.cache, entt::null, 1.0f, scene);
    REQUIRE(scene.camera.view == glm::mat4(1.0f));
    REQUIRE(scene.camera.verticalFovRadians == Camera{}.verticalFovRadians);
    REQUIRE(origin(scene.objects[0].modelRelativeToCamera) == glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_CASE("Transforms are interpolated between the last two steps", "[world]") {
    Fixture fixture;
    const entt::entity object = fixture.addObject(fixture.cubeModel, {});
    rememberTransforms(fixture.registry);
    fixture.registry.get<Transform>(object) =
        Transform{.position = {2.0f, 0.0f, 0.0f},
                  .rotation = rotationFromEulerDegrees({0.0f, 90.0f, 0.0f}),
                  .scale = glm::vec3(3.0f)};

    const glm::mat4 middle = fixture.extract(0.5f).objects[0].modelRelativeToCamera;
    requireNear(origin(middle), {1.0f, 0.0f, 0.0f});
    // Halfway: turned 45 degrees and scaled by 2. The object's +X axis, scaled, points between +X and -Z.
    const float diagonal = 2.0f * std::sqrt(0.5f);
    requireNear(glm::vec3(middle[0]), {diagonal, 0.0f, -diagonal});

    requireNear(origin(fixture.extract(0.0f).objects[0].modelRelativeToCamera), {0.0f, 0.0f, 0.0f});
    requireNear(origin(fixture.extract(1.0f).objects[0].modelRelativeToCamera), {2.0f, 0.0f, 0.0f});
}

TEST_CASE("The camera is interpolated like any other entity", "[world]") {
    Fixture fixture;
    (void)fixture.addObject(fixture.cubeModel, {});
    rememberTransforms(fixture.registry);
    fixture.registry.get<Transform>(fixture.camera).position = {0.0f, 0.0f, 4.0f};
    // The object was remembered too and did not move: only the camera's half step shows.
    requireNear(origin(fixture.extract(0.5f).objects[0].modelRelativeToCamera), {0.0f, 0.0f, -2.0f});
}

TEST_CASE("An entity without a previous transform is drawn where it is", "[world]") {
    Fixture fixture;
    (void)fixture.addObject(fixture.cubeModel, {.position = {4.0f, 0.0f, 0.0f}});
    requireNear(origin(fixture.extract(0.25f).objects[0].modelRelativeToCamera), {4.0f, 0.0f, 0.0f});
}

TEST_CASE("A model gives one object per part, all at the entity's transform", "[world]") {
    Fixture fixture;
    const auto red = fixture.cache.addMaterial("Red", {.baseColorFactor = {1.0f, 0.0f, 0.0f, 1.0f}});
    const std::array parts{ModelPartRef{.mesh = fixture.cube, .material = fixture.grey},
                           ModelPartRef{.mesh = fixture.cube, .material = red}};
    (void)fixture.addObject(fixture.cache.addModel(parts), {.position = {0.0f, 3.0f, 0.0f}});

    const RenderScene scene = fixture.extract();
    REQUIRE(scene.objects.size() == 2);
    REQUIRE(scene.objects[0].material == fixture.grey);
    REQUIRE(scene.objects[1].material == red);
    REQUIRE(scene.objects[0].modelRelativeToCamera == scene.objects[1].modelRelativeToCamera);
}

TEST_CASE("A material override replaces the material of every part", "[world]") {
    Fixture fixture;
    const auto gold = fixture.cache.addMaterial("Gold", {.metallicFactor = 1.0f});
    const entt::entity object = fixture.addObject(fixture.cubeModel, {});
    fixture.registry.get<MeshRenderer>(object).materialOverride = gold;
    REQUIRE(fixture.extract().objects[0].material == gold);
}

TEST_CASE("A model that is not ready is drawn as the fallback, checker included", "[world]") {
    Fixture fixture;
    const entt::entity object = fixture.addObject(ModelHandle{}, {});
    fixture.registry.get<MeshRenderer>(object).materialOverride = fixture.grey;

    const RenderScene scene = fixture.extract();
    REQUIRE(scene.objects.size() == 1);
    REQUIRE(scene.objects[0].mesh == fixture.cache.fallbackMesh());
    REQUIRE(scene.objects[0].material == fixture.cache.fallbackMaterial());
}

TEST_CASE("The light and the ambient term are extracted", "[world]") {
    Fixture fixture;
    REQUIRE(fixture.extract().light.intensity == 0.0f);
    REQUIRE(fixture.extract().ambient == SceneEnvironment{}.ambient);

    const entt::entity sun = fixture.registry.create();
    fixture.registry.emplace<DirectionalLight>(
        sun,
        DirectionalLight{.direction = {0.0f, -2.0f, 0.0f}, .color = {1.0f, 0.9f, 0.8f}, .intensity = 3.0f});
    fixture.registry.ctx().emplace<SceneEnvironment>(SceneEnvironment{.ambient = {0.1f, 0.2f, 0.3f}});

    const RenderScene scene = fixture.extract();
    // The component says where the light goes; the renderer wants where it comes from.
    requireNear(scene.light.directionToLight, {0.0f, 1.0f, 0.0f});
    REQUIRE(scene.light.color == glm::vec3(1.0f, 0.9f, 0.8f));
    REQUIRE(scene.light.intensity == 3.0f);
    REQUIRE(scene.ambient == glm::vec3(0.1f, 0.2f, 0.3f));
}

TEST_CASE("rememberTransforms keeps the state of the step that just ended", "[world]") {
    Fixture fixture;
    const entt::entity object = fixture.addObject(fixture.cubeModel, {.position = {1.0f, 0.0f, 0.0f}});
    rememberTransforms(fixture.registry);
    fixture.registry.get<Transform>(object).position = {2.0f, 0.0f, 0.0f};
    REQUIRE(fixture.registry.get<PreviousTransform>(object).value.position == glm::vec3(1.0f, 0.0f, 0.0f));
    rememberTransforms(fixture.registry);
    REQUIRE(fixture.registry.get<PreviousTransform>(object).value.position == glm::vec3(2.0f, 0.0f, 0.0f));
}
