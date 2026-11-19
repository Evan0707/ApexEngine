#include "Graphics/Projection.h"

#include "Core/Assert.h"

namespace apex::graphics {

glm::mat4 perspectiveProjection(float verticalFovRadians, float aspectRatio, float nearPlane,
                                float farPlane) noexcept {
    APEX_ASSERT(verticalFovRadians > 0.0f && aspectRatio > 0.0f, "Invalid field of view or aspect ratio");
    APEX_ASSERT(nearPlane > 0.0f && farPlane > nearPlane, "Invalid clip planes");
    glm::mat4 projection = glm::perspectiveRH_ZO(verticalFovRadians, aspectRatio, nearPlane, farPlane);
    // Vulkan's framebuffer Y points down.
    projection[1][1] = -projection[1][1];
    return projection;
}

} // namespace apex::graphics
