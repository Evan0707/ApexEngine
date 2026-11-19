#pragma once

#include "World/Components.h"

#include <entt/entity/registry.hpp>

#include <cstddef>
#include <string_view>

namespace apex::world {

// The entity with this SceneId, entt::null when there is none. Linear: meant for setup, not per frame.
[[nodiscard]] entt::entity findBySceneId(const entt::registry& registry, std::string_view id);

// Entities that come from a scene: those with a SceneId.
[[nodiscard]] std::size_t sceneEntityCount(const entt::registry& registry);

// The scene's ambient term; the default of SceneEnvironment when the registry has none.
[[nodiscard]] SceneEnvironment sceneEnvironment(const entt::registry& registry);

} // namespace apex::world
