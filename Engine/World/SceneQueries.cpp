#include "World/SceneQueries.h"

namespace apex::world {

entt::entity findBySceneId(const entt::registry& registry, std::string_view id) {
    for (const auto [entity, sceneId] : registry.view<SceneId>().each()) {
        if (sceneId.value == id) {
            return entity;
        }
    }
    return entt::null;
}

std::size_t sceneEntityCount(const entt::registry& registry) {
    return registry.view<SceneId>().size();
}

SceneEnvironment sceneEnvironment(const entt::registry& registry) {
    const SceneEnvironment* environment = registry.ctx().find<SceneEnvironment>();
    return environment != nullptr ? *environment : SceneEnvironment{};
}

} // namespace apex::world
