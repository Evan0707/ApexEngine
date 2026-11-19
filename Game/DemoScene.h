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

    // Fills everything but the camera: objects relative to the camera position (G-07), light, ambient.
    void extract(const core::WorldPosition& cameraPosition, graphics::RenderScene& scene) const;

    [[nodiscard]] std::size_t objectCount() const noexcept;

private:
    struct Object {
        graphics::MeshHandle mesh;
        graphics::MaterialHandle material;
        ObjectTransform transform;
    };

    explicit DemoScene(graphics::Renderer& renderer) noexcept;

    graphics::Renderer* m_renderer;
    std::vector<graphics::MeshHandle> m_meshes;
    std::vector<graphics::MaterialHandle> m_materials;
    std::vector<Object> m_objects;
};

} // namespace apex::game
