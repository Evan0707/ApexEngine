#include "Resources/MaterialData.h"
#include "Resources/MeshData.h"
#include "Resources/Primitives.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <limits>

using namespace apex::resources;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("A mesh without triangles is rejected", "[resources]") {
    MeshData mesh = makeCube(1.0f);
    mesh.indices.clear();
    const auto result = validate(mesh, "empty");
    REQUIRE_FALSE(result);
    REQUIRE_THAT(result.error().message, ContainsSubstring("'empty'"));

    mesh.indices = {0, 1};
    REQUIRE_FALSE(validate(mesh, "two indices"));
}

TEST_CASE("A mesh with an index out of range is rejected, with the index named", "[resources]") {
    MeshData mesh = makeCube(1.0f);
    mesh.indices[4] = 24;
    const auto result = validate(mesh, "crate");
    REQUIRE_FALSE(result);
    REQUIRE_THAT(result.error().message, ContainsSubstring("crate"));
    REQUIRE_THAT(result.error().message, ContainsSubstring("index 4 is 24"));
}

TEST_CASE("A mesh with an invalid vertex is rejected", "[resources]") {
    SECTION("Non-finite position") {
        MeshData mesh = makeCube(1.0f);
        mesh.vertices[3].position.x = std::numeric_limits<float>::quiet_NaN();
        REQUIRE_FALSE(validate(mesh, "nan"));
    }
    SECTION("Normal not unit length") {
        MeshData mesh = makeCube(1.0f);
        mesh.vertices[3].normal *= 2.0f;
        REQUIRE_FALSE(validate(mesh, "normal"));
    }
    SECTION("Tangent sign not +1 or -1") {
        MeshData mesh = makeCube(1.0f);
        mesh.vertices[3].tangent.w = 0.0f;
        REQUIRE_FALSE(validate(mesh, "tangent"));
    }
}

TEST_CASE("The default material is a light grey dielectric", "[resources]") {
    const MaterialData material;
    REQUIRE(material.baseColorFactor == glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    REQUIRE(material.metallicFactor == 0.0f);
    REQUIRE(material.roughnessFactor == 0.5f);
}
