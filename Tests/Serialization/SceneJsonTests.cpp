#include "Serialization/SceneJson.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <format>
#include <initializer_list>
#include <string>
#include <string_view>

using namespace apex::serialization;
using Catch::Matchers::ContainsSubstring;

namespace {

constexpr std::string_view kSource = "Scenes/test.json";

[[nodiscard]] SceneDescription parseOrFail(std::string_view text) {
    auto scene = parseScene(text, kSource);
    if (!scene) {
        FAIL(scene.error().message);
    }
    return std::move(*scene);
}

// The scene is refused, and the error names the file and every given piece (entity, field, reason).
void requireError(std::string_view text, std::initializer_list<std::string_view> pieces) {
    const auto scene = parseScene(text, kSource);
    REQUIRE_FALSE(scene.has_value());
    CAPTURE(scene.error().message);
    REQUIRE_THAT(scene.error().message, ContainsSubstring(std::string(kSource)));
    for (const std::string_view piece : pieces) {
        REQUIRE_THAT(scene.error().message, ContainsSubstring(std::string(piece)));
    }
}

// One entity "crate" with the given extra fields.
[[nodiscard]] std::string withEntity(std::string_view fields) {
    return std::format(R"({{"version": 1, "entities": [{{"id": "crate"{}{}}}]}})", fields.empty() ? "" : ", ",
                       fields);
}

} // namespace

TEST_CASE("A full scene is read with every component", "[serialization]") {
    const SceneDescription scene = parseOrFail(R"({
        "version": 1,
        "ambient": [0.1, 0.2, 0.3],
        "entities": [
            {
                "id": "crate_01",
                "name": "Caisse",
                "transform": { "position": [2.0, 0.5, -3.0], "rotation_deg": [0, 45, 0], "scale": [1, 2, 1] },
                "mesh": "Models\\crate.gltf",
                "material": { "base_color": [1, 0, 0], "metallic": 1.0, "roughness": 0.25 },
                "collider": { "shape": "box", "half_extents": [0.5, 0.5, 0.5] },
                "rigid_body": { "type": "dynamic", "mass": 20.0 }
            },
            { "id": "sun", "light": { "direction": [-0.4, -1.0, -0.35], "color": [1, 0.95, 0.88], "intensity": 3 } },
            { "id": "camera", "transform": { "position": [0, 2.5, 7] }, "camera": { "fov_deg": 70, "near": 0.1, "far": 500 } }
        ]
    })");

    REQUIRE(scene.ambient == glm::vec3(0.1f, 0.2f, 0.3f));
    REQUIRE(scene.entities.size() == 3);

    const EntityDesc& crate = scene.entities[0];
    REQUIRE(crate.id == "crate_01");
    REQUIRE(crate.name == "Caisse");
    REQUIRE(crate.transform->position == glm::vec3(2.0f, 0.5f, -3.0f));
    REQUIRE(crate.transform->rotationDegrees == glm::vec3(0.0f, 45.0f, 0.0f));
    REQUIRE(crate.transform->scale == glm::vec3(1.0f, 2.0f, 1.0f));
    // Normalized like any asset path.
    REQUIRE(crate.mesh == "Models/crate.gltf");
    REQUIRE(crate.material->baseColor == glm::vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(crate.material->metallic == 1.0f);
    REQUIRE(crate.material->roughness == 0.25f);
    REQUIRE(crate.collider->shape == apex::world::ColliderShape::Box);
    REQUIRE(crate.collider->halfExtents == glm::vec3(0.5f));
    REQUIRE(crate.rigidBody->type == apex::world::BodyType::Dynamic);
    REQUIRE(crate.rigidBody->mass == 20.0f);

    REQUIRE(scene.entities[1].light->direction == glm::vec3(-0.4f, -1.0f, -0.35f));
    REQUIRE(scene.entities[1].light->intensity == 3.0f);
    REQUIRE_FALSE(scene.entities[1].transform.has_value());
    REQUIRE(scene.entities[2].camera->fovDegrees == 70.0f);
    REQUIRE(scene.entities[2].camera->farPlane == 500.0f);
}

TEST_CASE("Omitted fields take their defaults", "[serialization]") {
    const SceneDescription scene = parseOrFail(
        withEntity(R"("transform": {}, "mesh": "Models/cube.glb", "material": {}, "camera": {})"));
    const EntityDesc& entity = scene.entities[0];
    REQUIRE(*entity.transform == TransformDesc{});
    REQUIRE(*entity.material == MaterialDesc{});
    REQUIRE(*entity.camera == CameraDesc{});
    REQUIRE(scene.ambient == SceneDescription{}.ambient);
    REQUIRE_FALSE(entity.name.has_value());
    REQUIRE(parseOrFail(R"({"version": 1, "entities": []})").entities.empty());
}

TEST_CASE("Sphere and capsule colliders read their own dimensions", "[serialization]") {
    const auto sphere = parseOrFail(withEntity(R"("collider": {"shape": "sphere", "radius": 0.25})"));
    REQUIRE(sphere.entities[0].collider->shape == apex::world::ColliderShape::Sphere);
    REQUIRE(sphere.entities[0].collider->radius == 0.25f);
    const auto capsule =
        parseOrFail(withEntity(R"("collider": {"shape": "capsule", "radius": 0.3, "half_height": 0.6})"));
    REQUIRE(capsule.entities[0].collider->shape == apex::world::ColliderShape::Capsule);
    REQUIRE(capsule.entities[0].collider->halfHeight == 0.6f);
    const auto fixed = parseOrFail(withEntity(
        R"("collider": {"shape": "box", "half_extents": [1, 1, 1]}, "rigid_body": {"type": "static"})"));
    REQUIRE(fixed.entities[0].rigidBody->type == apex::world::BodyType::Static);
}

TEST_CASE("The version is required and must be the supported one", "[serialization]") {
    requireError(R"({"entities": []})", {"field 'version'", "missing"});
    requireError(R"({"version": 2, "entities": []})", {"field 'version'", "version 2 is not supported"});
    requireError(R"({"version": "1", "entities": []})", {"field 'version'", "integer"});
}

TEST_CASE("Malformed JSON is refused with the position of the error", "[serialization]") {
    requireError(R"({"version": 1, "entities": [)", {"invalid JSON"});
    requireError("", {"invalid JSON"});
    requireError("[1, 2]", {"expected an object"});
    requireError(R"({"version": 1})", {"field 'entities'"});
}

TEST_CASE("Entity ids are required, non-empty and unique", "[serialization]") {
    // No id yet: the entity is named by its place in the file.
    requireError(R"({"version": 1, "entities": [{"id": "a"}, {"name": "x"}]})",
                 {"entity '#1'", "field 'id'"});
    requireError(R"({"version": 1, "entities": [{"id": ""}]})", {"entity '#0'", "field 'id'", "empty"});
    requireError(R"({"version": 1, "entities": [{"id": 7}]})", {"field 'id'", "expected a string"});
    requireError(R"({"version": 1, "entities": [{"id": "a"}, {"id": "a"}]})", {"entity 'a'", "already used"});
}

TEST_CASE("Unknown fields are refused at every level", "[serialization]") {
    requireError(R"({"version": 1, "entities": [], "gravity": 9.81})", {"field 'gravity'", "unknown field"});
    requireError(withEntity(R"("postion": [0, 0, 0])"),
                 {"entity 'crate'", "field 'postion'", "unknown field"});
    requireError(withEntity(R"("transform": {"rotation": [0, 0, 0]})"),
                 {"entity 'crate'", "field 'transform.rotation'", "unknown field"});
    requireError(withEntity(R"("collider": {"shape": "sphere", "radius": 1, "half_extents": [1, 1, 1]})"),
                 {"field 'collider.half_extents'", "unknown field"});
}

TEST_CASE("A field of the wrong type or size names the entity and the field", "[serialization]") {
    requireError(withEntity(R"("transform": {"position": [1, 2]})"),
                 {"entity 'crate'", "field 'transform.position'", "array of 3 numbers"});
    requireError(withEntity(R"("transform": {"position": [1, "two", 3]})"), {"field 'transform.position'"});
    requireError(withEntity(R"("transform": 5)"), {"field 'transform'", "expected an object"});
    requireError(withEntity(R"("mesh": 12)"), {"field 'mesh'", "expected a string"});
    requireError(withEntity(R"("name": "")"), {"field 'name'", "empty"});
    requireError(R"({"version": 1, "ambient": [1, 2], "entities": []})", {"field 'ambient'"});
}

TEST_CASE("Values out of range are refused", "[serialization]") {
    requireError(withEntity(R"("transform": {"scale": [1, 0, 1]})"),
                 {"field 'transform.scale'", "greater than 0"});
    requireError(withEntity(R"("mesh": "../outside.glb")"), {"field 'mesh'", "../outside.glb"});
    requireError(withEntity(R"("mesh": "m.glb", "material": {"roughness": 1.5})"),
                 {"field 'material.roughness'", "between 0 and 1"});
    requireError(withEntity(R"("mesh": "m.glb", "material": {"base_color": [-1, 0, 0]})"),
                 {"field 'material.base_color'"});
    requireError(withEntity(R"("camera": {"fov_deg": 180})"), {"field 'camera.fov_deg'"});
    requireError(withEntity(R"("camera": {"near": 10, "far": 5})"),
                 {"field 'camera.far'", "greater than near"});
    requireError(withEntity(R"("light": {"direction": [0, 0, 0]})"),
                 {"field 'light.direction'", "zero vector"});
    requireError(withEntity(R"("light": {"direction": [0, -1, 0], "intensity": -1})"),
                 {"field 'light.intensity'"});
    requireError(withEntity(R"("transform": {"position": [1e60, 0, 0]})"), {"field 'transform.position'"});
}

TEST_CASE("Colliders and rigid bodies must be complete", "[serialization]") {
    requireError(withEntity(R"("collider": {"half_extents": [1, 1, 1]})"),
                 {"field 'collider.shape'", "missing"});
    requireError(withEntity(R"("collider": {"shape": "cone"})"), {"field 'collider.shape'", "cone"});
    requireError(withEntity(R"("collider": {"shape": "box"})"), {"field 'collider.half_extents'", "missing"});
    requireError(withEntity(R"("collider": {"shape": "sphere", "radius": 0})"),
                 {"field 'collider.radius'", "greater than 0"});
    requireError(withEntity(R"("collider": {"shape": "capsule", "radius": 0.3})"),
                 {"field 'collider.half_height'", "missing"});
    requireError(withEntity(R"("rigid_body": {"type": "static"})"),
                 {"field 'rigid_body'", "needs a collider"});
    const std::string_view box = R"("collider": {"shape": "box", "half_extents": [1, 1, 1]}, )";
    requireError(withEntity(std::string(box) + R"("rigid_body": {"type": "dynamic"})"),
                 {"field 'rigid_body.mass'", "missing"});
    requireError(withEntity(std::string(box) + R"("rigid_body": {"type": "dynamic", "mass": -20})"),
                 {"field 'rigid_body.mass'", "greater than 0"});
    requireError(withEntity(std::string(box) + R"("rigid_body": {"type": "kinematic"})"),
                 {"field 'rigid_body.type'", "kinematic"});
}

TEST_CASE("A material needs a mesh", "[serialization]") {
    requireError(withEntity(R"("material": {"metallic": 1})"),
                 {"entity 'crate'", "field 'material'", "needs a mesh"});
}
