#pragma once

#include "Core/Math.h"
#include "Resources/MeshData.h"

#include <array>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. The volume a view-projection matrix sees (decision 0021): six planes,
// normals pointing inside, unit length, for Vulkan's clip space (depth from 0 to 1).
struct Frustum {
    // xyz: unit normal, w: distance; a point p is inside a plane when dot(xyz, p) + w >= 0.
    std::array<glm::vec4, 6> planes{};
};

// The planes of a perspective or orthographic view-projection matrix (Gribb and Hartmann).
[[nodiscard]] Frustum frustumFromMatrix(const glm::mat4& viewProjection) noexcept;

// The sphere after the model matrix: its center moved, its radius scaled by the largest axis scale, so that
// it still contains the mesh under a non-uniform scale.
[[nodiscard]] resources::BoundingSphere transformSphere(const glm::mat4& model,
                                                        const resources::BoundingSphere& sphere) noexcept;

// False only when the sphere is entirely outside one plane. A sphere near a corner of the frustum may be kept
// though outside: a few objects drawn for nothing, never one missing.
[[nodiscard]] bool intersects(const Frustum& frustum, const resources::BoundingSphere& sphere) noexcept;

} // namespace apex::graphics
