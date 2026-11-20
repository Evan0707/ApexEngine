#pragma once

#include "Core/Error.h"
#include "Physics/PhysicsTypes.h"
#include "Physics/PhysicsWorld.h"
#include "World/Components.h"

#include <entt/entity/registry.hpp>

namespace apex::world {

// The link between the registry and Physics (spec 4.3). Physics knows neither entities nor EnTT: these
// functions push the entities' data to it and copy its results back.

// The collider's dimensions multiplied by the entity's scale (decision 0019): Physics has no scale. A
// sphere needs a uniform scale, a capsule the same scale on X and Z; no component of the scale may be zero.
[[nodiscard]] core::Result<physics::Shape> scaledShape(const Collider& collider, const glm::vec3& scale);

// Creates what the entities describe and attaches the handles to them:
//  - Transform and Collider: a body. Static without a RigidBody, else of the RigidBody's type and mass;
//  - Transform and CharacterBody: a character whose feet are at the Transform's position.
// Entities that already have their handle are left alone, so this can be called again after adding
// entities. Stops at the first failure; the error names the entity.
[[nodiscard]] core::Result<void> createPhysicsObjects(entt::registry& registry,
                                                      physics::PhysicsWorld& physics);

// Destroys every body and character created above and removes the handles. To call before the registry is
// cleared when the physics world outlives the scene.
void destroyPhysicsObjects(entt::registry& registry, physics::PhysicsWorld& physics);

// One fixed step (G-05): gives each character its CharacterMotion, advances Physics, then writes the
// Transform of every dynamic body and of every character, and each character's CharacterGround.
void stepPhysics(entt::registry& registry, physics::PhysicsWorld& physics, double deltaSeconds);

} // namespace apex::world
