#pragma once

#include "FreeCamera.h"
#include "GameComponents.h"

#include "Core/Error.h"
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

// One fixed step of every Spin.
void stepSpinners(entt::registry& registry, double deltaSeconds);

} // namespace apex::game
