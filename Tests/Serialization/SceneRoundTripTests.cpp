#include "Core/Logger.h"
#include "Resources/Primitives.h"
#include "Resources/ResourceCache.h"
#include "Serialization/SceneJson.h"
#include "Serialization/SceneLoader.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace apex;
using namespace apex::serialization;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

// Every component at least once, with values that are not the defaults.
constexpr std::string_view kFullScene = R"({
    "version": 1,
    "ambient": [0.1, 0.2, 0.3],
    "entities": [
        { "id": "camera", "transform": { "position": [0, 2.5, 7], "rotation_deg": [-15, 30, 0] },
          "camera": { "fov_deg": 65, "near": 0.25, "far": 300 } },
        { "id": "crate", "name": "Caisse",
          "transform": { "position": [2, 0.5, -3], "rotation_deg": [10, 45, -20], "scale": [1, 2, 0.5] },
          "mesh": "Models/crate.gltf",
          "material": { "base_color": [1, 0.8, 0.3], "metallic": 1, "roughness": 0.25 },
          "collider": { "shape": "box", "half_extents": [0.5, 1, 0.25] },
          "rigid_body": { "type": "dynamic", "mass": 20 } },
        { "id": "floor", "mesh": "Models/reference_cube.glb", "transform": { "scale": [30, 0.2, 30] },
          "collider": { "shape": "box", "half_extents": [15, 0.1, 15] }, "rigid_body": { "type": "static" } },
        { "id": "pill", "transform": {}, "collider": { "shape": "capsule", "radius": 0.3, "half_height": 0.6 } },
        { "id": "ball", "transform": { "position": [0, 5, 0] }, "collider": { "shape": "sphere", "radius": 0.1 } },
        { "id": "sun", "light": { "direction": [0, -1, 0], "color": [1, 0.95, 0.88], "intensity": 3 } }
    ]
})";

[[nodiscard]] SceneDescription parseOrFail(std::string_view text) {
    auto scene = parseScene(text, "Scenes/round_trip.json");
    if (!scene) {
        FAIL(scene.error().message);
    }
    return std::move(*scene);
}

[[nodiscard]] SceneDescription sortedById(SceneDescription scene) {
    std::ranges::sort(scene.entities, {}, &EntityDesc::id);
    return scene;
}

void requireNear(const glm::vec3& actual, const glm::vec3& expected, float tolerance) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, tolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, tolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, tolerance));
}

} // namespace

TEST_CASE("A scene written and read back is the same scene", "[serialization]") {
    const SceneDescription original = parseOrFail(kFullScene);
    const std::string written = writeScene(original);
    REQUIRE(parseOrFail(written) == original);
}

TEST_CASE("Writing a scene twice gives the same text", "[serialization]") {
    const SceneDescription scene = parseOrFail(kFullScene);
    const std::string first = writeScene(scene);
    REQUIRE(writeScene(scene) == first);
    REQUIRE(writeScene(parseOrFail(first)) == first);
}

TEST_CASE("Numbers are written in their shortest exact form", "[serialization]") {
    SceneDescription scene;
    scene.ambient = {0.1f, 0.25f, 3.0f};
    const std::string written = writeScene(scene);
    REQUIRE_THAT(written, ContainsSubstring("0.1"));
    REQUIRE_THAT(written, !ContainsSubstring("0.10000000"));
    REQUIRE_THAT(written, ContainsSubstring("\"version\": 1"));
    REQUIRE(parseOrFail(written).ambient == scene.ambient);
}

TEST_CASE("A scene survives the trip through the registry", "[serialization]") {
    core::Logger logger;
    resources::ResourceCache cache(std::filesystem::path(APEX_ASSETS_DIR), logger);
    entt::registry registry;
    const SceneDescription original = sortedById(parseOrFail(kFullScene));
    instantiateScene(original, registry, cache);

    const SceneDescription described = describeScene(registry, cache);
    REQUIRE(described.ambient == original.ambient);
    REQUIRE(described.entities.size() == original.entities.size());
    for (std::size_t i = 0; i < original.entities.size(); ++i) {
        const EntityDesc& before = original.entities[i];
        const EntityDesc& after = described.entities[i];
        CAPTURE(before.id);
        REQUIRE(after.id == before.id);
        REQUIRE(after.name == before.name);
        REQUIRE(after.mesh == before.mesh);
        REQUIRE(after.material == before.material);
        REQUIRE(after.collider == before.collider);
        REQUIRE(after.rigidBody == before.rigidBody);
        REQUIRE(after.light == before.light);
        // Angles pass through a quaternion and the field of view through radians: equal up to rounding.
        REQUIRE(after.transform.has_value() == before.transform.has_value());
        if (before.transform) {
            REQUIRE(after.transform->position == before.transform->position);
            REQUIRE(after.transform->scale == before.transform->scale);
            requireNear(after.transform->rotationDegrees, before.transform->rotationDegrees, 1e-3f);
        }
        REQUIRE(after.camera.has_value() == before.camera.has_value());
        if (before.camera) {
            REQUIRE_THAT(after.camera->fovDegrees, WithinAbs(before.camera->fovDegrees, 1e-4));
            REQUIRE(after.camera->nearPlane == before.camera->nearPlane);
            REQUIRE(after.camera->farPlane == before.camera->farPlane);
        }
    }
}

TEST_CASE("What the registry gives can be written and read again", "[serialization]") {
    core::Logger logger;
    resources::ResourceCache cache(std::filesystem::path(APEX_ASSETS_DIR), logger);
    entt::registry registry;
    instantiateScene(parseOrFail(kFullScene), registry, cache);
    const SceneDescription described = describeScene(registry, cache);
    REQUIRE(parseOrFail(writeScene(described)) == described);
}

TEST_CASE("Entities that do not come from a file are described without what a file cannot name",
          "[serialization]") {
    core::Logger logger;
    resources::ResourceCache cache(std::filesystem::path(APEX_ASSETS_DIR), logger);
    entt::registry registry;
    // No SceneId: not part of the scene.
    registry.emplace<world::Transform>(registry.create());
    // A model made in code has no path: the entity is kept, its mesh is left out.
    const resources::ModelPartRef part{.mesh = cache.addMesh("Cube", resources::makeCube(1.0f)),
                                       .material = cache.defaultMaterial()};
    const entt::entity generated = registry.create();
    registry.emplace<world::SceneId>(generated, "generated");
    registry.emplace<world::Transform>(generated);
    registry.emplace<world::MeshRenderer>(generated, cache.addModel(std::span(&part, 1)));

    const SceneDescription described = describeScene(registry, cache);
    REQUIRE(described.entities.size() == 1);
    REQUIRE(described.entities[0].id == "generated");
    REQUIRE_FALSE(described.entities[0].mesh.has_value());
    REQUIRE(described.entities[0].transform.has_value());
}
