#include "Resources/MeshData.h"

#include <cmath>
#include <format>

namespace apex::resources {

namespace {

// Loose enough for data written by exporters in single precision.
constexpr float kUnitLengthTolerance = 1e-3f;

[[nodiscard]] bool isFinite(const glm::vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool isUnit(const glm::vec3& value) noexcept {
    return isFinite(value) && std::abs(glm::length(value) - 1.0f) <= kUnitLengthTolerance;
}

[[nodiscard]] std::unexpected<core::Error> invalid(std::string_view name, std::string_view reason) {
    return core::makeError(core::ErrorCode::InvalidArgument, std::format("Mesh '{}': {}", name, reason));
}

} // namespace

std::size_t triangleCount(const MeshData& mesh) noexcept {
    return mesh.indices.size() / 3;
}

BoundingSphere boundingSphere(const MeshData& mesh) noexcept {
    if (mesh.vertices.empty()) {
        return {};
    }
    glm::vec3 low = mesh.vertices.front().position;
    glm::vec3 high = low;
    for (const Vertex& vertex : mesh.vertices) {
        low = glm::min(low, vertex.position);
        high = glm::max(high, vertex.position);
    }
    BoundingSphere sphere{.center = (low + high) * 0.5f, .radius = 0.0f};
    for (const Vertex& vertex : mesh.vertices) {
        sphere.radius = std::max(sphere.radius, glm::length(vertex.position - sphere.center));
    }
    return sphere;
}

core::Result<void> validate(const MeshData& mesh, std::string_view name) {
    if (mesh.indices.empty() || mesh.indices.size() % 3 != 0) {
        return invalid(name,
                       std::format("{} indices, expected a non-zero multiple of 3", mesh.indices.size()));
    }
    for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
        if (mesh.indices[i] >= mesh.vertices.size()) {
            return invalid(name, std::format("index {} is {}, but there are {} vertices", i, mesh.indices[i],
                                             mesh.vertices.size()));
        }
    }
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Vertex& vertex = mesh.vertices[i];
        if (!isFinite(vertex.position)) {
            return invalid(name, std::format("vertex {} has a non-finite position", i));
        }
        if (!isUnit(vertex.normal)) {
            return invalid(name, std::format("vertex {} has a normal that is not unit length", i));
        }
        if (!isUnit(glm::vec3(vertex.tangent)) || std::abs(vertex.tangent.w) != 1.0f) {
            return invalid(name, std::format("vertex {} has an invalid tangent", i));
        }
    }
    return {};
}

} // namespace apex::resources
