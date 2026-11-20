#include "World/ColliderLines.h"

#include "Core/Profiling.h"
#include "World/Components.h"
#include "World/RenderExtraction.h"

#include <array>
#include <cmath>
#include <optional>

namespace apex::world {

namespace {

void addLine(const glm::mat4& pose, const glm::vec3& start, const glm::vec3& end, const glm::vec4& color,
             std::vector<graphics::RenderLine>& lines) {
    lines.push_back(graphics::RenderLine{.start = glm::vec3(pose * glm::vec4(start, 1.0f)),
                                         .end = glm::vec3(pose * glm::vec4(end, 1.0f)),
                                         .color = color});
}

// An arc of `segments` segments from angle `from`, around `center`, in the plane of the unit vectors a and b.
void addArc(const glm::mat4& pose, const glm::vec3& center, const glm::vec3& a, const glm::vec3& b,
            float radius, float from, float sweep, int segments, const glm::vec4& color,
            std::vector<graphics::RenderLine>& lines) {
    const auto at = [&](int i) {
        const float angle = from + sweep * static_cast<float>(i) / static_cast<float>(segments);
        return center + radius * (std::cos(angle) * a + std::sin(angle) * b);
    };
    for (int i = 0; i < segments; ++i) {
        addLine(pose, at(i), at(i + 1), color, lines);
    }
}

void addBox(const glm::vec3& half, const glm::mat4& pose, const glm::vec4& color,
            std::vector<graphics::RenderLine>& lines) {
    const auto corner = [&half](int index) {
        return glm::vec3((index & 1) != 0 ? half.x : -half.x, (index & 2) != 0 ? half.y : -half.y,
                         (index & 4) != 0 ? half.z : -half.z);
    };
    // Two corners share an edge when they differ by one coordinate.
    for (int from = 0; from < 8; ++from) {
        for (const int axis : {1, 2, 4}) {
            if ((from & axis) == 0) {
                addLine(pose, corner(from), corner(from | axis), color, lines);
            }
        }
    }
}

} // namespace

void appendShapeLines(const physics::Shape& shape, const glm::mat4& pose, const glm::vec4& color,
                      std::vector<graphics::RenderLine>& lines) {
    const glm::vec3 x{1.0f, 0.0f, 0.0f};
    const glm::vec3 y{0.0f, 1.0f, 0.0f};
    const glm::vec3 z{0.0f, 0.0f, 1.0f};
    const float full = glm::two_pi<float>();
    switch (shape.type) {
    case physics::ShapeType::Box:
        addBox(shape.halfExtents, pose, color, lines);
        break;
    case physics::ShapeType::Sphere:
        addArc(pose, glm::vec3(0.0f), x, y, shape.radius, 0.0f, full, kColliderCircleSegments, color, lines);
        addArc(pose, glm::vec3(0.0f), y, z, shape.radius, 0.0f, full, kColliderCircleSegments, color, lines);
        addArc(pose, glm::vec3(0.0f), z, x, shape.radius, 0.0f, full, kColliderCircleSegments, color, lines);
        break;
    case physics::ShapeType::Capsule: {
        const glm::vec3 top = y * shape.halfHeight;
        const glm::vec3 bottom = -top;
        const int half = kColliderCircleSegments / 2;
        for (const glm::vec3& end : {top, bottom}) {
            addArc(pose, end, z, x, shape.radius, 0.0f, full, kColliderCircleSegments, color, lines);
        }
        for (const glm::vec3& side : {x, -x, z, -z}) {
            addLine(pose, bottom + side * shape.radius, top + side * shape.radius, color, lines);
        }
        // Over the top from +X (or +Z) to the other side, and under the bottom.
        addArc(pose, top, x, y, shape.radius, 0.0f, glm::pi<float>(), half, color, lines);
        addArc(pose, top, z, y, shape.radius, 0.0f, glm::pi<float>(), half, color, lines);
        addArc(pose, bottom, x, -y, shape.radius, 0.0f, glm::pi<float>(), half, color, lines);
        addArc(pose, bottom, z, -y, shape.radius, 0.0f, glm::pi<float>(), half, color, lines);
        break;
    }
    }
}

void extractColliderLines(const entt::registry& registry, const physics::PhysicsWorld& physics,
                          entt::entity camera, float alpha, graphics::RenderScene& scene) {
    APEX_PROFILE_SCOPE("World::extractColliderLines");
    scene.lines.clear();
    core::WorldPosition cameraPosition{0.0f};
    if (registry.valid(camera) && registry.all_of<Transform, Camera>(camera)) {
        cameraPosition = drawnTransform(registry, camera, alpha).position;
    }
    // Rotation and translation relative to the camera; the subtraction comes first (G-07).
    const auto poseOf = [&](const core::WorldPosition& position, const glm::quat& rotation) {
        return glm::translate(glm::mat4(1.0f), position - cameraPosition) * glm::mat4_cast(rotation);
    };

    for (const auto [entity, body] : registry.view<PhysicsBody>().each()) {
        const std::optional<physics::Shape> shape = physics.shape(body.handle);
        if (!shape || !registry.all_of<Transform>(entity)) {
            continue;
        }
        const RigidBody* rigidBody = registry.try_get<RigidBody>(entity);
        const bool dynamic = rigidBody != nullptr && rigidBody->type == BodyType::Dynamic;
        const glm::vec4 color =
            !dynamic ? kStaticColliderColor
                     : (physics.isActive(body.handle) ? kAwakeColliderColor : kAsleepColliderColor);
        const Transform pose = drawnTransform(registry, entity, alpha);
        appendShapeLines(*shape, poseOf(pose.position, pose.rotation), color, scene.lines);
    }
    for (const auto [entity, character] : registry.view<PhysicsCharacter>().each()) {
        const std::optional<physics::Shape> shape = physics.characterShape(character.handle);
        if (!shape || !registry.all_of<Transform>(entity)) {
            continue;
        }
        // The capsule stands on the character's position, upright whatever way it faces.
        const core::WorldPosition feet = drawnTransform(registry, entity, alpha).position;
        const glm::vec3 lift{0.0f, shape->halfHeight + shape->radius, 0.0f};
        appendShapeLines(*shape, poseOf(feet + lift, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
                         kCharacterColliderColor, scene.lines);
    }
}

} // namespace apex::world
