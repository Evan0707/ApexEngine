#include "Resources/GltfImporter.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

using namespace apex::resources;

namespace {

[[nodiscard]] ModelData importOrFail(const std::filesystem::path& file) {
    auto model = importGltf(file, file.filename().string());
    if (!model) {
        FAIL(model.error().message);
    }
    return std::move(*model);
}

[[nodiscard]] std::filesystem::path testData(const char* relative) {
    return std::filesystem::path(APEX_TEST_DATA_DIR) / relative;
}

[[nodiscard]] std::filesystem::path asset(const char* relative) {
    return std::filesystem::path(APEX_ASSETS_DIR) / relative;
}

[[nodiscard]] const ModelMaterial& materialOf(const ModelData& model, std::size_t part) {
    return model.materials.at(model.parts.at(part).material);
}

[[nodiscard]] bool isPng(const std::vector<std::byte>& bytes) {
    return bytes.size() > 8 && bytes[1] == std::byte{'P'} && bytes[2] == std::byte{'N'} &&
           bytes[3] == std::byte{'G'};
}

} // namespace

TEST_CASE("Material factors are read from the file", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/two_materials.gltf"));
    const ModelMaterial& red = materialOf(model, 0);
    REQUIRE(red.name == "Red");
    REQUIRE(red.factors.baseColorFactor == glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(red.factors.metallicFactor == 0.0f);
    REQUIRE(red.factors.roughnessFactor == 0.25f);
    const ModelMaterial& gold = materialOf(model, 1);
    REQUIRE(gold.factors.metallicFactor == 1.0f);
    REQUIRE(gold.factors.roughnessFactor == 0.5f);
    REQUIRE_FALSE(red.baseColorImage.has_value());
}

TEST_CASE("A primitive without a material gets the engine's default material", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/non_indexed.gltf"));
    const ModelMaterial& material = materialOf(model, 0);
    REQUIRE(material.name == "Default");
    REQUIRE(material.factors.metallicFactor == MaterialData{}.metallicFactor);
    REQUIRE(material.factors.baseColorFactor == MaterialData{}.baseColorFactor);
}

TEST_CASE("Textures point to external files or to bytes inside the model", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/textured.gltf"));
    const ModelMaterial& material = materialOf(model, 0);
    REQUIRE(material.factors.baseColorFactor == glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
    REQUIRE(material.factors.metallicFactor == 0.25f);
    REQUIRE(material.factors.roughnessFactor == 0.75f);

    REQUIRE(material.baseColorImage.has_value());
    const ModelImage& color = model.images.at(*material.baseColorImage);
    REQUIRE(color.uri == "../Images/rgba_2x2.png");
    REQUIRE(color.bytes.empty());

    REQUIRE(material.metallicRoughnessImage.has_value());
    const ModelImage& metalRough = model.images.at(*material.metallicRoughnessImage);
    REQUIRE(metalRough.uri.empty());
    REQUIRE(isPng(metalRough.bytes));
}

TEST_CASE("A GLB carries its images inside", "[resources]") {
    const ModelData model = importOrFail(asset("Models/reference_cube.glb"));
    const ModelMaterial& material = materialOf(model, 0);
    REQUIRE(material.baseColorImage.has_value());
    REQUIRE(isPng(model.images.at(*material.baseColorImage).bytes));
    REQUIRE_FALSE(material.metallicRoughnessImage.has_value());
}

TEST_CASE("The crate refers to its JPG and PNG next to it", "[resources]") {
    const ModelData model = importOrFail(asset("Models/crate.gltf"));
    const ModelMaterial& material = materialOf(model, 0);
    REQUIRE(model.images.at(*material.baseColorImage).uri == "crate_color.jpg");
    REQUIRE(model.images.at(*material.metallicRoughnessImage).uri == "crate_metal_rough.png");
}

TEST_CASE("A missing texture does not prevent the model from importing", "[resources]") {
    const ModelData model = importOrFail(testData("Gltf/missing_texture.gltf"));
    const ModelMaterial& material = materialOf(model, 0);
    REQUIRE(model.images.at(*material.baseColorImage).uri == "absent.png");
}
