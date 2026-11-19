#include "DemoScene.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>

using namespace apex::game;

namespace {

[[nodiscard]] float bottom(const ObjectTransform& transform) {
    return transform.position.y - 0.5f * transform.scale.y;
}

} // namespace

TEST_CASE("The demo scene has a 1 m reference cube resting on the floor", "[game]") {
    const auto objects = demoObjects();
    const auto reference = std::ranges::find_if(objects, [](const DemoObjectDesc& object) {
        return object.shape == DemoShape::Cube && object.transform.position == glm::vec3(0.0f, 0.5f, 0.0f);
    });
    REQUIRE(reference != objects.end());
    REQUIRE(reference->transform.scale == glm::vec3(1.0f));
    REQUIRE(reference->transform.rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    REQUIRE(bottom(reference->transform) == 0.0f);

    // The floor is the first object; its top face is at y = 0.
    const ObjectTransform& floor = objects.front().transform;
    REQUIRE(floor.position.y + 0.5f * floor.scale.y == 0.0f);
}

TEST_CASE("Every demo object uses a valid material", "[game]") {
    const auto materials = demoMaterials();
    for (const DemoObjectDesc& object : demoObjects()) {
        REQUIRE(object.material < materials.size());
    }
    // Leaves room in the renderer's table of 256, default material included.
    REQUIRE(materials.size() < 256);
    for (const auto& material : materials) {
        REQUIRE(material.metallicFactor >= 0.0f);
        REQUIRE(material.metallicFactor <= 1.0f);
        REQUIRE(material.roughnessFactor >= 0.0f);
        REQUIRE(material.roughnessFactor <= 1.0f);
    }
}

TEST_CASE("The sphere rows go from smooth to rough, dielectric and metallic", "[game]") {
    const auto materials = demoMaterials();
    std::size_t dielectric = 0;
    std::size_t metallic = 0;
    bool smooth = false;
    bool rough = false;
    for (const DemoObjectDesc& object : demoObjects()) {
        if (object.shape != DemoShape::Sphere) {
            continue;
        }
        const auto& material = materials[object.material];
        (material.metallicFactor == 1.0f ? metallic : dielectric) += 1;
        smooth = smooth || material.roughnessFactor == 0.0f;
        rough = rough || material.roughnessFactor == 1.0f;
    }
    REQUIRE(dielectric == 5);
    REQUIRE(metallic == 5);
    REQUIRE(smooth);
    REQUIRE(rough);
}

TEST_CASE("The demo light points down onto the scene", "[game]") {
    const auto light = demoLight();
    REQUIRE(light.directionToLight.y > 0.0f);
    REQUIRE(glm::length(light.directionToLight) > 0.999f);
    REQUIRE(light.intensity > 0.0f);
}
