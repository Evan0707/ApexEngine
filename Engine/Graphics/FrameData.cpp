#include "Graphics/FrameData.h"

#include "Graphics/Projection.h"

namespace apex::graphics {

FrameUniforms makeFrameUniforms(const RenderScene& scene, float aspectRatio) noexcept {
    const RenderCamera& camera = scene.camera;
    const glm::mat4 projection =
        perspectiveProjection(camera.verticalFovRadians, aspectRatio, camera.nearPlane, camera.farPlane);
    return FrameUniforms{
        .viewProjection = projection * camera.view,
        .directionToLight = glm::vec4(glm::normalize(scene.light.directionToLight), 0.0f),
        .lightRadiance = glm::vec4(scene.light.color * scene.light.intensity, 0.0f),
        .ambient = glm::vec4(scene.ambient, 0.0f),
    };
}

ObjectGpu makeObjectGpu(const glm::mat4& model, std::uint32_t materialIndex) noexcept {
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    return ObjectGpu{
        .model = model,
        .normalMatrix = glm::mat4(normalMatrix),
        .materialIndex = materialIndex,
        .padding = {},
    };
}

} // namespace apex::graphics
