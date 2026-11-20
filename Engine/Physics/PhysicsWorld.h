#pragma once

#include "Core/Error.h"
#include "Core/Math.h"
#include "Physics/PhysicsSettings.h"
#include "Physics/PhysicsTypes.h"
#include "Physics/Runtime.h"

#include <cstdint>
#include <memory>
#include <optional>

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
    // Bodies that are not asleep.
    [[nodiscard]] std::uint32_t activeBodyCount() const;

    // Fails on an invalid description (InvalidArgument) or when the world is full (PhysicsFailure).
    [[nodiscard]] core::Result<BodyHandle> createBody(const BodyDesc& desc);
    void destroyBody(BodyHandle body);
    [[nodiscard]] bool contains(BodyHandle body) const;

    // Empty for a stale handle.
    [[nodiscard]] std::optional<BodyTransform> transform(BodyHandle body) const;
    // Teleports: no sweep between the old and the new place. Wakes the body.
    void setTransform(BodyHandle body, const BodyTransform& transform);

    // Meters per second; zero for a static body or a stale handle.
    [[nodiscard]] glm::vec3 linearVelocity(BodyHandle body) const;
    // Setting a velocity or applying an impulse wakes a dynamic body and ignores a static one.
    void setLinearVelocity(BodyHandle body, const glm::vec3& velocity);
    // Newton seconds, at the center of mass.
    void applyImpulse(BodyHandle body, const glm::vec3& impulse);
    [[nodiscard]] bool isActive(BodyHandle body) const;

private:
    std::unique_ptr<State> m_state;
};

} // namespace apex::physics
