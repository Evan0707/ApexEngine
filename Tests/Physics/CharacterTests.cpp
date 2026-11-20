#include "PhysicsTestWorld.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>

using namespace apex;
using namespace apex::tests;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using physics::GroundState;
using physics::MotionType;

namespace {

// Spec 5.8.
constexpr float kRadius = 0.3f;
constexpr float kWalkSpeed = 4.0f;
constexpr float kRunSpeed = 7.0f;
// Jolt keeps the character 2 cm away from what it touches.
constexpr double kContactTolerance = 0.05;

[[nodiscard]] physics::CharacterHandle addCharacter(const PhysicsTestWorld& world, const glm::vec3& feet) {
    const auto character = world->createCharacter({.position = feet});
    REQUIRE(character.has_value());
    return *character;
}

[[nodiscard]] physics::CharacterState stateOf(const PhysicsTestWorld& world,
                                              physics::CharacterHandle character) {
    const auto state = world->characterState(character);
    REQUIRE(state.has_value());
    return *state;
}

struct Extremes {
    float highest = 0.0f;
    float farthestX = 0.0f;
};

// Moves the character as a game controller does (see PhysicsWorld::setCharacterVelocity): a horizontal
// velocity, gravity while in the air, and an optional jump on the first step.
Extremes drive(const PhysicsTestWorld& world, physics::CharacterHandle character, const glm::vec3& horizontal,
               double seconds, float jumpSpeed = 0.0f) {
    Extremes extremes{.highest = stateOf(world, character).position.y,
                      .farthestX = stateOf(world, character).position.x};
    const int steps = static_cast<int>(seconds / kPhysicsStep + 0.5);
    for (int i = 0; i < steps; ++i) {
        const physics::CharacterState state = stateOf(world, character);
        glm::vec3 velocity = horizontal;
        if (state.ground == GroundState::OnGround) {
            velocity += state.groundVelocity;
            velocity.y += i == 0 ? jumpSpeed : 0.0f;
        } else {
            velocity.y = state.velocity.y;
        }
        velocity += world->gravity() * static_cast<float>(kPhysicsStep);
        world->setCharacterVelocity(character, velocity);
        world->step(kPhysicsStep);
        const glm::vec3 position = stateOf(world, character).position;
        extremes.highest = std::max(extremes.highest, position.y);
        extremes.farthestX = std::max(extremes.farthestX, position.x);
    }
    return extremes;
}

// A static slab whose low end touches the floor at x = startX and which rises toward +X.
void addRamp(const PhysicsTestWorld& world, float startX, float degrees) {
    const float angle = glm::radians(degrees);
    const float halfLength = 6.0f;
    const float halfThickness = 0.1f;
    const glm::vec3 center{startX + halfLength * std::cos(angle) + halfThickness * std::sin(angle),
                           halfLength * std::sin(angle) - halfThickness * std::cos(angle), 0.0f};
    const auto ramp = world->createBody(
        {.shape = {.type = physics::ShapeType::Box, .halfExtents = {halfLength, halfThickness, 3.0f}},
         .motion = MotionType::Static,
         .transform = {.position = center, .rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f))}});
    REQUIRE(ramp.has_value());
}

} // namespace

TEST_CASE("A character stands on the floor at its feet position", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::CharacterHandle character = addCharacter(world, {1.0f, 0.0f, 2.0f});
    REQUIRE(world->contains(character));
    REQUIRE(world->characterCount() == 1);
    // A character is not a body.
    REQUIRE(world->bodyCount() == 1);

    (void)drive(world, character, glm::vec3(0.0f), 1.0);
    const physics::CharacterState state = stateOf(world, character);
    REQUIRE(state.ground == GroundState::OnGround);
    REQUIRE_THAT(state.position.x, WithinAbs(1.0, 1e-3));
    REQUIRE_THAT(state.position.y, WithinAbs(0.0, kContactTolerance));
    REQUIRE_THAT(state.position.z, WithinAbs(2.0, 1e-3));
    REQUIRE_THAT(state.groundNormal.y, WithinAbs(1.0, 1e-3));
}

TEST_CASE("A character created in the air falls and lands", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 3.0f, 0.0f});

    (void)drive(world, character, glm::vec3(0.0f), 0.2);
    REQUIRE(stateOf(world, character).ground == GroundState::InAir);
    REQUIRE(stateOf(world, character).position.y < 3.0f);
    REQUIRE(stateOf(world, character).velocity.y < -1.0f);

    (void)drive(world, character, glm::vec3(0.0f), 2.0);
    REQUIRE(stateOf(world, character).ground == GroundState::OnGround);
    REQUIRE_THAT(stateOf(world, character).position.y, WithinAbs(0.0, kContactTolerance));
}

TEST_CASE("A character walks at the velocity it is given", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});

    (void)drive(world, character, {kWalkSpeed, 0.0f, 0.0f}, 2.0);
    const physics::CharacterState state = stateOf(world, character);
    REQUIRE_THAT(state.position.x, WithinAbs(8.0, 0.1));
    REQUIRE_THAT(state.position.y, WithinAbs(0.0, kContactTolerance));
    REQUIRE_THAT(state.velocity.x, WithinAbs(kWalkSpeed, 0.05));
    REQUIRE(state.ground == GroundState::OnGround);
}

// AC-08.
TEST_CASE("A running character does not go through a 10 cm wall", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    // Near face at x = 4.95.
    (void)world.addBox({5.0f, 1.5f, 0.0f}, {0.05f, 1.5f, 4.0f}, MotionType::Static);
    const float speed = GENERATE(kRunSpeed, 30.0f);
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});

    const Extremes extremes = drive(world, character, {speed, 0.0f, 0.0f}, 3.0);
    // The capsule's surface never passes the wall's near face.
    REQUIRE(extremes.farthestX < 4.95f - kRadius + 0.01f);
    REQUIRE_THAT(stateOf(world, character).position.x, WithinAbs(4.95 - kRadius, kContactTolerance));
    // The velocity read back is the one asked for; the position shows that the wall stopped the character.
    REQUIRE(stateOf(world, character).velocity.x == speed);
    const float before = stateOf(world, character).position.x;
    (void)drive(world, character, {speed, 0.0f, 0.0f}, 1.0);
    REQUIRE_THAT(stateOf(world, character).position.x, WithinAbs(before, 1e-3));
}

// AC-09.
TEST_CASE("A walking character pushes a crate without entering it", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::BodyHandle crate =
        world.addBox({2.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Dynamic, 20.0f);
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});

    (void)drive(world, character, {kWalkSpeed, 0.0f, 0.0f}, 3.0);
    const float crateX = world.position(crate).x;
    const float characterX = stateOf(world, character).position.x;
    REQUIRE(crateX > 3.0f);
    // The capsule stays behind the crate's near face.
    REQUIRE(characterX < crateX - 0.5f - kRadius + 0.05f);
    // The crate slowed the character down.
    REQUIRE(characterX < 12.0f - 0.5f);
    REQUIRE_THAT(world.position(crate).y, WithinAbs(0.5, 0.05));
}

TEST_CASE("A character cannot push a static body", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::BodyHandle block = world.addBox({2.0f, 0.5f, 0.0f}, glm::vec3(0.5f), MotionType::Static);
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});

    (void)drive(world, character, {kWalkSpeed, 0.0f, 0.0f}, 2.0);
    REQUIRE(world.position(block) == glm::vec3(2.0f, 0.5f, 0.0f));
    REQUIRE_THAT(stateOf(world, character).position.x, WithinAbs(1.5 - kRadius, kContactTolerance));
}

TEST_CASE("A character climbs a 30 degree ramp but not a 60 degree one", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const float degrees = GENERATE(30.0f, 60.0f);
    addRamp(world, 2.0f, degrees);
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});

    const Extremes extremes = drive(world, character, {kWalkSpeed, 0.0f, 0.0f}, 3.0);
    if (degrees < 45.0f) {
        // 2 m of flat ground, then about 2.5 s of ramp.
        REQUIRE(stateOf(world, character).position.y > 2.0f);
        REQUIRE(stateOf(world, character).ground == GroundState::OnGround);
    } else {
        REQUIRE(extremes.highest < 0.6f);
        REQUIRE(extremes.farthestX < 2.6f);
    }
}

TEST_CASE("A character walks up a 0.25 m step but is stopped by a 0.6 m one", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const float height = GENERATE(0.25f, 0.6f);
    // A platform whose near face is at x = 2.
    (void)world.addBox({5.0f, 0.5f * height, 0.0f}, {3.0f, 0.5f * height, 3.0f}, MotionType::Static);
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});

    (void)drive(world, character, {kWalkSpeed, 0.0f, 0.0f}, 1.5);
    const physics::CharacterState state = stateOf(world, character);
    if (height < 0.3f) {
        REQUIRE(state.position.x > 4.0f);
        REQUIRE_THAT(state.position.y, WithinAbs(height, kContactTolerance));
        REQUIRE(state.ground == GroundState::OnGround);
    } else {
        REQUIRE_THAT(state.position.x, WithinAbs(2.0 - kRadius, kContactTolerance));
        REQUIRE_THAT(state.position.y, WithinAbs(0.0, kContactTolerance));
    }
}

TEST_CASE("A jump at 4.85 m/s rises about 1.2 m and comes back down", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});
    (void)drive(world, character, glm::vec3(0.0f), 0.5);
    REQUIRE(stateOf(world, character).ground == GroundState::OnGround);

    // v = sqrt(2 g h), spec 5.8: h = 1.2 m.
    const float jumpSpeed = std::sqrt(2.0f * 9.81f * 1.2f);
    const Extremes extremes = drive(world, character, glm::vec3(0.0f), 1.5, jumpSpeed);
    REQUIRE_THAT(extremes.highest, WithinAbs(1.2, 0.08));
    REQUIRE(stateOf(world, character).ground == GroundState::OnGround);
    REQUIRE_THAT(stateOf(world, character).position.y, WithinAbs(0.0, kContactTolerance));
}

TEST_CASE("A character can be moved to another place", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::CharacterHandle character = addCharacter(world, {0.0f, 0.0f, 0.0f});
    world->setCharacterPosition(character, {10.0f, 0.0f, -4.0f});
    REQUIRE(stateOf(world, character).position == glm::vec3(10.0f, 0.0f, -4.0f));
    (void)drive(world, character, glm::vec3(0.0f), 0.5);
    REQUIRE(stateOf(world, character).ground == GroundState::OnGround);
    REQUIRE_THAT(stateOf(world, character).position.x, WithinAbs(10.0, 1e-3));
}

TEST_CASE("A stale or invalid character handle is harmless", "[physics]") {
    const PhysicsTestWorld world;
    (void)world.addFloor();
    const physics::CharacterHandle first = addCharacter(world, {0.0f, 0.0f, 0.0f});
    world->destroyCharacter(first);
    REQUIRE(world->characterCount() == 0);

    // The slot is reused: the old handle must not name the new character.
    const physics::CharacterHandle second = addCharacter(world, {5.0f, 0.0f, 0.0f});
    REQUIRE(first != second);
    for (const physics::CharacterHandle handle : {first, physics::CharacterHandle{}}) {
        REQUIRE_FALSE(world->contains(handle));
        REQUIRE_FALSE(world->characterState(handle).has_value());
        world->setCharacterVelocity(handle, {1.0f, 0.0f, 0.0f});
        world->setCharacterPosition(handle, {9.0f, 9.0f, 9.0f});
        world->destroyCharacter(handle);
    }
    world.stepSeconds(0.1);
    REQUIRE(world->characterCount() == 1);
    REQUIRE_THAT(stateOf(world, second).position.x, WithinAbs(5.0, 1e-3));
}

TEST_CASE("An invalid character description is refused with its reason", "[physics]") {
    const PhysicsTestWorld world;
    const auto squat = world->createCharacter({.height = 0.5f, .radius = 0.3f});
    REQUIRE_FALSE(squat.has_value());
    REQUIRE(squat.error().code == core::ErrorCode::InvalidArgument);
    REQUIRE_THAT(squat.error().message, ContainsSubstring("height"));

    REQUIRE_FALSE(world->createCharacter({.radius = 0.0f}).has_value());
    REQUIRE_FALSE(world->createCharacter({.maxSlopeRadians = glm::radians(90.0f)}).has_value());
    REQUIRE_FALSE(world->createCharacter({.stepHeight = -0.1f}).has_value());
    REQUIRE_FALSE(world->createCharacter({.mass = 0.0f}).has_value());
    REQUIRE_FALSE(world->createCharacter({.maxPushForce = -1.0f}).has_value());
    REQUIRE(world->characterCount() == 0);
}
