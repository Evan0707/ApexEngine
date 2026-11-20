#include "Graphics/Frustum.h"

#include <algorithm>

namespace apex::graphics {

Frustum frustumFromMatrix(const glm::mat4& viewProjection) noexcept {
    // GLM is column-major: row i of the matrix is (m[0][i], m[1][i], m[2][i], m[3][i]).
    const auto row = [&viewProjection](int i) {
        return glm::vec4(viewProjection[0][i], viewProjection[1][i], viewProjection[2][i],
                         viewProjection[3][i]);
    };
    const glm::vec4 x = row(0);
    const glm::vec4 y = row(1);
    const glm::vec4 z = row(2);
    const glm::vec4 w = row(3);
    // -w <= x <= w, -w <= y <= w, and 0 <= z <= w for Vulkan's depth.
    Frustum frustum{{w + x, w - x, w + y, w - y, z, w - z}};
    for (glm::vec4& plane : frustum.planes) {
        plane /= glm::length(glm::vec3(plane));
    }
    return frustum;
}

resources::BoundingSphere transformSphere(const glm::mat4& model,
                                          const resources::BoundingSphere& sphere) noexcept {
    const float scale = std::max({glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])),
                                  glm::length(glm::vec3(model[2]))});
    return resources::BoundingSphere{.center = glm::vec3(model * glm::vec4(sphere.center, 1.0f)),
                                     .radius = sphere.radius * scale};
}

bool intersects(const Frustum& frustum, const resources::BoundingSphere& sphere) noexcept {
    return std::ranges::all_of(frustum.planes, [&sphere](const glm::vec4& plane) {
        return glm::dot(glm::vec3(plane), sphere.center) + plane.w >= -sphere.radius;
    });
}

} // namespace apex::graphics
