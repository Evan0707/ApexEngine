#include "Serialization/SceneLoader.h"

#include "Resources/FileReader.h"
#include "Serialization/SceneJson.h"
#include "World/Rotation.h"
#include "World/SceneQueries.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace apex::serialization {

void instantiateScene(const SceneDescription& scene, entt::registry& registry,
                      resources::ResourceCache& cache) {
    registry.ctx().insert_or_assign(world::SceneEnvironment{.ambient = scene.ambient});

    for (const EntityDesc& description : scene.entities) {
        const entt::entity entity = registry.create();
        registry.emplace<world::SceneId>(entity, description.id);
        if (description.name) {
            registry.emplace<world::Name>(entity, *description.name);
        }
        // A drawn entity always has a place, even when the file gives none.
        if (description.transform || description.mesh) {
            const TransformDesc transform = description.transform.value_or(TransformDesc{});
            registry.emplace<world::Transform>(
                entity,
                world::Transform{.position = transform.position,
                                 .rotation = world::rotationFromEulerDegrees(transform.rotationDegrees),
                                 .scale = transform.scale});
        }
        if (description.mesh) {
            world::MeshRenderer renderer{.model = cache.loadModel(*description.mesh)};
            if (description.material) {
                renderer.materialOverride =
                    cache.addMaterial(std::format("Scene entity '{}'", description.id),
                                      resources::MaterialData{
                                          .baseColorFactor = glm::vec4(description.material->baseColor, 1.0f),
                                          .metallicFactor = description.material->metallic,
                                          .roughnessFactor = description.material->roughness});
            }
            registry.emplace<world::MeshRenderer>(entity, renderer);
        }
        if (description.collider) {
            registry.emplace<world::Collider>(entity, *description.collider);
        }
        if (description.rigidBody) {
            registry.emplace<world::RigidBody>(entity, *description.rigidBody);
        }
        if (description.camera) {
            registry.emplace<world::Camera>(
                entity, world::Camera{.verticalFovRadians = glm::radians(description.camera->fovDegrees),
                                      .nearPlane = description.camera->nearPlane,
                                      .farPlane = description.camera->farPlane});
        }
        if (description.light) {
            registry.emplace<world::DirectionalLight>(
                entity, world::DirectionalLight{.direction = glm::normalize(description.light->direction),
                                                .color = description.light->color,
                                                .intensity = description.light->intensity});
        }
    }
}

core::Result<void> loadSceneFile(const std::filesystem::path& file, std::string_view sourceName,
                                 entt::registry& registry, resources::ResourceCache& cache) {
    const core::Result<std::vector<std::byte>> bytes = resources::readFile(file);
    if (!bytes) {
        return core::makeError(bytes.error().code,
                               std::format("Scene '{}': {}", sourceName, bytes.error().message));
    }
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    const core::Result<SceneDescription> scene = parseScene(text, sourceName);
    if (!scene) {
        return std::unexpected(scene.error());
    }
    instantiateScene(*scene, registry, cache);
    return {};
}

SceneDescription describeScene(const entt::registry& registry, const resources::ResourceCache& cache) {
    SceneDescription scene;
    scene.ambient = world::sceneEnvironment(registry).ambient;

    for (const auto [entity, sceneId] : registry.view<world::SceneId>().each()) {
        EntityDesc description;
        description.id = sceneId.value;
        if (const world::Name* name = registry.try_get<world::Name>(entity)) {
            description.name = name->value;
        }
        if (const world::Transform* transform = registry.try_get<world::Transform>(entity)) {
            description.transform =
                TransformDesc{.position = transform->position,
                              .rotationDegrees = world::eulerDegreesFromRotation(transform->rotation),
                              .scale = transform->scale};
        }
        if (const world::MeshRenderer* renderer = registry.try_get<world::MeshRenderer>(entity)) {
            if (const std::optional<resources::AssetPath> path = cache.modelPath(renderer->model)) {
                description.mesh = path->str();
                if (const resources::MaterialData* material =
                        cache.materials().find(renderer->materialOverride)) {
                    description.material = MaterialDesc{.baseColor = glm::vec3(material->baseColorFactor),
                                                        .metallic = material->metallicFactor,
                                                        .roughness = material->roughnessFactor};
                }
            }
        }
        if (const world::Collider* collider = registry.try_get<world::Collider>(entity)) {
            description.collider = *collider;
        }
        if (const world::RigidBody* body = registry.try_get<world::RigidBody>(entity)) {
            description.rigidBody = *body;
        }
        if (const world::Camera* camera = registry.try_get<world::Camera>(entity)) {
            description.camera = CameraDesc{.fovDegrees = glm::degrees(camera->verticalFovRadians),
                                            .nearPlane = camera->nearPlane,
                                            .farPlane = camera->farPlane};
        }
        if (const world::DirectionalLight* light = registry.try_get<world::DirectionalLight>(entity)) {
            description.light = LightDesc{
                .direction = light->direction, .color = light->color, .intensity = light->intensity};
        }
        scene.entities.push_back(std::move(description));
    }
    // The registry has no order of its own: sorted, so that the same scene always gives the same result.
    std::ranges::sort(scene.entities, {}, &EntityDesc::id);
    return scene;
}

} // namespace apex::serialization
