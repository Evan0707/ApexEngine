#include "World/PhysicsSync.h"

#include "Core/Profiling.h"

#include <cmath>
#include <format>
#include <string>
#include <vector>

namespace apex::world {

namespace {

// Relative difference under which two scale factors count as equal.
constexpr float kScaleTolerance = 1e-4f;

[[nodiscard]] bool sameScale(float a, float b) noexcept {
    return std::abs(a - b) <= kScaleTolerance * std::max(std::abs(a), std::abs(b));
}

// "'crate_01'" for a scene entity, its number otherwise.
[[nodiscard]] std::string entityName(const entt::registry& registry, entt::entity entity) {
    if (const SceneId* id = registry.try_get<SceneId>(entity)) {
        return std::format("'{}'", id->value);
    }
    return std::format("#{}", entt::to_integral(entity));
}

[[nodiscard]] std::unexpected<core::Error> entityError(const entt::registry& registry, entt::entity entity,
                                                       const core::Error& cause) {
    return core::makeError(cause.code,
                           std::format("Entity {}: {}", entityName(registry, entity), cause.message));
}

[[nodiscard]] core::Result<void> createBodies(entt::registry& registry, physics::PhysicsWorld& physics) {
    // Collected first: attaching a component while iterating the same view is not allowed.
    std::vector<entt::entity> pending;
    for (const entt::entity entity :
         registry.view<const Transform, const Collider>(entt::exclude<PhysicsBody>)) {
        pending.push_back(entity);
    }
    for (const entt::entity entity : pending) {
        const Transform& transform = registry.get<Transform>(entity);
        const core::Result<physics::Shape> shape =
            scaledShape(registry.get<Collider>(entity), transform.scale);
        if (!shape) {
            return entityError(registry, entity, shape.error());
        }
        physics::BodyDesc desc{.shape = *shape,
                               .transform = {.position = transform.position, .rotation = transform.rotation}};
        if (const RigidBody* body = registry.try_get<RigidBody>(entity);
            body != nullptr && body->type == BodyType::Dynamic) {
            desc.motion = physics::MotionType::Dynamic;
            desc.mass = body->mass;
        }
        const core::Result<physics::BodyHandle> handle = physics.createBody(desc);
        if (!handle) {
            return entityError(registry, entity, handle.error());
        }
        registry.emplace<PhysicsBody>(entity, *handle);
    }
    return {};
}

[[nodiscard]] core::Result<void> createCharacters(entt::registry& registry, physics::PhysicsWorld& physics) {
    std::vector<entt::entity> pending;
    for (const entt::entity entity :
         registry.view<const Transform, const CharacterBody>(entt::exclude<PhysicsCharacter>)) {
        pending.push_back(entity);
    }
    for (const entt::entity entity : pending) {
        const CharacterBody& body = registry.get<CharacterBody>(entity);
        const core::Result<physics::CharacterHandle> handle =
            physics.createCharacter({.position = registry.get<Transform>(entity).position,
                                     .height = body.height,
                                     .radius = body.radius,
                                     .maxSlopeRadians = body.maxSlopeRadians,
                                     .stepHeight = body.stepHeight,
                                     .mass = body.mass,
                                     .maxPushForce = body.maxPushForce});
        if (!handle) {
            return entityError(registry, entity, handle.error());
        }
        registry.emplace<PhysicsCharacter>(entity, *handle);
        (void)registry.get_or_emplace<CharacterMotion>(entity);
        (void)registry.get_or_emplace<CharacterGround>(entity);
    }
    return {};
}

} // namespace

core::Result<physics::Shape> scaledShape(const Collider& collider, const glm::vec3& scale) {
    const glm::vec3 size = glm::abs(scale);
    if (!(size.x > 0.0f) || !(size.y > 0.0f) || !(size.z > 0.0f) ||
        !std::isfinite(size.x + size.y + size.z)) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               "a collider needs a scale that is finite and not zero on every axis");
    }
    switch (collider.shape) {
    case ColliderShape::Sphere:
        if (!sameScale(size.x, size.y) || !sameScale(size.x, size.z)) {
            return core::makeError(core::ErrorCode::InvalidArgument,
                                   "a sphere collider needs the same scale on the three axes");
        }
        return physics::Shape{.type = physics::ShapeType::Sphere, .radius = collider.radius * size.x};
    case ColliderShape::Capsule:
        if (!sameScale(size.x, size.z)) {
            return core::makeError(core::ErrorCode::InvalidArgument,
                                   "a capsule collider needs the same scale on X and Z");
        }
        return physics::Shape{.type = physics::ShapeType::Capsule,
                              .radius = collider.radius * size.x,
                              .halfHeight = collider.halfHeight * size.y};
    case ColliderShape::Box:
        break;
    }
    return physics::Shape{.type = physics::ShapeType::Box, .halfExtents = collider.halfExtents * size};
}

core::Result<void> createPhysicsObjects(entt::registry& registry, physics::PhysicsWorld& physics) {
    if (core::Result<void> bodies = createBodies(registry, physics); !bodies) {
        return bodies;
    }
    return createCharacters(registry, physics);
}

void destroyPhysicsObjects(entt::registry& registry, physics::PhysicsWorld& physics) {
    for (const auto [entity, body] : registry.view<const PhysicsBody>().each()) {
        physics.destroyBody(body.handle);
    }
    for (const auto [entity, character] : registry.view<const PhysicsCharacter>().each()) {
        physics.destroyCharacter(character.handle);
    }
    registry.clear<PhysicsBody>();
    registry.clear<PhysicsCharacter>();
}

void stepPhysics(entt::registry& registry, physics::PhysicsWorld& physics, double deltaSeconds) {
    APEX_PROFILE_SCOPE("World physics");
    for (const auto [entity, character, motion] :
         registry.view<const PhysicsCharacter, const CharacterMotion>().each()) {
        physics.setCharacterVelocity(character.handle, motion.velocity);
    }

    physics.step(deltaSeconds);

    for (const auto [entity, body, rigidBody, transform] :
         registry.view<const PhysicsBody, const RigidBody, Transform>().each()) {
        if (rigidBody.type != BodyType::Dynamic) {
            continue;
        }
        if (const auto moved = physics.transform(body.handle)) {
            transform.position = moved->position;
            transform.rotation = moved->rotation;
        }
    }
    for (const auto [entity, character, transform, ground] :
         registry.view<const PhysicsCharacter, Transform, CharacterGround>().each()) {
        if (const auto state = physics.characterState(character.handle)) {
            // The rotation stays the game's: a capsule has no facing for Physics.
            transform.position = state->position;
            ground.state = state->ground;
            ground.velocity = state->velocity;
            ground.normal = state->groundNormal;
            ground.groundVelocity = state->groundVelocity;
        }
    }
}

} // namespace apex::world
