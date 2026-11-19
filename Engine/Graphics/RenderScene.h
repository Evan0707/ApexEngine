#pragma once

#include "Core/Math.h"
#include "Graphics/GpuHandles.h"

#include <vector>

namespace apex::graphics {

// Render space is world space translated so that the camera sits at its origin (G-07): positions stay
// small and precise near the camera, however far it is from the world origin.
struct RenderCamera {
    // Rotation only, render space to view space. View space looks towards -Z, +Y up.
    glm::mat4 view{1.0f};
    float verticalFovRadians = glm::radians(70.0f);
    // Meters.
    float nearPlane = 0.1f;
    float farPlane = 500.0f;
};

struct RenderLight {
    // Unit vector in world space, from the surface towards the light.
    glm::vec3 directionToLight{0.0f, 1.0f, 0.0f};
    // Linear RGB.
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct RenderObject {
    MeshHandle mesh;
    // A stale or default handle gives the default material.
    MaterialHandle material;
    // Object to render space.
    glm::mat4 modelRelativeToCamera{1.0f};
};

// Everything Graphics draws in a frame (G-01): copied data, no pointer to the game's objects.
struct RenderScene {
    RenderCamera camera;
    RenderLight light;
    // Constant ambient term, linear RGB (spec 5.5).
    glm::vec3 ambient{0.03f};
    std::vector<RenderObject> objects;
};

} // namespace apex::graphics
