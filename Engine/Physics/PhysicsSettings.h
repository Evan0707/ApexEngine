#pragma once

#include "Core/Math.h"

#include <cstdint>

namespace apex::physics {

struct PhysicsSettings {
    // Meters per second squared (spec 5.7).
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    // Fixed capacities: creating a body beyond maxBodies fails.
    std::uint32_t maxBodies = 1024;
    std::uint32_t maxBodyPairs = 4096;
    std::uint32_t maxContactConstraints = 2048;
};

} // namespace apex::physics
