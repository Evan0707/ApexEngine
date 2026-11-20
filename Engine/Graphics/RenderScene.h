#pragma once

#include "Core/Math.h"
#include "Resources/Handles.h"

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

// The shadow area (spec 5.4, decision 0021), framed by the extraction: a square seen from the light.
struct RenderShadow {
    bool enabled = false;
    // Render space to the light's view space, a rotation and a translation: the view looks along the light,
    // towards -Z, from the middle of the area's near face.
    glm::mat4 lightView{1.0f};
    // Meters: half the side of the square, and its depth along the light.
    float halfExtent = 20.0f;
    float depthRange = 100.0f;
    // Depth bias of the shadow pass: constant in meters, slope factor in units of the depth slope.
    float constantBias = 0.02f;
    float slopeBias = 2.0f;
};

// How the HDR image of the scene becomes the displayed one (spec 5.4, decision 0021).
struct RenderToneMapping {
    // Stops: the HDR image is multiplied by 2^exposureStops.
    float exposureStops = 0.0f;
    // Off: values above 1 clip, which isolates the curve when comparing (AC-P1-5).
    bool enabled = true;
};

// The light around the scene (spec 5.4, decision 0022).
struct RenderEnvironment {
    // Not ready, failed, stale or default: the constant ambient term lights the scene, without a sky (G-03).
    resources::EnvironmentHandle environment;
    // Multiplies the environment's radiance.
    float intensity = 1.0f;
    // Off: the constant ambient term, as without an environment.
    bool enabled = true;
};

// One mesh with one material: a model gives one object per part (resources::ResourceCache::parts).
struct RenderObject {
    // Not ready, failed or stale: the fallback mesh is drawn (G-03).
    resources::MeshHandle mesh;
    // Not ready, failed or stale: the default material.
    resources::MaterialHandle material;
    // Object to render space.
    glm::mat4 modelRelativeToCamera{1.0f};
};

// Everything Graphics draws in a frame (G-01): copied data, no pointer to the game's objects.
struct RenderScene {
    RenderCamera camera;
    RenderLight light;
    // Constant ambient term, linear RGB (spec 5.5): without an environment only.
    glm::vec3 ambient{0.03f};
    RenderShadow shadow;
    RenderToneMapping toneMapping;
    RenderEnvironment environment;
    std::vector<RenderObject> objects;
};

} // namespace apex::graphics
