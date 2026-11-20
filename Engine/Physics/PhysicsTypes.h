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

} // namespace apex::physics
