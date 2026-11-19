#include "Resources/MeshData.h"
#include "Resources/Primitives.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>

using namespace apex::resources;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;

// For a convex mesh centered on the origin, a counter-clockwise front face has its geometric normal
// pointing away from the center, and the vertex normals agree with it.
void requireOutwardCounterClockwise(const MeshData& mesh) {
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[i]];
        const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 faceNormal = glm::cross(b.position - a.position, c.position - a.position);
        REQUIRE(glm::length(faceNormal) > 0.0f);
        const glm::vec3 centroid = (a.position + b.position + c.position) / 3.0f;
        REQUIRE(glm::dot(faceNormal, centroid) > 0.0f);
        REQUIRE(glm::dot(faceNormal, a.normal + b.normal + c.normal) > 0.0f);
    }
}

void requireOrthonormalTangents(const MeshData& mesh) {
    for (const Vertex& vertex : mesh.vertices) {
        REQUIRE_THAT(glm::dot(vertex.normal, glm::vec3(vertex.tangent)), WithinAbs(0.0, kTolerance));
        REQUIRE(std::abs(vertex.tangent.w) == 1.0f);
    }
}

} // namespace

TEST_CASE("The cube is centered, 1 m wide and valid", "[resources]") {
    const MeshData cube = makeCube(1.0f);
    REQUIRE(validate(cube, "cube"));
    REQUIRE(cube.vertices.size() == 24);
    REQUIRE(triangleCount(cube) == 12);

    glm::vec3 low(1e9f);
    glm::vec3 high(-1e9f);
    for (const Vertex& vertex : cube.vertices) {
        low = glm::min(low, vertex.position);
        high = glm::max(high, vertex.position);
    }
    REQUIRE(low == glm::vec3(-0.5f));
    REQUIRE(high == glm::vec3(0.5f));
}

TEST_CASE("The cube faces are counter-clockwise seen from outside", "[resources]") {
    const MeshData cube = makeCube(2.0f);
    requireOutwardCounterClockwise(cube);
    requireOrthonormalTangents(cube);
}

TEST_CASE("Each cube face maps the whole texture, top of the texture up", "[resources]") {
    const MeshData cube = makeCube(1.0f);
    // Face +Z, seen from the front: its top edge is at +Y and has v = 0 (glTF).
    for (const Vertex& vertex : cube.vertices) {
        REQUIRE(vertex.uv.x >= 0.0f);
        REQUIRE(vertex.uv.x <= 1.0f);
        if (vertex.normal == glm::vec3(0.0f, 0.0f, 1.0f)) {
            REQUIRE((vertex.uv.y == 0.0f) == (vertex.position.y > 0.0f));
            REQUIRE((vertex.uv.x == 1.0f) == (vertex.position.x > 0.0f));
            const glm::vec3 up = glm::cross(vertex.normal, glm::vec3(vertex.tangent)) * vertex.tangent.w;
            REQUIRE(up == glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }
}

TEST_CASE("The sphere has its vertices on its radius and is valid", "[resources]") {
    const MeshData sphere = makeSphere(0.5f, 16, 8);
    REQUIRE(validate(sphere, "sphere"));
    REQUIRE(sphere.vertices.size() == 17 * 9);
    // Two pole rows of 16 triangles, and 6 rows of 32.
    REQUIRE(triangleCount(sphere) == 2 * 16 + 6 * 32);
    for (const Vertex& vertex : sphere.vertices) {
        REQUIRE_THAT(glm::length(vertex.position), WithinAbs(0.5, kTolerance));
    }
}

TEST_CASE("The sphere faces are counter-clockwise seen from outside", "[resources]") {
    const MeshData sphere = makeSphere(1.0f, 12, 6);
    requireOutwardCounterClockwise(sphere);
    requireOrthonormalTangents(sphere);
}

TEST_CASE("The sphere starts at the north pole and goes around from +Z towards +X", "[resources]") {
    const MeshData sphere = makeSphere(1.0f, 4, 2);
    REQUIRE_THAT(sphere.vertices.front().position.y, WithinAbs(1.0, kTolerance));
    // Ring 1 is the equator: segment 0 at +Z, segment 1 a quarter turn later at +X.
    const Vertex& front = sphere.vertices[5];
    const Vertex& right = sphere.vertices[6];
    REQUIRE_THAT(front.position.z, WithinAbs(1.0, kTolerance));
    REQUIRE_THAT(right.position.x, WithinAbs(1.0, kTolerance));
    REQUIRE_THAT(right.uv.x, WithinAbs(0.25, kTolerance));
}
