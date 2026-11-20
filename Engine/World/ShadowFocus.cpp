#include "World/ShadowFocus.h"

#include "Core/Assert.h"
#include "World/RenderExtraction.h"

#include <cmath>

namespace apex::world {

namespace {

// Above this, the light is too close to vertical for +Y to give a stable "up".
constexpr float kVerticalLight = 0.99f;

} // namespace

glm::mat3 lightViewRotation(const glm::vec3& directionToLight) noexcept {
    const glm::vec3 back = glm::normalize(directionToLight);
    const glm::vec3 worldUp =
        std::abs(back.y) > kVerticalLight ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(worldUp, back));
    const glm::vec3 up = glm::cross(back, right);
    // Columns right, up, back: its transpose takes world vectors to the light's view.
    return glm::transpose(glm::mat3(right, up, back));
}

graphics::RenderShadow frameShadow(const glm::vec3& directionToLight, const core::WorldPosition& focus,
                                   const core::WorldPosition& cameraPosition,
                                   const ShadowSettings& settings) noexcept {
    APEX_ASSERT(settings.mapSize > 0 && settings.areaSize > 0.0f && settings.depthRange > 0.0f,
                "Invalid shadow settings");
    const glm::mat3 rotation = lightViewRotation(directionToLight);
    const float texel = settings.areaSize / static_cast<float>(settings.mapSize);
    glm::vec3 inLight = rotation * focus;
    inLight.x = std::round(inLight.x / texel) * texel;
    inLight.y = std::round(inLight.y / texel) * texel;
    const core::WorldPosition center = glm::transpose(rotation) * inLight;

    // The eye sits on the area's near face, half the depth towards the light. The subtraction of the camera
    // comes first: large world coordinates cancel out here (G-07).
    const glm::vec3 eye =
        (center - cameraPosition) + glm::normalize(directionToLight) * (settings.depthRange * 0.5f);
    glm::mat4 lightView(rotation);
    lightView[3] = glm::vec4(-(rotation * eye), 1.0f);
    return graphics::RenderShadow{.enabled = true,
                                  .lightView = lightView,
                                  .halfExtent = settings.areaSize * 0.5f,
                                  .depthRange = settings.depthRange,
                                  .constantBias = settings.constantBias,
                                  .slopeBias = settings.slopeBias};
}

void extractShadow(const entt::registry& registry, entt::entity camera, entt::entity focus, float alpha,
                   const ShadowSettings& settings, graphics::RenderScene& scene) {
    scene.shadow = graphics::RenderShadow{};
    if (!settings.enabled || scene.light.intensity <= 0.0f) {
        return;
    }
    core::WorldPosition cameraPosition{0.0f};
    if (registry.valid(camera) && registry.all_of<Transform, Camera>(camera)) {
        cameraPosition = drawnTransform(registry, camera, alpha).position;
    }
    core::WorldPosition focusPosition = cameraPosition;
    if (registry.valid(focus) && registry.all_of<Transform>(focus)) {
        focusPosition = drawnTransform(registry, focus, alpha).position;
    }
    scene.shadow = frameShadow(scene.light.directionToLight, focusPosition, cameraPosition, settings);
}

} // namespace apex::world
