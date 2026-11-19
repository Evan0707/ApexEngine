#include "World/Components.h"
#include "World/SceneQueries.h"

#include <catch2/catch_test_macros.hpp>

using namespace apex::world;

TEST_CASE("An entity is found by its scene id", "[world]") {
    entt::registry registry;
    const entt::entity crate = registry.create();
    registry.emplace<SceneId>(crate, "crate_01");
    const entt::entity sun = registry.create();
    registry.emplace<SceneId>(sun, "sun");
    // Not from a scene: no id.
    (void)registry.create();

    REQUIRE(findBySceneId(registry, "crate_01") == crate);
    REQUIRE(findBySceneId(registry, "sun") == sun);
    REQUIRE(findBySceneId(registry, "absent") == entt::entity{entt::null});
    REQUIRE(sceneEntityCount(registry) == 2);
}

TEST_CASE("An empty registry has no scene entity", "[world]") {
    const entt::registry registry;
    REQUIRE(findBySceneId(registry, "anything") == entt::entity{entt::null});
    REQUIRE(sceneEntityCount(registry) == 0);
}

TEST_CASE("The scene environment comes from the registry's context", "[world]") {
    entt::registry registry;
    REQUIRE(sceneEnvironment(registry) == SceneEnvironment{});
    registry.ctx().emplace<SceneEnvironment>(SceneEnvironment{.ambient = {0.1f, 0.2f, 0.3f}});
    REQUIRE(sceneEnvironment(registry).ambient == glm::vec3(0.1f, 0.2f, 0.3f));
}

TEST_CASE("Components start with usable defaults", "[world]") {
    const Transform transform;
    REQUIRE(transform.position == glm::vec3(0.0f));
    REQUIRE(transform.scale == glm::vec3(1.0f));
    REQUIRE(transform.rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    REQUIRE(MeshRenderer{}.materialOverride == apex::resources::MaterialHandle{});
    REQUIRE(RigidBody{}.type == BodyType::Static);
    REQUIRE(DirectionalLight{}.direction.y < 0.0f);
}
