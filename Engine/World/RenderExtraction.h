#pragma once

#include "Graphics/RenderScene.h"
#include "Resources/ResourceCache.h"
#include "World/Components.h"

#include <entt/entity/registry.hpp>

namespace apex::world {

// Copies every Transform into PreviousTransform. Called at the start of each fixed step, before anything
// moves: the extraction then interpolates between the last two simulation states (spec 5.1).
void rememberTransforms(entt::registry& registry);

// alpha in [0, 1]: 0 gives previous, 1 gives current. Rotations take the shortest way.
[[nodiscard]] Transform interpolateTransform(const Transform& previous, const Transform& current,
                                             float alpha) noexcept;

// The extraction (spec 4.3, G-01): fills the scene with copies of what the registry holds, one object
// per part of each MeshRenderer's model. It is the only place where world positions become render space,
// relative to the camera (G-07). Transforms are interpolated with alpha where a PreviousTransform exists.
// camera: the entity seen through, with a Transform and a Camera; without one, the view is from the world
// origin with the default Camera. The first DirectionalLight found lights the scene (spec 5.5); without
// one, only the ambient term does.
void extractRenderScene(const entt::registry& registry, const resources::ResourceCache& cache,
                        entt::entity camera, float alpha, graphics::RenderScene& scene);

} // namespace apex::world
