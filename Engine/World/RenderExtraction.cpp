#include "World/RenderExtraction.h"

#include "Core/Profiling.h"
#include "World/SceneQueries.h"

namespace apex::world {

namespace {

[[nodiscard]] Transform renderTransform(const entt::registry& registry, entt::entity entity,
                                        const Transform& current, float alpha) {
    const PreviousTransform* previous = registry.try_get<PreviousTransform>(entity);
    return previous != nullptr ? interpolateTransform(previous->value, current, alpha) : current;
}

[[nodiscard]] glm::mat4 modelRelativeToCamera(const Transform& transform,
                                              const core::WorldPosition& cameraPosition) noexcept {
    // The subtraction comes before any matrix: large world coordinates cancel out exactly here.
    const glm::vec3 relative = transform.position - cameraPosition;
    return glm::scale(glm::translate(glm::mat4(1.0f), relative) * glm::mat4_cast(transform.rotation),
                      transform.scale);
}

} // namespace

void rememberTransforms(entt::registry& registry) {
    for (const auto [entity, transform] : registry.view<Transform>().each()) {
        registry.emplace_or_replace<PreviousTransform>(entity, transform);
    }
}

Transform drawnTransform(const entt::registry& registry, entt::entity entity, float alpha) {
    return renderTransform(registry, entity, registry.get<Transform>(entity), alpha);
}

Transform interpolateTransform(const Transform& previous, const Transform& current, float alpha) noexcept {
    return Transform{
        .position = glm::mix(previous.position, current.position, alpha),
        .rotation = glm::slerp(previous.rotation, current.rotation, alpha),
        .scale = glm::mix(previous.scale, current.scale, alpha),
    };
}

void extractRenderScene(const entt::registry& registry, const resources::ResourceCache& cache,
                        entt::entity camera, float alpha, graphics::RenderScene& scene) {
    APEX_PROFILE_SCOPE("World::extractRenderScene");

    Transform cameraTransform;
    Camera cameraSettings;
    if (registry.valid(camera) && registry.all_of<Transform, Camera>(camera)) {
        cameraTransform = drawnTransform(registry, camera, alpha);
        cameraSettings = registry.get<Camera>(camera);
    }
    scene.camera = graphics::RenderCamera{
        // Rotation only: render space is centered on the camera.
        .view = glm::mat4_cast(glm::inverse(cameraTransform.rotation)),
        .verticalFovRadians = cameraSettings.verticalFovRadians,
        .nearPlane = cameraSettings.nearPlane,
        .farPlane = cameraSettings.farPlane,
    };

    scene.light = graphics::RenderLight{.intensity = 0.0f};
    for (const entt::entity entity : registry.view<DirectionalLight>()) {
        const DirectionalLight& light = registry.get<DirectionalLight>(entity);
        scene.light = graphics::RenderLight{
            .directionToLight = -glm::normalize(light.direction),
            .color = light.color,
            .intensity = light.intensity,
        };
        break;
    }
    scene.ambient = sceneEnvironment(registry).ambient;

    scene.objects.clear();
    for (const auto [entity, transform, renderer] : registry.view<Transform, MeshRenderer>().each()) {
        const glm::mat4 model = modelRelativeToCamera(renderTransform(registry, entity, transform, alpha),
                                                      cameraTransform.position);
        // A model that is not ready gives the fallback model's parts, and keeps their checker (G-03).
        const bool overridden = renderer.materialOverride != resources::MaterialHandle{} &&
                                cache.state(renderer.model) == resources::ResourceState::Ready;
        for (const resources::ModelPartRef& part : cache.parts(renderer.model)) {
            scene.objects.push_back(graphics::RenderObject{
                .mesh = part.mesh,
                .material = overridden ? renderer.materialOverride : part.material,
                .modelRelativeToCamera = model,
            });
        }
    }
}

} // namespace apex::world
