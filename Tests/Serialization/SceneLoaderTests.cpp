#include "Core/Logger.h"
#include "Resources/ResourceCache.h"
#include "Serialization/SceneJson.h"
#include "Serialization/SceneLoader.h"
#include "World/Components.h"
#include "World/Rotation.h"
#include "World/SceneQueries.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <filesystem>

using namespace apex;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

// A cache over Assets/ and an empty registry.
struct Fixture {
    Fixture() : cache(std::filesystem::path(APEX_ASSETS_DIR), logger) {}

    void load(std::string_view text) {
        const auto scene = serialization::parseScene(text, "Scenes/test.json");
        if (!scene) {
            FAIL(scene.error().message);
        }
        serialization::instantiateScene(*scene, registry, cache);
    }

    core::Logger logger;
    resources::ResourceCache cache;
    entt::registry registry;
};

} // namespace

TEST_CASE("A scene becomes entities with their components", "[serialization]") {
    Fixture fixture;
    fixture.load(R"({
        "version": 1,
        "ambient": [0.1, 0.2, 0.3],
        "entities": [
            { "id": "crate", "name": "Caisse",
              "transform": { "position": [2, 0.5, -3], "rotation_deg": [0, 90, 0], "scale": [1, 2, 1] },
              "mesh": "Models/crate.gltf",
              "collider": { "shape": "box", "half_extents": [0.5, 0.5, 0.5] },
              "rigid_body": { "type": "dynamic", "mass": 20 } },
            { "id": "sun", "light": { "direction": [0, -2, 0], "color": [1, 0.9, 0.8], "intensity": 3 } },
            { "id": "camera", "transform": { "position": [0, 2.5, 7] }, "camera": { "fov_deg": 90, "near": 0.5, "far": 50 } }
        ]
    })");
    entt::registry& registry = fixture.registry;
    REQUIRE(world::sceneEntityCount(registry) == 3);
    REQUIRE(world::sceneEnvironment(registry).ambient == glm::vec3(0.1f, 0.2f, 0.3f));

    const entt::entity crate = world::findBySceneId(registry, "crate");
    REQUIRE(registry.get<world::Name>(crate).value == "Caisse");
    const world::Transform& transform = registry.get<world::Transform>(crate);
    REQUIRE(transform.position == glm::vec3(2.0f, 0.5f, -3.0f));
    REQUIRE(transform.scale == glm::vec3(1.0f, 2.0f, 1.0f));
    // Degrees in the file, a quaternion in memory: a quarter turn to the left.
    const glm::vec3 facing = transform.rotation * core::kWorldForward;
    REQUIRE_THAT(facing.x, WithinAbs(-1.0, 1e-5));
    REQUIRE(fixture.cache.state(registry.get<world::MeshRenderer>(crate).model) ==
            resources::ResourceState::Ready);
    REQUIRE(registry.get<world::MeshRenderer>(crate).materialOverride == resources::MaterialHandle{});
    REQUIRE(registry.get<world::Collider>(crate).shape == world::ColliderShape::Box);
    REQUIRE(registry.get<world::RigidBody>(crate).mass == 20.0f);

    const entt::entity sun = world::findBySceneId(registry, "sun");
    // Normalized on the way in.
    REQUIRE(registry.get<world::DirectionalLight>(sun).direction == glm::vec3(0.0f, -1.0f, 0.0f));
    REQUIRE(registry.get<world::DirectionalLight>(sun).intensity == 3.0f);
    REQUIRE_FALSE(registry.all_of<world::Transform>(sun));

    const world::Camera& camera = registry.get<world::Camera>(world::findBySceneId(registry, "camera"));
    REQUIRE_THAT(camera.verticalFovRadians, WithinAbs(glm::half_pi<float>(), 1e-6));
    REQUIRE(camera.nearPlane == 0.5f);
    REQUIRE(camera.farPlane == 50.0f);
}

TEST_CASE("A scene material becomes a material override in the cache", "[serialization]") {
    Fixture fixture;
    fixture.load(R"({"version": 1, "entities": [
        {"id": "gold", "mesh": "Models/crate.gltf", "material": {"base_color": [1, 0.8, 0.3], "metallic": 1, "roughness": 0.25}}
    ]})");
    const entt::entity gold = world::findBySceneId(fixture.registry, "gold");
    const world::MeshRenderer& renderer = fixture.registry.get<world::MeshRenderer>(gold);
    const resources::MaterialData* material = fixture.cache.materials().find(renderer.materialOverride);
    REQUIRE(material != nullptr);
    REQUIRE(material->baseColorFactor == glm::vec4(1.0f, 0.8f, 0.3f, 1.0f));
    REQUIRE(material->metallicFactor == 1.0f);
    REQUIRE(material->roughnessFactor == 0.25f);
    // A mesh without a transform is still placed, at the origin.
    REQUIRE(fixture.registry.get<world::Transform>(gold) == world::Transform{});
}

TEST_CASE("A missing model does not fail the scene: its entity gets the fallback", "[serialization]") {
    Fixture fixture;
    fixture.load(R"({"version": 1, "entities": [{"id": "ghost", "mesh": "Models/absent.glb"}]})");
    const world::MeshRenderer& renderer =
        fixture.registry.get<world::MeshRenderer>(world::findBySceneId(fixture.registry, "ghost"));
    REQUIRE(fixture.cache.state(renderer.model) == resources::ResourceState::Failed);
    REQUIRE(fixture.cache.parts(renderer.model)[0].mesh == fixture.cache.fallbackMesh());
}

TEST_CASE("Two entities with the same mesh share the loaded model", "[serialization]") {
    Fixture fixture;
    fixture.load(R"({"version": 1, "entities": [
        {"id": "a", "mesh": "Models/crate.gltf"}, {"id": "b", "mesh": "Models\\crate.gltf"}
    ]})");
    const auto modelOf = [&](std::string_view id) {
        return fixture.registry.get<world::MeshRenderer>(world::findBySceneId(fixture.registry, id)).model;
    };
    REQUIRE(modelOf("a") == modelOf("b"));
}

TEST_CASE("An invalid scene file creates nothing and names the file", "[serialization]") {
    Fixture fixture;
    const std::filesystem::path data(APEX_TEST_DATA_DIR);

    const auto missing = serialization::loadSceneFile(data / "Scenes/absent.json", "Scenes/absent.json",
                                                      fixture.registry, fixture.cache);
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == core::ErrorCode::NotFound);
    REQUIRE_THAT(missing.error().message, ContainsSubstring("Scenes/absent.json"));

    // Valid first entity, broken second one: the first must not be created either.
    const auto broken = serialization::loadSceneFile(data / "Scenes/broken.json", "Scenes/broken.json",
                                                     fixture.registry, fixture.cache);
    REQUIRE_FALSE(broken.has_value());
    REQUIRE_THAT(broken.error().message, ContainsSubstring("Scenes/broken.json"));
    REQUIRE_THAT(broken.error().message, ContainsSubstring("entity 'second'"));
    REQUIRE_THAT(broken.error().message, ContainsSubstring("field 'transform.position'"));
    REQUIRE(world::sceneEntityCount(fixture.registry) == 0);
}

TEST_CASE("A valid scene file is loaded from disk", "[serialization]") {
    Fixture fixture;
    const std::filesystem::path data(APEX_TEST_DATA_DIR);
    const auto loaded = serialization::loadSceneFile(data / "Scenes/small.json", "Scenes/small.json",
                                                     fixture.registry, fixture.cache);
    if (!loaded) {
        FAIL(loaded.error().message);
    }
    REQUIRE(world::sceneEntityCount(fixture.registry) == 2);
    REQUIRE(world::findBySceneId(fixture.registry, "first") != entt::entity{entt::null});
}

TEST_CASE("The scene's environment is loaded into the registry's context", "[serialization]") {
    Fixture fixture;
    fixture.load(R"({"version": 1, "environment": {"path": "Environments/sky.hdr", "intensity": 0.5},
                     "entities": []})");
    const world::SceneEnvironment environment = world::sceneEnvironment(fixture.registry);
    REQUIRE(fixture.cache.state(environment.environment) == resources::ResourceState::Ready);
    REQUIRE(environment.environmentIntensity == 0.5f);
}

TEST_CASE("A missing environment fails alone: the scene still loads", "[serialization]") {
    Fixture fixture;
    fixture.load(R"({"version": 1, "environment": {"path": "Environments/missing.hdr"},
                     "entities": [{ "id": "sun", "light": { "direction": [0, -1, 0] } }]})");
    REQUIRE(world::sceneEntityCount(fixture.registry) == 1);
    const world::SceneEnvironment environment = world::sceneEnvironment(fixture.registry);
    REQUIRE(fixture.cache.state(environment.environment) == resources::ResourceState::Failed);
}
