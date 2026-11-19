#include "Resources/GltfImporter.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <string>

using namespace apex::resources;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;

[[nodiscard]] std::filesystem::path testData(const char* relative) {
    return std::filesystem::path(APEX_TEST_DATA_DIR) / relative;
}

[[nodiscard]] std::filesystem::path asset(const char* relative) {
    return std::filesystem::path(APEX_ASSETS_DIR) / relative;
}

[[nodiscard]] ModelData importOrFail(const std::filesystem::path& file) {
    auto model = importGltf(file, file.filename().string());
    if (!model) {
        FAIL(model.error().message);
    }
    return std::move(*model);
}

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

// Each triangle's winding agrees with its vertex normals: counter-clockwise seen from the front.
void requireWindingMatchesNormals(const MeshData& mesh) {
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[i]];
        const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 face = glm::cross(b.position - a.position, c.position - a.position);
        REQUIRE(glm::dot(face, a.normal + b.normal + c.normal) > 0.0f);
    }
}

[[nodiscard]] std::pair<glm::vec3, glm::vec3> bounds(const MeshData& mesh) {
    glm::vec3 low(1e9f);
    glm::vec3 high(-1e9f);
    for (const Vertex& vertex : mesh.vertices) {
        low = glm::min(low, vertex.position);
        high = glm::max(high, vertex.position);
    }
    return {low, high};
}

} // namespace

TEST_CASE("The reference cube imports as one 1 m part with outward faces", "[resources]") {
    const ModelData model = importOrFail(asset("Models/reference_cube.glb"));
    REQUIRE(model.parts.size() == 1);
    const MeshData& mesh = model.parts[0].mesh;
    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(triangleCount(mesh) == 12);
    const auto [low, high] = bounds(mesh);
    requireNear(low, glm::vec3(-0.5f));
    requireNear(high, glm::vec3(0.5f));
    requireWindingMatchesNormals(mesh);
}

TEST_CASE("Node transforms are applied to the vertices", "[resources]") {
    // Parent: translate (10, 0, 0). Child: rotate 90 degrees around +Y, scale 2. Unsigned byte indices.
    const ModelData model = importOrFail(testData("Gltf/node_transforms.gltf"));
    REQUIRE(model.parts.size() == 1);
    const MeshData& mesh = model.parts[0].mesh;
    REQUIRE(mesh.indices == std::vector<std::uint32_t>{0, 1, 2});
    requireNear(mesh.vertices[0].position, {10.0f, 0.0f, 0.0f});
    requireNear(mesh.vertices[1].position, {10.0f, 0.0f, -2.0f});
    requireNear(mesh.vertices[2].position, {10.0f, 2.0f, 0.0f});
    requireNear(mesh.vertices[0].normal, {1.0f, 0.0f, 0.0f});
    requireWindingMatchesNormals(mesh);
}

TEST_CASE("A mirroring node keeps counter-clockwise front faces", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/mirrored.gltf"));
    const MeshData& mesh = model.parts[0].mesh;
    requireNear(mesh.vertices[1].position, {-1.0f, 0.0f, 0.0f});
    requireNear(mesh.vertices[0].normal, {0.0f, 0.0f, 1.0f});
    requireWindingMatchesNormals(mesh);
}

TEST_CASE("Missing normals and tangents are computed", "[resources]") {
    // Unsigned int indices, no NORMAL, no TEXCOORD_0.
    const ModelData model = importOrFail(testData("Gltf/without_normals.gltf"));
    const MeshData& mesh = model.parts[0].mesh;
    REQUIRE(validate(mesh, "without_normals"));
    for (const Vertex& vertex : mesh.vertices) {
        requireNear(vertex.normal, {0.0f, 0.0f, 1.0f});
        REQUIRE_THAT(glm::dot(vertex.normal, glm::vec3(vertex.tangent)), WithinAbs(0.0, kTolerance));
    }
}

TEST_CASE("Computed tangents follow u, with the top of the texture along cross(normal, tangent)",
          "[resources]") {
    // Triangle in the XY plane, u along +X, v = 0 at the top (+Y). Its node turns it a quarter around
    // +Y: u now runs along -Z.
    const ModelData model = importOrFail(testData("Gltf/node_transforms.gltf"));
    const Vertex& vertex = model.parts[0].mesh.vertices[0];
    requireNear(glm::vec3(vertex.tangent), {0.0f, 0.0f, -1.0f});
    const glm::vec3 up = glm::cross(vertex.normal, glm::vec3(vertex.tangent)) * vertex.tangent.w;
    requireNear(up, {0.0f, 1.0f, 0.0f});
}

TEST_CASE("Primitives without indices are drawn in vertex order", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/non_indexed.gltf"));
    REQUIRE(model.parts[0].mesh.indices == std::vector<std::uint32_t>{0, 1, 2});
}

TEST_CASE("Primitives that share a material are merged into one part", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/two_materials.gltf"));
    REQUIRE(model.parts.size() == 2);
    REQUIRE(model.parts[0].material != model.parts[1].material);
    // The mesh is used by two nodes: each part holds both copies, the second 5 m higher.
    const MeshData& first = model.parts[0].mesh;
    REQUIRE(first.vertices.size() == 6);
    REQUIRE(first.indices == std::vector<std::uint32_t>{0, 1, 2, 3, 4, 5});
    requireNear(first.vertices[5].position, {0.0f, 6.0f, 0.0f});
}

TEST_CASE("The crate's node lifts it onto the floor", "[resources]") {
    const ModelData model = importOrFail(asset("Models/crate.gltf"));
    const auto [low, high] = bounds(model.parts[0].mesh);
    requireNear(low, {-0.5f, 0.0f, -0.5f});
    requireNear(high, {0.5f, 1.0f, 0.5f});
}

TEST_CASE("A model exported by another tool imports at its real size", "[resources]") {
    // Khronos sample "Water Bottle": 26 cm tall, turned half a turn by its node.
    const ModelData model = importOrFail(asset("Models/water_bottle.glb"));
    REQUIRE(model.parts.size() == 1);
    const auto [low, high] = bounds(model.parts[0].mesh);
    REQUIRE_THAT(high.y - low.y, WithinAbs(0.26, 0.001));
    REQUIRE(validate(model.parts[0].mesh, "water_bottle"));
}

TEST_CASE("Broken glTF files are refused with their name", "[resources]") {
    SECTION("Truncated GLB") {
        const auto model = importGltf(testData("Gltf/truncated.glb"), "Models/truncated.glb");
        REQUIRE_FALSE(model.has_value());
        REQUIRE_THAT(model.error().message, ContainsSubstring("Models/truncated.glb"));
    }
    SECTION("Unsupported required extension") {
        const auto model = importGltf(testData("Gltf/required_extension.gltf"), "Models/draco.gltf");
        REQUIRE_FALSE(model.has_value());
        REQUIRE_THAT(model.error().message, ContainsSubstring("KHR_draco_mesh_compression"));
    }
    SECTION("Missing file") {
        const auto model = importGltf(testData("Gltf/absent.glb"), "Models/absent.glb");
        REQUIRE_FALSE(model.has_value());
        REQUIRE(model.error().code == apex::core::ErrorCode::NotFound);
        REQUIRE_THAT(model.error().message, ContainsSubstring("Models/absent.glb"));
    }
}
