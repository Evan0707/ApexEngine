#pragma once

#include "SceneExtraction.h"

#include "Core/Error.h"
#include "Graphics/GpuHandles.h"
#include "Graphics/RenderScene.h"
#include "Graphics/Renderer.h"
#include "Resources/MaterialData.h"

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

// The demo scene's GPU objects. The renderer must outlive it; its meshes and materials are destroyed
// with it.
class DemoScene {
public:
    [[nodiscard]] static core::Result<DemoScene> create(graphics::Renderer& renderer);

    ~DemoScene();
    DemoScene(DemoScene&& other) noexcept;
    DemoScene& operator=(DemoScene&&) = delete;
    DemoScene(const DemoScene&) = delete;
    DemoScene& operator=(const DemoScene&) = delete;

    // One fixed step (G-05).
    void simulate(double deltaSeconds) noexcept;
    // Fills everything but the camera: objects relative to the camera position (G-07), interpolated
    // between the last two steps (alpha in [0, 1]), light, ambient.
    void extract(const core::WorldPosition& cameraPosition, float alpha, graphics::RenderScene& scene) const;

    [[nodiscard]] std::size_t objectCount() const noexcept;

private:
    struct Object {
        graphics::MeshHandle mesh;
        graphics::MaterialHandle material;
        ObjectTransform transform;
        float spinRadiansPerSecond = 0.0f;
        float previousSpin = 0.0f;
        float spin = 0.0f;
    };

    explicit DemoScene(graphics::Renderer& renderer) noexcept;

    graphics::Renderer* m_renderer;
    std::vector<graphics::MeshHandle> m_meshes;
    std::vector<graphics::MaterialHandle> m_materials;
    std::vector<Object> m_objects;
};

} // namespace apex::game
