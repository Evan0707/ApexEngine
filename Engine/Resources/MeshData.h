#pragma once

#include "Core/Error.h"
#include "Core/Math.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace apex::resources {

// The single vertex format of the engine (spec 5.4).
struct Vertex {
    glm::vec3 position;
    // Unit length.
    glm::vec3 normal;
    // xyz: unit length, along increasing u. w: +1 or -1, as in glTF: cross(normal, tangent.xyz) * w points
    // towards the top of the texture.
    glm::vec4 tangent;
    // glTF convention: (0, 0) is the top-left corner of the texture.
    glm::vec2 uv;
};

// Indexed triangle list, counter-clockwise front faces (spec 3.3). Independent of any file format (G-04).
struct MeshData {
    // For error messages and GPU debugging: the asset path and part, or the name given in code.
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

[[nodiscard]] std::size_t triangleCount(const MeshData& mesh) noexcept;

// A sphere that contains every vertex: frustum culling (spec 5.4, decision 0021).
struct BoundingSphere {
    glm::vec3 center{0.0f};
    float radius = 0.0f;

    [[nodiscard]] bool operator==(const BoundingSphere&) const noexcept = default;
};

// Centered on the box around the vertices: not the smallest sphere, but close for the shapes of the demo, and
// cheap. An empty mesh gives a sphere of radius 0 at the origin.
[[nodiscard]] BoundingSphere boundingSphere(const MeshData& mesh) noexcept;

// Checks what Graphics relies on: at least one triangle, indices in range, finite positions, unit
// normals and tangents. The error names the mesh.
[[nodiscard]] core::Result<void> validate(const MeshData& mesh, std::string_view name);

} // namespace apex::resources
