#pragma once

#include "Core/Math.h"
#include "Core/WorldPosition.h"
#include "Resources/Handles.h"

#include <string>

namespace apex::world {

// The engine's components (spec 5.6): plain data, attached to the entities of an entt::registry. The
// components of the demo's own gameplay live in Game.

// Unique and stable inside a scene: how the scene file and the game name an entity.
struct SceneId {
    std::string value;
};

// For people, optional.
struct Name {
    std::string value;
};

// No parent: the hierarchy of transforms is out of scope (spec 2.2).
struct Transform {
    core::WorldPosition position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] bool operator==(const Transform&) const noexcept = default;
};

// The transform at the previous fixed step, written by rememberTransforms: the extraction interpolates
// between it and Transform (spec 5.1). An entity without it is drawn at its current transform.
struct PreviousTransform {
    Transform value;
};

// A model of the resource cache, drawn at the entity's Transform.
struct MeshRenderer {
    resources::ModelHandle model;
    // Replaces the material of every part of the model. A default handle keeps the model's own.
    resources::MaterialHandle materialOverride;
};

enum class ColliderShape { Box, Sphere, Capsule };

// Meters, before the Transform's scale. Read by Physics from phase 6 on.
struct Collider {
    ColliderShape shape = ColliderShape::Box;
    // Box.
    glm::vec3 halfExtents{0.5f};
    // Sphere and capsule.
    float radius = 0.5f;
    // Capsule: half the distance between the centers of its two end spheres, along Y.
    float halfHeight = 0.5f;

    [[nodiscard]] bool operator==(const Collider&) const noexcept = default;
};

enum class BodyType { Static, Dynamic };

struct RigidBody {
    BodyType type = BodyType::Static;
    // Kilograms. Only read for a dynamic body.
    float mass = 1.0f;

    [[nodiscard]] bool operator==(const RigidBody&) const noexcept = default;
};

// Looks along the entity's -Z, +Y up.
struct Camera {
    float verticalFovRadians = glm::radians(70.0f);
    // Meters.
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

    [[nodiscard]] bool operator==(const Camera&) const noexcept = default;
};

struct DirectionalLight {
    // Unit vector in world space: the way the light travels. Independent of the entity's Transform.
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    // Linear RGB.
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    [[nodiscard]] bool operator==(const DirectionalLight&) const noexcept = default;
};

// Scene-wide settings, stored in the registry's context.
struct SceneEnvironment {
    // Constant ambient term, linear RGB (spec 5.5).
    glm::vec3 ambient{0.03f};

    [[nodiscard]] bool operator==(const SceneEnvironment&) const noexcept = default;
};

} // namespace apex::world
