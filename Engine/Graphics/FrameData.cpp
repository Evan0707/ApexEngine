#include "Graphics/FrameData.h"

#include "Graphics/Projection.h"

#include <cmath>

namespace apex::graphics {

float exposureMultiplier(float stops) noexcept {
    return std::exp2(stops);
}

glm::mat4 shadowViewProjection(const RenderShadow& shadow) noexcept {
    return orthographicProjection(shadow.halfExtent, shadow.halfExtent, 0.0f, shadow.depthRange) *
           shadow.lightView;
}

FrameUniforms makeFrameUniforms(const RenderScene& scene, float aspectRatio, std::uint32_t shadowMapSize,
                                const FrameEnvironment& environment) noexcept {
    const RenderCamera& camera = scene.camera;
    const glm::mat4 projection =
        perspectiveProjection(camera.verticalFovRadians, aspectRatio, camera.nearPlane, camera.farPlane);
    const bool shadows = scene.shadow.enabled && shadowMapSize > 0;
    const glm::mat4 viewProjection = projection * camera.view;
    FrameUniforms uniforms{
        .viewProjection = viewProjection,
        .inverseViewProjection = glm::inverse(viewProjection),
        .lightViewProjection = shadows ? shadowViewProjection(scene.shadow) : glm::mat4(1.0f),
        .directionToLight = glm::vec4(glm::normalize(scene.light.directionToLight), 0.0f),
        .lightRadiance = glm::vec4(scene.light.color * scene.light.intensity, 0.0f),
        .ambient = glm::vec4(scene.ambient, 0.0f),
        .shadow = glm::vec4(shadows ? 1.0f : 0.0f,
                            shadowMapSize > 0 ? 1.0f / static_cast<float>(shadowMapSize) : 0.0f,
                            kShadowFadeStart, 0.0f),
        .toneMapping = glm::vec4(exposureMultiplier(scene.toneMapping.exposureStops),
                                 scene.toneMapping.enabled ? 1.0f : 0.0f, 0.0f, 0.0f),
        .irradiance = {},
        .environment = glm::vec4(0.0f),
    };
    if (environment.irradiance != nullptr) {
        for (std::size_t i = 0; i < environment.irradiance->coefficients.size(); ++i) {
            uniforms.irradiance[i] = glm::vec4(environment.irradiance->coefficients[i], 0.0f);
        }
        uniforms.environment = glm::vec4(1.0f, environment.intensity, environment.lastLevel, 0.0f);
    }
    return uniforms;
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
