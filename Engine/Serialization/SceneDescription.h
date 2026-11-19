#pragma once

#include "Core/Math.h"
#include "World/Components.h"

#include <optional>
#include <string>
#include <vector>

namespace apex::serialization {

// The only scene format version this build reads and writes (spec 5.6).
inline constexpr int kSceneVersion = 1;

// A scene as plain data, in the units of the file (decision 0017): degrees, and paths instead of handles.
// Parsing gives one that is fully validated; nothing here refers to JSON, to the registry or to the cache.

struct TransformDesc {
    glm::vec3 position{0.0f};
    // Around X, Y and Z, applied in the order Y, X, Z (world::rotationFromEulerDegrees).
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] bool operator==(const TransformDesc&) const noexcept = default;
};

// Replaces the material of the entity's model: factors only, no texture.
struct MaterialDesc {
    // Linear RGB.
    glm::vec3 baseColor{0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;

    [[nodiscard]] bool operator==(const MaterialDesc&) const noexcept = default;
};

struct CameraDesc {
    // Vertical.
    float fovDegrees = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

    [[nodiscard]] bool operator==(const CameraDesc&) const noexcept = default;
};

struct LightDesc {
    // The way the light travels; not necessarily unit length.
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    [[nodiscard]] bool operator==(const LightDesc&) const noexcept = default;
};

struct EntityDesc {
    // Unique in the scene, never empty.
    std::string id;
    std::optional<std::string> name;
    std::optional<TransformDesc> transform;
    // Normalized asset path of a model.
    std::optional<std::string> mesh;
    std::optional<MaterialDesc> material;
    std::optional<world::Collider> collider;
    std::optional<world::RigidBody> rigidBody;
    std::optional<CameraDesc> camera;
    std::optional<LightDesc> light;

    [[nodiscard]] bool operator==(const EntityDesc&) const noexcept = default;
};

struct SceneDescription {
    // Constant ambient term, linear RGB.
    glm::vec3 ambient{0.03f};
    std::vector<EntityDesc> entities;

    [[nodiscard]] bool operator==(const SceneDescription&) const noexcept = default;
};

} // namespace apex::serialization
