#include "Graphics/FrameData.h"

#include "Graphics/Projection.h"

namespace apex::graphics {

glm::mat4 shadowViewProjection(const RenderShadow& shadow) noexcept {
    return orthographicProjection(shadow.halfExtent, shadow.halfExtent, 0.0f, shadow.depthRange) *
           shadow.lightView;
}

FrameUniforms makeFrameUniforms(const RenderScene& scene, float aspectRatio,
                                std::uint32_t shadowMapSize) noexcept {
    const RenderCamera& camera = scene.camera;
    const glm::mat4 projection =
        perspectiveProjection(camera.verticalFovRadians, aspectRatio, camera.nearPlane, camera.farPlane);
    const bool shadows = scene.shadow.enabled && shadowMapSize > 0;
    return FrameUniforms{
        .viewProjection = projection * camera.view,
        .lightViewProjection = shadows ? shadowViewProjection(scene.shadow) : glm::mat4(1.0f),
        .directionToLight = glm::vec4(glm::normalize(scene.light.directionToLight), 0.0f),
        .lightRadiance = glm::vec4(scene.light.color * scene.light.intensity, 0.0f),
        .ambient = glm::vec4(scene.ambient, 0.0f),
        .shadow = glm::vec4(shadows ? 1.0f : 0.0f,
                            shadowMapSize > 0 ? 1.0f / static_cast<float>(shadowMapSize) : 0.0f,
                            kShadowFadeStart, 0.0f),
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
