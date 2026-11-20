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

    // Advances the simulation by one fixed step: the characters first, then the bodies. deltaSeconds must be
    // positive.
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

    // The closest hit along `direction` (any length, not zero) within maxDistance meters; empty when nothing
    // is hit or when an argument is not usable. A ray that starts inside a body hits it at distance zero.
    [[nodiscard]] std::optional<CastHit> raycast(const core::WorldPosition& origin,
                                                 const glm::vec3& direction, float maxDistance,
                                                 const QueryFilter& filter = {}) const;
    // The same with a sphere of `radius` meters whose center starts at `origin`.
    [[nodiscard]] std::optional<CastHit> spherecast(const core::WorldPosition& origin, float radius,
                                                    const glm::vec3& direction, float maxDistance,
                                                    const QueryFilter& filter = {}) const;

    // Fails on an invalid description (InvalidArgument).
    [[nodiscard]] core::Result<CharacterHandle> createCharacter(const CharacterDesc& desc);
    void destroyCharacter(CharacterHandle character);
    [[nodiscard]] bool contains(CharacterHandle character) const;
    [[nodiscard]] std::uint32_t characterCount() const;

    // The velocity the character tries to move with during the next steps, until set again. Physics adds
    // neither gravity nor a jump: the caller builds the whole vector, each step, from characterState():
    //  - on the ground: ground velocity + wanted horizontal velocity + jump speed + gravity * step;
    //  - otherwise: wanted horizontal velocity + current vertical velocity + gravity * step.
    void setCharacterVelocity(CharacterHandle character, const glm::vec3& velocity);
    // Teleports the feet, without a sweep.
    void setCharacterPosition(CharacterHandle character, const core::WorldPosition& position);
    // Empty for a stale handle.
    [[nodiscard]] std::optional<CharacterState> characterState(CharacterHandle character) const;

private:
    std::unique_ptr<State> m_state;
};

} // namespace apex::physics
