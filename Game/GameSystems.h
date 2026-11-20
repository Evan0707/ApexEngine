#pragma once

#include "FreeCamera.h"
#include "GameComponents.h"

#include "Core/Error.h"
#include "Physics/PhysicsWorld.h"
#include "World/Components.h"

#include <entt/entity/registry.hpp>

#include <string_view>

namespace apex::game {

// Scene ids the demo gives a behavior to (decision 0017).
inline constexpr std::string_view kCameraId = "camera";
inline constexpr std::string_view kSpinnerId = "spinner";
inline constexpr std::string_view kPlayerId = "player";
inline constexpr float kSpinnerDegreesPerSecond = 45.0f;

// The entities the demo drives. `player` is entt::null in a scene without a character.
struct GameEntities {
    entt::entity camera = entt::null;
    entt::entity player = entt::null;
};

// Attaches the demo's components after a scene is loaded, before its physics objects are created:
//  - "camera": a free camera controller starting at the entity's orientation;
//  - "spinner" (optional): a Spin;
//  - "player" (optional): the character body given here and a PlayerController facing where the entity does.
// Fails when the scene has no entity "camera" with a Transform and a Camera.
[[nodiscard]] core::Result<GameEntities> attachGameComponents(entt::registry& registry,
                                                              const world::CharacterBody& characterBody);

// One fixed step of the free camera (G-05): turns and moves the entity's Transform.
void stepFreeCameraEntity(entt::registry& registry, entt::entity camera, const FreeCameraInput& input,
                          double deltaSeconds, const FreeCameraSettings& settings);

// Puts the camera entity at a given state, as a step would: the benchmark's fixed path.
void placeFreeCameraEntity(entt::registry& registry, entt::entity camera, const FreeCameraState& state);

// One fixed step of the character (G-05), before world::stepPhysics: reads its CharacterGround, writes its
// CharacterMotion and turns its Transform. `player` must have been through attachGameComponents and
// world::createPhysicsObjects.
void stepPlayerEntity(entt::registry& registry, entt::entity player, const PlayerInput& input,
                      const glm::vec3& gravity, double deltaSeconds, const PlayerSettings& settings);

// Switches the camera entity to third person: it keeps looking the way it does, from behind `player`.
void startOrbitCameraEntity(entt::registry& registry, entt::entity camera, entt::entity player,
                            const OrbitCameraSettings& settings);

// One fixed step of the third-person camera (G-05), after world::stepPhysics: turns with the mouse,
// follows the character, and sweeps a sphere through `physics` so that nothing static or dynamic stands
// between the character and the camera (spec 5.3).
void stepOrbitCameraEntity(entt::registry& registry, entt::entity camera, entt::entity player,
                           const OrbitCameraInput& input, const physics::PhysicsWorld& physics,
                           double deltaSeconds, const OrbitCameraSettings& settings);

// Gives the camera entity back to its free camera controller, where it is and looking the same way.
void releaseOrbitCameraEntity(entt::registry& registry, entt::entity camera);

// One fixed step of every Spin.
void stepSpinners(entt::registry& registry, double deltaSeconds);

} // namespace apex::game
