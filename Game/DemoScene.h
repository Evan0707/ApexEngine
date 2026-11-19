#pragma once

#include "SceneExtraction.h"

#include "Graphics/RenderScene.h"
#include "Resources/Handles.h"
#include "Resources/MaterialData.h"
#include "Resources/ResourceCache.h"

#include <cstddef>
#include <vector>

namespace apex::game {

enum class DemoShape { Cube, Sphere };

// One object of the demo scene, as plain data: tested without a GPU.
struct DemoObjectDesc {
    DemoShape shape;
    // Index in demoMaterials().
    std::size_t material;
    ObjectTransform transform;
    // Turns around world +Y by fixed steps, to show the interpolation of spec 5.1.
    float spinDegreesPerSecond = 0.0f;
};

// Phase 3 scene, written in code until the JSON scene of phase 5: a floor, a reference cube of 1 m,
// boxes, and rows of spheres from smooth to rough.
[[nodiscard]] std::vector<resources::MaterialData> demoMaterials();
[[nodiscard]] std::vector<DemoObjectDesc> demoObjects();
[[nodiscard]] graphics::RenderLight demoLight() noexcept;
[[nodiscard]] glm::vec3 demoAmbient() noexcept;

// The objects of the demo scene, each drawn as a model of the resource cache (decision 0014). The cache
// owns the meshes and materials, and must outlive the scene.
class DemoScene {
public:
    // Adds the scene's primitives and materials to the cache. An invalid one is logged by the cache and
    // drawn with the fallbacks.
    [[nodiscard]] static DemoScene create(resources::ResourceCache& cache);

    // One fixed step (G-05).
    void simulate(double deltaSeconds) noexcept;
    // Fills everything but the camera: one object per model part, relative to the camera position
    // (G-07), interpolated between the last two steps (alpha in [0, 1]); light, ambient.
    void extract(const core::WorldPosition& cameraPosition, float alpha, graphics::RenderScene& scene) const;

    [[nodiscard]] std::size_t objectCount() const noexcept;

private:
    struct Object {
        resources::ModelHandle model;
        ObjectTransform transform;
        float spinRadiansPerSecond = 0.0f;
        float previousSpin = 0.0f;
        float spin = 0.0f;
    };

    explicit DemoScene(const resources::ResourceCache& cache) noexcept;

    const resources::ResourceCache* m_cache;
    std::vector<Object> m_objects;
};

} // namespace apex::game
