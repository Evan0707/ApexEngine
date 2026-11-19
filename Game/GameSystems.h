#pragma once

#include "FreeCamera.h"
#include "GameComponents.h"

#include "Core/Error.h"

#include <entt/entity/registry.hpp>

#include <string_view>

namespace apex::game {

// Scene ids the demo gives a behavior to (decision 0017).
inline constexpr std::string_view kCameraId = "camera";
inline constexpr std::string_view kSpinnerId = "spinner";
inline constexpr float kSpinnerDegreesPerSecond = 45.0f;

// Attaches the demo's components after a scene is loaded: the entity "camera" gets a free camera
// controller starting at the entity's orientation, the entity "spinner" (optional) a Spin. Returns the
// camera entity; fails when the scene has no entity "camera" with a Transform and a Camera.
[[nodiscard]] core::Result<entt::entity> attachGameComponents(entt::registry& registry);

// One fixed step of the free camera (G-05): turns and moves the entity's Transform.
void stepFreeCameraEntity(entt::registry& registry, entt::entity camera, const FreeCameraInput& input,
                          double deltaSeconds, const FreeCameraSettings& settings);

// Puts the camera entity at a given state, as a step would: the benchmark's fixed path.
void placeFreeCameraEntity(entt::registry& registry, entt::entity camera, const FreeCameraState& state);

// One fixed step of every Spin.
void stepSpinners(entt::registry& registry, double deltaSeconds);

} // namespace apex::game
