#pragma once

#include "Core/Error.h"
#include "Resources/ResourceCache.h"
#include "Serialization/SceneDescription.h"

#include <entt/entity/registry.hpp>

#include <filesystem>
#include <string_view>

namespace apex::serialization {

// Creates the entities of a scene in the registry, in the order of the description, and sets the scene's
// environment in the registry's context. Models are loaded through the cache: one that fails is logged
// there and drawn as the fallback (AC-11); it does not fail the scene.
void instantiateScene(const SceneDescription& scene, entt::registry& registry,
                      resources::ResourceCache& cache);

// The inverse of instantiateScene: the entities that carry a SceneId, sorted by id, in the units of the
// file. A MeshRenderer whose model was not loaded from a file is left out, with its material.
[[nodiscard]] SceneDescription describeScene(const entt::registry& registry,
                                             const resources::ResourceCache& cache);

// Reads, checks and instantiates a scene file. On any error the registry is left untouched; the error
// names the file (`sourceName`, its asset path), the entity and the field (spec 5.6).
[[nodiscard]] core::Result<void> loadSceneFile(const std::filesystem::path& file, std::string_view sourceName,
                                               entt::registry& registry, resources::ResourceCache& cache);

} // namespace apex::serialization
