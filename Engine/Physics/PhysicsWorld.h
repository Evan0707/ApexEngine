#pragma once

#include "Core/Error.h"
#include "Core/Math.h"
#include "Physics/PhysicsSettings.h"
#include "Physics/Runtime.h"

#include <cstdint>
#include <memory>

namespace apex::physics {

// The simulated world (spec 5.7): rigid bodies, queries and characters, advanced by fixed steps (G-05).
// Knows neither entities nor EnTT: World copies data in and out (spec 4.3). No Jolt type appears here.
class PhysicsWorld final {
    struct State;
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    // runtime must outlive the world.
    [[nodiscard]] static core::Result<std::unique_ptr<PhysicsWorld>> create(const Runtime& runtime,
                                                                            const PhysicsSettings& settings);

    PhysicsWorld(ConstructionKey key, std::unique_ptr<State> state) noexcept;
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Advances the simulation by one fixed step. deltaSeconds must be positive.
    void step(double deltaSeconds);

    [[nodiscard]] glm::vec3 gravity() const;
    [[nodiscard]] std::uint32_t bodyCount() const;

private:
    std::unique_ptr<State> m_state;
};

} // namespace apex::physics
