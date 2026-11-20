#include "Physics/PhysicsWorldState.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>

namespace apex::physics {

namespace {

[[nodiscard]] bool isPositive(float value) noexcept {
    return std::isfinite(value) && value > 0.0f;
}

[[nodiscard]] bool isFinite(const glm::vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] std::optional<std::string> validate(const BodyDesc& desc) {
    const Shape& shape = desc.shape;
    switch (shape.type) {
    case ShapeType::Box:
        if (!isPositive(shape.halfExtents.x) || !isPositive(shape.halfExtents.y) ||
            !isPositive(shape.halfExtents.z)) {
            return "the half extents of a box must be positive";
        }
        break;
    case ShapeType::Sphere:
        if (!isPositive(shape.radius)) {
            return "the radius of a sphere must be positive";
        }
        break;
    case ShapeType::Capsule:
        if (!isPositive(shape.radius) || !isPositive(shape.halfHeight)) {
            return "the radius and the half height of a capsule must be positive";
        }
        break;
    }
    if (desc.motion == MotionType::Dynamic && !isPositive(desc.mass)) {
        return "the mass of a dynamic body must be positive";
    }
    if (!std::isfinite(desc.friction) || desc.friction < 0.0f || !std::isfinite(desc.restitution) ||
        desc.restitution < 0.0f) {
        return "friction and restitution must not be negative";
    }
    const glm::quat& rotation = desc.transform.rotation;
    const float rotationLength = glm::length(rotation);
    if (!isFinite(desc.transform.position) || !std::isfinite(rotationLength) || rotationLength < 1e-6f) {
        return "the position and the rotation must be finite";
    }
    return std::nullopt;
}

[[nodiscard]] JPH::ShapeSettings::ShapeResult createShape(const Shape& shape) {
    switch (shape.type) {
    case ShapeType::Sphere: {
        JPH::SphereShapeSettings settings(shape.radius);
        settings.SetEmbedded();
        return settings.Create();
    }
    case ShapeType::Capsule: {
        JPH::CapsuleShapeSettings settings(shape.halfHeight, shape.radius);
        settings.SetEmbedded();
        return settings.Create();
    }
    case ShapeType::Box:
        break;
    }
    // Jolt rounds the edges of a box; the rounding must fit inside a thin one.
    const float smallest = std::min({shape.halfExtents.x, shape.halfExtents.y, shape.halfExtents.z});
    JPH::BoxShapeSettings settings(toJolt(shape.halfExtents),
                                   std::min(JPH::cDefaultConvexRadius, 0.5f * smallest));
    settings.SetEmbedded();
    return settings.Create();
}

// False for an invalid or stale handle.
[[nodiscard]] bool isAlive(const JPH::PhysicsSystem& system, JPH::BodyID id) {
    if (id.IsInvalid()) {
        return false;
    }
    const JPH::BodyLockRead lock(system.GetBodyLockInterface(), id);
    return lock.Succeeded();
}

} // namespace

core::Result<BodyHandle> PhysicsWorld::createBody(const BodyDesc& desc) {
    if (const std::optional<std::string> problem = validate(desc)) {
        return core::makeError(core::ErrorCode::InvalidArgument, std::format("Invalid body: {}", *problem));
    }
    const JPH::ShapeSettings::ShapeResult shape = createShape(desc.shape);
    if (shape.HasError()) {
        return core::makeError(core::ErrorCode::PhysicsFailure,
                               std::format("Jolt refused the shape: {}", shape.GetError().c_str()));
    }

    const bool dynamic = desc.motion == MotionType::Dynamic;
    JPH::BodyCreationSettings settings(shape.Get(), toJolt(desc.transform.position),
                                       toJolt(glm::normalize(desc.transform.rotation)),
                                       dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                       dynamic ? kDynamicLayer : kStaticLayer);
    settings.mFriction = desc.friction;
    settings.mRestitution = desc.restitution;
    if (dynamic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass;
        // Swept: a fast or small body does not tunnel through thin geometry (AC-08).
        settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    }

    const JPH::BodyID id =
        m_state->system.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (id.IsInvalid()) {
        return core::makeError(core::ErrorCode::PhysicsFailure,
                               std::format("The physics world is full: {} bodies", bodyCount()));
    }
    return BodyHandle{id.GetIndexAndSequenceNumber()};
}

void PhysicsWorld::destroyBody(BodyHandle body) {
    const JPH::BodyID id(body.value);
    if (!isAlive(m_state->system, id)) {
        return;
    }
    JPH::BodyInterface& bodies = m_state->system.GetBodyInterface();
    bodies.RemoveBody(id);
    bodies.DestroyBody(id);
}

bool PhysicsWorld::contains(BodyHandle body) const {
    return isAlive(m_state->system, JPH::BodyID(body.value));
}

std::optional<BodyTransform> PhysicsWorld::transform(BodyHandle body) const {
    const JPH::BodyID id(body.value);
    if (!isAlive(m_state->system, id)) {
        return std::nullopt;
    }
    JPH::RVec3 position;
    JPH::Quat rotation;
    m_state->system.GetBodyInterface().GetPositionAndRotation(id, position, rotation);
    return BodyTransform{.position = toGlm(position), .rotation = toGlm(rotation)};
}

void PhysicsWorld::setTransform(BodyHandle body, const BodyTransform& transform) {
    const JPH::BodyID id(body.value);
    if (!isAlive(m_state->system, id)) {
        return;
    }
    m_state->system.GetBodyInterface().SetPositionAndRotation(id, toJolt(transform.position),
                                                              toJolt(glm::normalize(transform.rotation)),
                                                              JPH::EActivation::Activate);
}

glm::vec3 PhysicsWorld::linearVelocity(BodyHandle body) const {
    const JPH::BodyID id(body.value);
    if (!isAlive(m_state->system, id)) {
        return glm::vec3(0.0f);
    }
    return toGlm(m_state->system.GetBodyInterface().GetLinearVelocity(id));
}

void PhysicsWorld::setLinearVelocity(BodyHandle body, const glm::vec3& velocity) {
    const JPH::BodyID id(body.value);
    if (!isAlive(m_state->system, id)) {
        return;
    }
    JPH::BodyInterface& bodies = m_state->system.GetBodyInterface();
    if (bodies.GetMotionType(id) != JPH::EMotionType::Dynamic) {
        return;
    }
    bodies.SetLinearVelocity(id, toJolt(velocity));
    bodies.ActivateBody(id);
}

void PhysicsWorld::applyImpulse(BodyHandle body, const glm::vec3& impulse) {
    const JPH::BodyID id(body.value);
    if (!isAlive(m_state->system, id)) {
        return;
    }
    JPH::BodyInterface& bodies = m_state->system.GetBodyInterface();
    if (bodies.GetMotionType(id) != JPH::EMotionType::Dynamic) {
        return;
    }
    bodies.ActivateBody(id);
    bodies.AddImpulse(id, toJolt(impulse));
}

bool PhysicsWorld::isActive(BodyHandle body) const {
    const JPH::BodyID id(body.value);
    return isAlive(m_state->system, id) && m_state->system.GetBodyInterface().IsActive(id);
}

std::uint32_t PhysicsWorld::activeBodyCount() const {
    return m_state->system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
}

} // namespace apex::physics
