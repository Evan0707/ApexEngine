#include "Core/Logger.h"
#include "Resources/Primitives.h"
#include "Resources/ResourceCache.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace apex::resources;
using apex::core::LogLevel;
using apex::core::LogRecord;

namespace {

class ErrorSink final : public apex::core::LogSink {
public:
    explicit ErrorSink(std::vector<std::string>& errors) : m_errors(errors) {}
    void write(const LogRecord& record) override {
        if (record.level >= LogLevel::Error) {
            m_errors.emplace_back(record.message);
        }
    }

private:
    std::vector<std::string>& m_errors;
};

// A cache over Tests/Data or Assets, and the errors it logged.
struct Fixture {
    explicit Fixture(const char* root) {
        logger.addSink(std::make_unique<ErrorSink>(errors));
        cache = std::make_unique<ResourceCache>(std::filesystem::path(root), logger);
    }

    [[nodiscard]] bool logged(std::string_view text) const {
        for (const std::string& error : errors) {
            if (error.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> errors;
    apex::core::Logger logger;
    std::unique_ptr<ResourceCache> cache;
};

} // namespace

TEST_CASE("A model path is loaded once, whatever its spelling", "[resources]") {
    Fixture fixture(APEX_ASSETS_DIR);
    const ModelHandle first = fixture.cache->loadModel("Models/crate.gltf");
    const ModelHandle again = fixture.cache->loadModel("Models\\.\\crate.gltf");
    REQUIRE(first == again);
    REQUIRE(fixture.cache->state(first) == ResourceState::Ready);
    // The model and its three textures.
    REQUIRE(fixture.cache->filesRead() == 4);
    REQUIRE(fixture.errors.empty());
}

TEST_CASE("A loaded model's material points to its loaded textures", "[resources]") {
    Fixture fixture(APEX_ASSETS_DIR);
    const ModelHandle crate = fixture.cache->loadModel("Models/crate.gltf");
    const auto parts = fixture.cache->parts(crate);
    REQUIRE(parts.size() == 1);
    REQUIRE(fixture.cache->meshes().find(parts[0].mesh) != nullptr);
    const MaterialData* material = fixture.cache->materials().find(parts[0].material);
    REQUIRE(material != nullptr);
    REQUIRE(fixture.cache->state(material->baseColorTexture) == ResourceState::Ready);
    REQUIRE(fixture.cache->textures().find(material->baseColorTexture)->colorSpace == ColorSpace::Srgb);
    REQUIRE(fixture.cache->textures().find(material->metallicRoughnessTexture)->colorSpace ==
            ColorSpace::Linear);
    // Normals are data, not colors.
    REQUIRE(fixture.cache->textures().find(material->normalTexture)->colorSpace == ColorSpace::Linear);
    REQUIRE(fixture.cache->loadTexture("Models/crate_color.jpg", ColorSpace::Srgb) ==
            material->baseColorTexture);
}

TEST_CASE("A missing model fails with its path logged and gives the fallback cube", "[resources]") {
    Fixture fixture(APEX_ASSETS_DIR);
    const ModelHandle missing = fixture.cache->loadModel("Models/absent.glb");
    REQUIRE(fixture.cache->state(missing) == ResourceState::Failed);
    REQUIRE(fixture.logged("Models/absent.glb"));
    const auto parts = fixture.cache->parts(missing);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].mesh == fixture.cache->fallbackMesh());
    REQUIRE(parts[0].material == fixture.cache->fallbackMaterial());
    const MaterialData* fallback = fixture.cache->materials().find(parts[0].material);
    REQUIRE(fallback->baseColorTexture == fixture.cache->checkerTexture());
    // Not retried: the failure is cached like a success.
    REQUIRE(fixture.cache->loadModel("Models/absent.glb") == missing);
    REQUIRE(fixture.cache->filesRead() == 1);
}

TEST_CASE("A missing texture fails alone: the model is still ready", "[resources]") {
    Fixture fixture(APEX_TEST_DATA_DIR);
    const ModelHandle model = fixture.cache->loadModel("Gltf/missing_texture.gltf");
    REQUIRE(fixture.cache->state(model) == ResourceState::Ready);
    const MaterialData* material = fixture.cache->materials().find(fixture.cache->parts(model)[0].material);
    REQUIRE(fixture.cache->state(material->baseColorTexture) == ResourceState::Failed);
    REQUIRE(fixture.logged("Gltf/absent.png"));
}

TEST_CASE("Textures next to the model and inside it are both loaded", "[resources]") {
    Fixture fixture(APEX_TEST_DATA_DIR);
    const ModelHandle model = fixture.cache->loadModel("Gltf/textured.gltf");
    const MaterialData* material = fixture.cache->materials().find(fixture.cache->parts(model)[0].material);
    const TextureData* color = fixture.cache->textures().find(material->baseColorTexture);
    REQUIRE(color != nullptr);
    REQUIRE(color->width == 2);
    REQUIRE(fixture.cache->loadTexture("Images/rgba_2x2.png", ColorSpace::Srgb) ==
            material->baseColorTexture);
    REQUIRE(fixture.cache->state(material->metallicRoughnessTexture) == ResourceState::Ready);
}

TEST_CASE("Invalid paths and corrupt files fail with a log line", "[resources]") {
    Fixture fixture(APEX_TEST_DATA_DIR);
    REQUIRE(fixture.cache->state(fixture.cache->loadModel("../outside.glb")) == ResourceState::Failed);
    REQUIRE(fixture.logged("../outside.glb"));
    const TextureHandle corrupt = fixture.cache->loadTexture("Images/corrupt.png", ColorSpace::Srgb);
    REQUIRE(fixture.cache->state(corrupt) == ResourceState::Failed);
    REQUIRE(fixture.logged("Images/corrupt.png"));
    REQUIRE(fixture.cache->state(fixture.cache->loadModel("Gltf/truncated.glb")) == ResourceState::Failed);
}

TEST_CASE("unloadAll makes every handle stale and keeps the fallbacks ready", "[resources]") {
    Fixture fixture(APEX_ASSETS_DIR);
    const ModelHandle crate = fixture.cache->loadModel("Models/crate.gltf");
    const MeshHandle mesh = fixture.cache->parts(crate)[0].mesh;
    fixture.cache->unloadAll();

    REQUIRE(fixture.cache->state(crate) == ResourceState::Failed);
    REQUIRE(fixture.cache->meshes().find(mesh) == nullptr);
    REQUIRE(fixture.cache->parts(crate)[0].mesh == fixture.cache->fallbackMesh());
    REQUIRE(fixture.cache->meshes().find(fixture.cache->fallbackMesh()) != nullptr);
    REQUIRE(fixture.cache->textures().find(fixture.cache->checkerTexture()) != nullptr);
    const TextureData* flat = fixture.cache->textures().find(fixture.cache->flatNormalTexture());
    REQUIRE(flat != nullptr);
    REQUIRE(flat->pixels == std::vector<std::uint8_t>{128, 128, 255, 255});

    // Loaded again: read again.
    REQUIRE(fixture.cache->state(fixture.cache->loadModel("Models/crate.gltf")) == ResourceState::Ready);
    REQUIRE(fixture.cache->filesRead() == 8);
}

TEST_CASE("Meshes and materials made in code are checked", "[resources]") {
    Fixture fixture(APEX_ASSETS_DIR);
    MeshData broken;
    REQUIRE(fixture.cache->meshes().state(fixture.cache->addMesh("Broken", broken)) == ResourceState::Failed);
    REQUIRE(fixture.logged("Broken"));
    const MaterialHandle bad = fixture.cache->addMaterial("Bad", MaterialData{.metallicFactor = 2.0f});
    REQUIRE(fixture.cache->materials().state(bad) == ResourceState::Failed);

    const MeshHandle cube = fixture.cache->addMesh("Cube", makeCube(1.0f));
    const MaterialHandle grey = fixture.cache->addMaterial("Grey", MaterialData{});
    const ModelPartRef part{.mesh = cube, .material = grey};
    const ModelHandle model = fixture.cache->addModel(std::span(&part, 1));
    REQUIRE(fixture.cache->state(model) == ResourceState::Ready);
    REQUIRE(fixture.cache->parts(model)[0].mesh == cube);
}
