#pragma once

#include "Core/Math.h"
#include "Core/WorldPosition.h"
#include "Graphics/RenderScene.h"

namespace apex::game {

struct CameraView {
    core::WorldPosition position{0.0f};
    // Camera to world. Identity looks towards -Z with +Y up.
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    float verticalFovRadians = glm::radians(70.0f);
    float nearPlane = 0.1f;
    float farPlane = 500.0f;
};

struct ObjectTransform {
    core::WorldPosition position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

// The extraction (G-07): the only place where world positions become render space, which is world
// space translated so that the camera sits at the origin. Moves to World in phase 5.
[[nodiscard]] graphics::RenderCamera makeRenderCamera(const CameraView& camera) noexcept;
[[nodiscard]] glm::mat4 modelRelativeToCamera(const ObjectTransform& transform,
                                              const core::WorldPosition& cameraPosition) noexcept;

} // namespace apex::game
