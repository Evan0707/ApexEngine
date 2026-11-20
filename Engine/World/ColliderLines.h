#pragma once

#include "Core/Math.h"
#include "Graphics/RenderScene.h"
#include "Physics/PhysicsTypes.h"
#include "Physics/PhysicsWorld.h"

#include <entt/entity/registry.hpp>

#include <vector>

namespace apex::world {

// Colors of the collider display (decision 0023), linear RGB with alpha 1.
inline constexpr glm::vec4 kStaticColliderColor{0.35f, 0.75f, 0.45f, 1.0f};
inline constexpr glm::vec4 kAwakeColliderColor{1.0f, 0.55f, 0.1f, 1.0f};
inline constexpr glm::vec4 kAsleepColliderColor{0.25f, 0.5f, 1.0f, 1.0f};
inline constexpr glm::vec4 kCharacterColliderColor{1.0f, 0.9f, 0.1f, 1.0f};

// Segments per full circle of a sphere or a capsule.
inline constexpr int kColliderCircleSegments = 32;

// The wireframe of a shape centered on the origin of `pose` (rotation and translation, no scale: the shape is
// already scaled). Box: its 12 edges. Sphere: three circles. Capsule along Y: a circle around each end, four
// lines between them, and two arcs over each end.
void appendShapeLines(const physics::Shape& shape, const glm::mat4& pose, const glm::vec4& color,
                      std::vector<graphics::RenderLine>& lines);

// After extractRenderScene: replaces scene.lines with the wireframe of every physics body and character, in
// render space relative to the camera (G-07), at their interpolated pose. Shapes come from Physics: what Jolt
// simulates, not what the components ask for (AC-P1-2).
void extractColliderLines(const entt::registry& registry, const physics::PhysicsWorld& physics,
                          entt::entity camera, float alpha, graphics::RenderScene& scene);

} // namespace apex::world
