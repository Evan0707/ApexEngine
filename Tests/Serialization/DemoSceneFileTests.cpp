#include "Core/Logger.h"
#include "Resources/ResourceCache.h"
#include "Serialization/SceneLoader.h"
#include "World/Components.h"
#include "World/SceneQueries.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace apex;

namespace {

class ErrorSink final : public core::LogSink {
public:
    explicit ErrorSink(std::vector<std::string>& errors) : m_errors(errors) {}
    void write(const core::LogRecord& record) override {
        if (record.level >= core::LogLevel::Warning) {
            m_errors.emplace_back(record.message);
        }
    }

private:
    std::vector<std::string>& m_errors;
};

// Assets/Scenes/demo.json, loaded the way the demo loads it.
struct DemoFile {
    DemoFile() {
        logger.addSink(std::make_unique<ErrorSink>(problems));
        cache = std::make_unique<resources::ResourceCache>(std::filesystem::path(APEX_ASSETS_DIR), logger);
        const auto loaded = serialization::loadSceneFile(cache->root() / "Scenes/demo.json",
                                                         "Scenes/demo.json", registry, *cache);
        if (!loaded) {
            FAIL(loaded.error().message);
        }
    }

    std::vector<std::string> problems;
    core::Logger logger;
    std::unique_ptr<resources::ResourceCache> cache;
    entt::registry registry;
};

} // namespace

TEST_CASE("demo.json loads without an error or a warning, every model ready", "[serialization]") {
    const DemoFile demo;
    for (const std::string& problem : demo.problems) {
        UNSCOPED_INFO(problem);
    }
    REQUIRE(demo.problems.empty());
    REQUIRE(world::sceneEntityCount(demo.registry) == 22);
    for (const auto [entity, renderer] : demo.registry.view<world::MeshRenderer>().each()) {
        CAPTURE(demo.registry.get<world::SceneId>(entity).value);
        REQUIRE(demo.cache->state(renderer.model) == resources::ResourceState::Ready);
    }
}

TEST_CASE("demo.json has the entities the demo looks for", "[serialization]") {
    const DemoFile demo;
    const entt::entity camera = world::findBySceneId(demo.registry, "camera");
    REQUIRE(demo.registry.all_of<world::Transform, world::Camera>(camera));
    REQUIRE(demo.registry.all_of<world::Transform, world::MeshRenderer>(
        world::findBySceneId(demo.registry, "spinner")));
    REQUIRE(demo.registry.view<world::DirectionalLight>().size() == 1);
    // The sun shines downwards.
    REQUIRE(
        demo.registry.get<world::DirectionalLight>(world::findBySceneId(demo.registry, "sun")).direction.y <
        0.0f);
}

TEST_CASE("demo.json has a 1 m reference cube resting on the floor at the origin", "[serialization]") {
    const DemoFile demo;
    const world::Transform& cube =
        demo.registry.get<world::Transform>(world::findBySceneId(demo.registry, "reference_cube"));
    REQUIRE(cube.position == glm::vec3(0.0f, 0.5f, 0.0f));
    REQUIRE(cube.scale == glm::vec3(1.0f));
    REQUIRE(cube.rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // Top of the floor at y = 0: a unit cube scaled to 0.2 m, centered 0.1 m below.
    const world::Transform& floor =
        demo.registry.get<world::Transform>(world::findBySceneId(demo.registry, "floor"));
    REQUIRE(floor.position.y + 0.5f * floor.scale.y == 0.0f);
    REQUIRE(demo.registry.get<world::RigidBody>(world::findBySceneId(demo.registry, "floor")).type ==
            world::BodyType::Static);
}

TEST_CASE("demo.json shows spheres from smooth to rough, plain and metallic", "[serialization]") {
    const DemoFile demo;
    int plain = 0;
    int metallic = 0;
    for (const auto [entity, id, renderer] :
         demo.registry.view<world::SceneId, world::MeshRenderer>().each()) {
        if (!id.value.starts_with("sphere_")) {
            continue;
        }
        const resources::MaterialData* material = demo.cache->materials().find(renderer.materialOverride);
        REQUIRE(material != nullptr);
        (material->metallicFactor == 1.0f ? metallic : plain) += 1;
    }
    REQUIRE(plain == 5);
    REQUIRE(metallic == 5);
}
