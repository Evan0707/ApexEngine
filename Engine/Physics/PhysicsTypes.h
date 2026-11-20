#pragma once

#include "Core/Math.h"
#include "Core/WorldPosition.h"

#include <cstdint>

namespace apex::physics {

// Names a body of one PhysicsWorld. A handle whose body was destroyed is stale: every function accepts it
// and does nothing. A slot reused 256 times can make an old handle name the new body.
struct BodyHandle {
    static constexpr std::uint32_t kInvalid = 0xffffffffu;

    std::uint32_t value = kInvalid;

    [[nodiscard]] constexpr bool valid() const noexcept { return value != kInvalid; }
    [[nodiscard]] bool operator==(const BodyHandle&) const noexcept = default;
};

enum class ShapeType { Box, Sphere, Capsule };

// Meters, centered on the body's position (spec 5.7).
struct Shape {
    ShapeType type = ShapeType::Box;
    // Box.
    glm::vec3 halfExtents{0.5f};
    // Sphere and capsule.
    float radius = 0.5f;
    // Capsule: half the distance between the centers of its two end spheres, along the body's Y.
    float halfHeight = 0.5f;
};

// Static: never moves, in the static layer. Dynamic: moved by forces and contacts, in the dynamic layer.
enum class MotionType { Static, Dynamic };

struct BodyTransform {
    core::WorldPosition position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct BodyDesc {
    Shape shape;
    MotionType motion = MotionType::Static;
    BodyTransform transform;
    // Kilograms. Only read for a dynamic body.
    float mass = 1.0f;
    // Dimensionless, 0 to 1; two bodies in contact use the geometric mean of theirs.
    float friction = 0.5f;
    // 0: no bounce.
    float restitution = 0.0f;
};

// Which bodies a query can hit (spec 5.7). A character is not a body: queries never hit it.
struct QueryFilter {
    bool staticBodies = true;
    bool dynamicBodies = true;
};

// The first thing a ray or a swept sphere touches.
struct CastHit {
    BodyHandle body;
    // Where the query stopped: the point hit for a ray, the center of the sphere for a swept sphere.
    core::WorldPosition position{0.0f};
    // The point touched on the body.
    core::WorldPosition contact{0.0f};
    // Unit vector pointing out of the body, toward where the query came from.
    glm::vec3 normal{0.0f};
    // Meters travelled from the origin. Zero when a swept sphere starts inside a body.
    float distance = 0.0f;
};

// Names a character of one PhysicsWorld. Stale after destroyCharacter: every function then ignores it.
struct CharacterHandle {
    static constexpr std::uint32_t kInvalid = 0xffffffffu;

    std::uint32_t index = kInvalid;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept { return index != kInvalid; }
    [[nodiscard]] bool operator==(const CharacterHandle&) const noexcept = default;
};

// An upright capsule moved by Jolt's CharacterVirtual (spec 5.8). The defaults are those of the spec.
struct CharacterDesc {
    // Where the feet are: the bottom of the capsule.
    core::WorldPosition position{0.0f};
    // Meters. The height is the whole capsule's and must be more than twice the radius.
    float height = 1.8f;
    float radius = 0.3f;
    // Steepest ground the character can stand on and climb.
    float maxSlopeRadians = glm::radians(45.0f);
    // Tallest step it walks up, in meters.
    float stepHeight = 0.3f;
    // Kilograms: its weight on what it stands on.
    float mass = 70.0f;
    // Newtons: the most it pushes on a dynamic body it walks into.
    float maxPushForce = 100.0f;
};

enum class GroundState {
    // Standing: can walk and jump.
    OnGround,
    // On a slope steeper than maxSlopeRadians: cannot climb, should slide down.
    OnSteepGround,
    InAir,
};

// A character after the last step.
struct CharacterState {
    // Feet.
    core::WorldPosition position{0.0f};
    // Meters per second, as last set: collisions do not change it, the position tells what really moved.
    glm::vec3 velocity{0.0f};
    GroundState ground = GroundState::InAir;
    // Unit vector; zero in the air.
    glm::vec3 groundNormal{0.0f};
    // Velocity of what the character stands on; zero on static ground.
    glm::vec3 groundVelocity{0.0f};
};

} // namespace apex::physics
