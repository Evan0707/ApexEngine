#include "DemoScene.h"

#include "Resources/Primitives.h"

#include <array>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>
#include <utility>

namespace apex::game {

namespace {

constexpr std::size_t kFloor = 0;
constexpr std::size_t kReference = 1;
constexpr std::size_t kRed = 2;
constexpr std::size_t kGreen = 3;
constexpr std::size_t kBlue = 4;
constexpr std::size_t kCrate = 5;
// Then five dielectric spheres and five metallic spheres, roughness 0 to 1.
constexpr std::size_t kFirstDielectricSphere = 6;
constexpr std::size_t kFirstMetalSphere = 11;
constexpr std::size_t kSpheresPerRow = 5;

constexpr std::uint32_t kSphereSegments = 48;
constexpr std::uint32_t kSphereRings = 24;

[[nodiscard]] glm::quat yaw(float degrees) {
    return glm::angleAxis(glm::radians(degrees), core::kWorldUp);
}

[[nodiscard]] float rowRoughness(std::size_t index) {
    return static_cast<float>(index) / static_cast<float>(kSpheresPerRow - 1);
}

} // namespace

std::vector<resources::MaterialData> demoMaterials() {
    // Linear colors.
    std::vector<resources::MaterialData> materials{
        {.baseColorFactor = {0.35f, 0.36f, 0.38f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.9f},
        {.baseColorFactor = {0.8f, 0.8f, 0.8f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.5f},
        {.baseColorFactor = {0.7f, 0.08f, 0.06f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.4f},
        {.baseColorFactor = {0.1f, 0.45f, 0.12f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.6f},
        {.baseColorFactor = {0.08f, 0.2f, 0.6f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.3f},
        {.baseColorFactor = {0.55f, 0.3f, 0.12f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.7f},
    };
    for (std::size_t i = 0; i < kSpheresPerRow; ++i) {
        materials.push_back({.baseColorFactor = {0.8f, 0.8f, 0.8f, 1.0f},
                             .metallicFactor = 0.0f,
                             .roughnessFactor = rowRoughness(i)});
    }
    for (std::size_t i = 0; i < kSpheresPerRow; ++i) {
        // Gold.
        materials.push_back({.baseColorFactor = {1.0f, 0.77f, 0.34f, 1.0f},
                             .metallicFactor = 1.0f,
                             .roughnessFactor = rowRoughness(i)});
    }
    return materials;
}

std::vector<DemoObjectDesc> demoObjects() {
    std::vector<DemoObjectDesc> objects{
        // Top face at y = 0.
        {DemoShape::Cube, kFloor, {.position = {0.0f, -0.1f, 0.0f}, .scale = {30.0f, 0.2f, 30.0f}}},
        // Reference cube (spec 6.1): 1 m, resting on the floor at the origin.
        {DemoShape::Cube, kReference, {.position = {0.0f, 0.5f, 0.0f}}},
        {DemoShape::Cube, kRed, {.position = {-2.5f, 0.5f, 1.0f}, .rotation = yaw(20.0f)}},
        {DemoShape::Cube,
         kGreen,
         {.position = {2.5f, 0.75f, 0.5f}, .rotation = yaw(-15.0f), .scale = glm::vec3(1.5f)}},
        {DemoShape::Cube,
         kBlue,
         {.position = {-1.2f, 0.3f, 2.5f}, .rotation = yaw(45.0f), .scale = glm::vec3(0.6f)}},
        {DemoShape::Cube, kCrate, {.position = {4.5f, 0.5f, -1.5f}}},
        {DemoShape::Cube, kCrate, {.position = {4.5f, 1.5f, -1.5f}, .rotation = yaw(10.0f)}},
        {DemoShape::Cube, kReference, {.position = {-4.5f, 1.5f, -2.0f}, .scale = {0.6f, 3.0f, 0.6f}}},
        {DemoShape::Cube, kBlue, {.position = {1.8f, 0.4f, 2.2f}, .scale = glm::vec3(0.8f)}, 45.0f},
    };
    for (std::size_t i = 0; i < kSpheresPerRow; ++i) {
        const float x = -4.0f + 2.0f * static_cast<float>(i);
        objects.push_back({DemoShape::Sphere, kFirstDielectricSphere + i, {.position = {x, 0.5f, -4.0f}}});
        objects.push_back({DemoShape::Sphere, kFirstMetalSphere + i, {.position = {x, 0.5f, -6.0f}}});
    }
    return objects;
}

graphics::RenderLight demoLight() noexcept {
    return graphics::RenderLight{.directionToLight = glm::normalize(glm::vec3(0.4f, 1.0f, 0.35f)),
                                 .color = {1.0f, 0.95f, 0.88f},
                                 .intensity = 3.0f};
}

glm::vec3 demoAmbient() noexcept {
    return {0.06f, 0.07f, 0.09f};
}

DemoScene DemoScene::create(resources::ResourceCache& cache) {
    DemoScene scene(cache);
    // Unit sizes: the transforms scale them.
    const std::array<resources::MeshHandle, 2> meshes{
        cache.addMesh("Cube", resources::makeCube(1.0f)),
        cache.addMesh("Sphere", resources::makeSphere(0.5f, kSphereSegments, kSphereRings)),
    };
    std::vector<resources::MaterialHandle> materials;
    const std::vector<resources::MaterialData> materialData = demoMaterials();
    for (std::size_t i = 0; i < materialData.size(); ++i) {
        materials.push_back(cache.addMaterial(std::format("Demo material {}", i), materialData[i]));
    }
    for (const DemoObjectDesc& desc : demoObjects()) {
        const resources::ModelPartRef part{.mesh = meshes[static_cast<std::size_t>(desc.shape)],
                                           .material = materials[desc.material]};
        scene.m_objects.push_back(Object{.model = cache.addModel(std::span(&part, 1)),
                                         .transform = desc.transform,
                                         .spinRadiansPerSecond = glm::radians(desc.spinDegreesPerSecond)});
    }
    return scene;
}

DemoScene::DemoScene(const resources::ResourceCache& cache) noexcept : m_cache(&cache) {}

void DemoScene::simulate(double deltaSeconds) noexcept {
    for (Object& object : m_objects) {
        object.previousSpin = object.spin;
        // Wrapped together, so that the interpolation never goes the long way round.
        object.spin += object.spinRadiansPerSecond * static_cast<float>(deltaSeconds);
        if (object.spin > glm::two_pi<float>()) {
            object.spin -= glm::two_pi<float>();
            object.previousSpin -= glm::two_pi<float>();
        }
    }
}

void DemoScene::extract(const core::WorldPosition& cameraPosition, float alpha,
                        graphics::RenderScene& scene) const {
    scene.light = demoLight();
    scene.ambient = demoAmbient();
    scene.objects.clear();
    for (const Object& object : m_objects) {
        ObjectTransform transform = object.transform;
        const float spin = glm::mix(object.previousSpin, object.spin, alpha);
        transform.rotation = glm::angleAxis(spin, core::kWorldUp) * transform.rotation;
        const glm::mat4 model = modelRelativeToCamera(transform, cameraPosition);
        // A model that is not ready gives the fallback model's parts (G-03).
        for (const resources::ModelPartRef& part : m_cache->parts(object.model)) {
            scene.objects.push_back(graphics::RenderObject{
                .mesh = part.mesh, .material = part.material, .modelRelativeToCamera = model});
        }
    }
}

std::size_t DemoScene::objectCount() const noexcept {
    return m_objects.size();
}

} // namespace apex::game
