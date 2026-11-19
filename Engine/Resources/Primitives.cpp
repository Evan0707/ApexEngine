#include "Resources/Primitives.h"

#include "Core/Assert.h"

#include <array>
#include <cmath>

namespace apex::resources {

namespace {

struct CubeFace {
    glm::vec3 normal;
    // Along increasing u. The face's "up" is cross(normal, tangent), so cross(tangent, up) == normal and
    // the corners below are counter-clockwise seen from outside.
    glm::vec3 tangent;
};

constexpr std::array<CubeFace, 6> kCubeFaces{{
    {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    {{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}},
}};

} // namespace

MeshData makeCube(float size) {
    APEX_ASSERT(size > 0.0f, "A cube needs a positive size");
    const float half = size * 0.5f;
    MeshData mesh;
    mesh.vertices.reserve(kCubeFaces.size() * 4);
    mesh.indices.reserve(kCubeFaces.size() * 6);
    for (const CubeFace& face : kCubeFaces) {
        const glm::vec3 up = glm::cross(face.normal, face.tangent);
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        // Bottom-left, bottom-right, top-right, top-left of the face; v = 0 at the top (glTF).
        constexpr std::array<glm::vec2, 4> kCorners{
            {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
        for (const glm::vec2& corner : kCorners) {
            mesh.vertices.push_back(Vertex{
                .position = (face.normal + corner.x * face.tangent + corner.y * up) * half,
                .normal = face.normal,
                .tangent = glm::vec4(face.tangent, 1.0f),
                .uv = {(corner.x + 1.0f) * 0.5f, (1.0f - corner.y) * 0.5f},
            });
        }
        for (const std::uint32_t index : {0u, 1u, 2u, 0u, 2u, 3u}) {
            mesh.indices.push_back(base + index);
        }
    }
    return mesh;
}

MeshData makeSphere(float radius, std::uint32_t segments, std::uint32_t rings) {
    APEX_ASSERT(radius > 0.0f, "A sphere needs a positive radius");
    APEX_ASSERT(segments >= 3 && rings >= 2, "A sphere needs at least 3 segments and 2 rings");
    MeshData mesh;
    // The seam and the poles repeat their vertices: each copy has its own uv.
    mesh.vertices.reserve(static_cast<std::size_t>(rings + 1) * (segments + 1));
    for (std::uint32_t ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float polar = v * glm::pi<float>();
        for (std::uint32_t segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float azimuth = u * glm::two_pi<float>();
            const glm::vec3 normal{std::sin(polar) * std::sin(azimuth), std::cos(polar),
                                   std::sin(polar) * std::cos(azimuth)};
            const glm::vec3 tangent{std::cos(azimuth), 0.0f, -std::sin(azimuth)};
            mesh.vertices.push_back(Vertex{.position = normal * radius,
                                           .normal = glm::normalize(normal),
                                           .tangent = glm::vec4(tangent, 1.0f),
                                           .uv = {u, v}});
        }
    }

    const std::uint32_t stride = segments + 1;
    for (std::uint32_t ring = 0; ring < rings; ++ring) {
        for (std::uint32_t segment = 0; segment < segments; ++segment) {
            const std::uint32_t topLeft = ring * stride + segment;
            const std::uint32_t bottomLeft = topLeft + stride;
            // Counter-clockwise seen from outside. The triangle that would collapse on a pole is skipped.
            if (ring + 1 < rings) {
                mesh.indices.insert(mesh.indices.end(), {topLeft, bottomLeft, bottomLeft + 1});
            }
            if (ring > 0) {
                mesh.indices.insert(mesh.indices.end(), {topLeft, bottomLeft + 1, topLeft + 1});
            }
        }
    }
    return mesh;
}

} // namespace apex::resources
