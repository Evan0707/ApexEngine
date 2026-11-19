#include "SceneExtraction.h"

namespace apex::game {

graphics::RenderCamera makeRenderCamera(const CameraView& camera) noexcept {
    return graphics::RenderCamera{
        .view = glm::mat4_cast(glm::inverse(camera.orientation)),
        .verticalFovRadians = camera.verticalFovRadians,
        .nearPlane = camera.nearPlane,
        .farPlane = camera.farPlane,
    };
}

glm::mat4 modelRelativeToCamera(const ObjectTransform& transform,
                                const core::WorldPosition& cameraPosition) noexcept {
    // The subtraction happens before any matrix: large world coordinates cancel out exactly here.
    const glm::vec3 relative = transform.position - cameraPosition;
    return glm::scale(glm::translate(glm::mat4(1.0f), relative) * glm::mat4_cast(transform.rotation),
                      transform.scale);
}

} // namespace apex::game
