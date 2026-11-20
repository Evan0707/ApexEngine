#pragma once

#include "Physics/PhysicsWorld.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>

namespace apex::tests {

inline constexpr double kPhysicsStep = 1.0 / 60.0;

// A Jolt runtime and one world, for the length of a test case.
struct PhysicsTestWorld {
    explicit PhysicsTestWorld(const physics::PhysicsSettings& settings = {})
        : runtime(requireValue(physics::Runtime::create())),
          world(requireValue(physics::PhysicsWorld::create(runtime, settings))) {}

    physics::PhysicsWorld& operator*() const noexcept { return *world; }
    physics::PhysicsWorld* operator->() const noexcept { return world.get(); }

    void stepSeconds(double seconds) const {
        const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
        for (int i = 0; i < steps; ++i) {
            world->step(kPhysicsStep);
        }
    }

    // A static box whose top face is at y = 0.
    [[nodiscard]] physics::BodyHandle addFloor(float halfSize = 50.0f, float thickness = 1.0f) const {
        return requireValue(world->createBody({
            .shape = {.type = physics::ShapeType::Box, .halfExtents = {halfSize, 0.5f * thickness, halfSize}},
            .motion = physics::MotionType::Static,
            .transform = {.position = {0.0f, -0.5f * thickness, 0.0f}},
        }));
    }

    [[nodiscard]] physics::BodyHandle addBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                             physics::MotionType motion, float mass = 20.0f) const {
        return requireValue(world->createBody({
            .shape = {.type = physics::ShapeType::Box, .halfExtents = halfExtents},
            .motion = motion,
            .transform = {.position = position},
            .mass = mass,
        }));
    }

    [[nodiscard]] glm::vec3 position(physics::BodyHandle body) const {
        const auto transform = world->transform(body);
        REQUIRE(transform.has_value());
        return transform->position;
    }

    physics::Runtime runtime;
    std::unique_ptr<physics::PhysicsWorld> world;

private:
    template <typename T>
    [[nodiscard]] static T requireValue(core::Result<T> result) {
        REQUIRE(result.has_value());
        return std::move(*result);
    }
};

} // namespace apex::tests
