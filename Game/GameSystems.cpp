#include "GameSystems.h"

#include "Core/Math.h"
#include "World/Components.h"
#include "World/Rotation.h"
#include "World/SceneQueries.h"

#include <format>

namespace apex::game {

core::Result<GameEntities> attachGameComponents(entt::registry& registry,
                                                const world::CharacterBody& characterBody) {
    const entt::entity camera = world::findBySceneId(registry, kCameraId);
    if (camera == entt::null || !registry.all_of<world::Transform, world::Camera>(camera)) {
        return core::makeError(
            core::ErrorCode::NotFound,
            std::format("The scene has no entity '{}' with a transform and a camera", kCameraId));
    }
    // The file gives pitch, yaw and roll; the free camera has no roll.
    const glm::vec3 degrees =
        world::eulerDegreesFromRotation(registry.get<world::Transform>(camera).rotation);
    registry.emplace_or_replace<FreeCameraController>(camera, glm::radians(degrees.y),
                                                      glm::radians(degrees.x));

    if (const entt::entity spinner = world::findBySceneId(registry, kSpinnerId);
        spinner != entt::null && registry.all_of<world::Transform>(spinner)) {
        registry.emplace_or_replace<Spin>(spinner, glm::radians(kSpinnerDegreesPerSecond));
    }

    GameEntities entities{.camera = camera};
    if (const entt::entity player = world::findBySceneId(registry, kPlayerId);
        player != entt::null && registry.all_of<world::Transform>(player)) {
        const glm::vec3 facing =
            world::eulerDegreesFromRotation(registry.get<world::Transform>(player).rotation);
        registry.emplace_or_replace<world::CharacterBody>(player, characterBody);
        registry.emplace_or_replace<PlayerController>(player,
                                                      PlayerState{.facingYaw = glm::radians(facing.y)});
        entities.player = player;
    }
    return entities;
}

void stepFreeCameraEntity(entt::registry& registry, entt::entity camera, const FreeCameraInput& input,
                          double deltaSeconds, const FreeCameraSettings& settings) {
    const FreeCameraController& controller = registry.get<FreeCameraController>(camera);
    const FreeCameraState current{.position = registry.get<world::Transform>(camera).position,
                                  .yaw = controller.yaw,
                                  .pitch = controller.pitch};
    placeFreeCameraEntity(registry, camera, stepFreeCamera(current, input, deltaSeconds, settings));
}

void placeFreeCameraEntity(entt::registry& registry, entt::entity camera, const FreeCameraState& state) {
    registry.emplace_or_replace<FreeCameraController>(camera, state.yaw, state.pitch);
    world::Transform& transform = registry.get<world::Transform>(camera);
    transform.position = state.position;
    transform.rotation = freeCameraOrientation(state);
}

void stepPlayerEntity(entt::registry& registry, entt::entity player, const PlayerInput& input,
                      const glm::vec3& gravity, double deltaSeconds, const PlayerSettings& settings) {
    const world::CharacterGround& ground = registry.get<world::CharacterGround>(player);
    const PlayerSupport support{.onGround = ground.state == physics::GroundState::OnGround,
                                .groundVelocity = ground.groundVelocity};
    PlayerController& controller = registry.get<PlayerController>(player);
    controller.state = stepPlayer(controller.state, input, support, gravity, deltaSeconds, settings);

    registry.get<world::CharacterMotion>(player).velocity = playerMotion(controller.state, support);
    registry.get<world::Transform>(player).rotation = playerOrientation(controller.state.facingYaw);
}

void stepSpinners(entt::registry& registry, double deltaSeconds) {
    for (const auto [entity, transform, spin] : registry.view<world::Transform, const Spin>().each()) {
        const float angle = spin.radiansPerSecond * static_cast<float>(deltaSeconds);
        // Renormalized: thousands of small products would let the quaternion drift.
        transform.rotation = glm::normalize(glm::angleAxis(angle, core::kWorldUp) * transform.rotation);
    }
}

} // namespace apex::game
