#pragma once

#include "PlayerMotion.h"

namespace apex::game {

// The demo's own components (spec 5.6): attached after the scene is loaded, to the entities the demo
// knows by their scene id (decision 0017).

// Drives the Transform of the entity it is on from the move and look actions. Radians; yaw is not
// wrapped. The entity's rotation is rebuilt from these two angles at every step: no roll.
struct FreeCameraController {
    float yaw = 0.0f;
    float pitch = 0.0f;
};

// The demo's character (spec 5.8): turns the move, sprint and jump actions into the entity's
// world::CharacterMotion and into its facing. The collisions are Physics's.
struct PlayerController {
    PlayerState state;
};

// Turns the entity around the world's +Y by fixed steps: shows the interpolation of spec 5.1.
struct Spin {
    float radiansPerSecond = 0.0f;
};

} // namespace apex::game
