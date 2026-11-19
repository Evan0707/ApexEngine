#include "Serialization/SceneLoader.h"

#include "Resources/FileReader.h"
#include "Serialization/SceneJson.h"
#include "World/Rotation.h"

#include <format>
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

} // namespace apex::serialization
