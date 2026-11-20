#include "Physics/PhysicsWorldState.h"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <cmath>
#include <optional>

namespace apex::physics {

namespace {

// Keeps the bodies of the layers the query asked for.
class QueryLayerFilter final : public JPH::ObjectLayerFilter {
public:
    explicit QueryLayerFilter(const QueryFilter& filter) noexcept : m_filter(filter) {}

    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer) const override {
        return (layer == kStaticLayer && m_filter.staticBodies) ||
               (layer == kDynamicLayer && m_filter.dynamicBodies);
    }

private:
    QueryFilter m_filter;
};

// The unit direction scaled to the distance; empty when the query cannot be made.
[[nodiscard]] std::optional<JPH::Vec3> sweep(const core::WorldPosition& origin, const glm::vec3& direction,
                                             float maxDistance) noexcept {
    const float length = glm::length(direction);
    const bool finite = std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z) &&
                        std::isfinite(length) && std::isfinite(maxDistance);
    if (!finite || length < 1e-6f || maxDistance <= 0.0f) {
        return std::nullopt;
    }
    return toJolt(direction * (maxDistance / length));
}

} // namespace

std::optional<CastHit> PhysicsWorld::raycast(const core::WorldPosition& origin, const glm::vec3& direction,
                                             float maxDistance, const QueryFilter& filter) const {
    const std::optional<JPH::Vec3> travel = sweep(origin, direction, maxDistance);
    if (!travel) {
        return std::nullopt;
    }
    const JPH::RRayCast ray(toJolt(origin), *travel);
    JPH::RayCastResult result;
    if (!m_state->system.GetNarrowPhaseQuery().CastRay(ray, result, {}, QueryLayerFilter(filter))) {
        return std::nullopt;
    }

    const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    CastHit hit{.body = BodyHandle{result.mBodyID.GetIndexAndSequenceNumber()},
                .position = toGlm(point),
                .contact = toGlm(point),
                .normal = glm::vec3(0.0f),
                .distance = result.mFraction * maxDistance};
    const JPH::BodyLockRead lock(m_state->system.GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        hit.normal = toGlm(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
    }
    return hit;
}

std::optional<CastHit> PhysicsWorld::spherecast(const core::WorldPosition& origin, float radius,
                                                const glm::vec3& direction, float maxDistance,
                                                const QueryFilter& filter) const {
    const std::optional<JPH::Vec3> travel = sweep(origin, direction, maxDistance);
    if (!travel || !std::isfinite(radius) || radius <= 0.0f) {
        return std::nullopt;
    }
    // On the stack: Jolt must not try to free it.
    JPH::SphereShape sphere(radius);
    sphere.SetEmbedded();
    const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
        &sphere, JPH::Vec3::sReplicate(1.0f), JPH::RMat44::sTranslation(toJolt(origin)), *travel);
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    m_state->system.GetNarrowPhaseQuery().CastShape(cast, JPH::ShapeCastSettings(), JPH::RVec3::sZero(),
                                                    collector, {}, QueryLayerFilter(filter));
    if (!collector.HadHit()) {
        return std::nullopt;
    }

    const JPH::ShapeCastResult& result = collector.mHit;
    // The penetration axis points from the sphere into the body.
    const JPH::Vec3 axis = result.mPenetrationAxis;
    return CastHit{.body = BodyHandle{result.mBodyID2.GetIndexAndSequenceNumber()},
                   .position = toGlm(toJolt(origin) + result.mFraction * *travel),
                   .contact = toGlm(result.mContactPointOn2),
                   .normal = axis.LengthSq() > 0.0f ? toGlm(-axis.Normalized()) : glm::vec3(0.0f),
                   .distance = result.mFraction * maxDistance};
}

} // namespace apex::physics
