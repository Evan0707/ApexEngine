#include "Physics/PhysicsWorld.h"

#include "Core/Assert.h"
#include "Core/Profiling.h"
#include "Physics/PhysicsWorldState.h"

#include <Jolt/Physics/PhysicsSettings.h>

#include <utility>

namespace apex::physics {

BroadPhaseLayers::BroadPhaseLayers()
    : JPH::BroadPhaseLayerInterfaceTable(kObjectLayerCount, kBroadPhaseLayerCount) {
    MapObjectToBroadPhaseLayer(kStaticLayer, kNonMovingBroadPhase);
    MapObjectToBroadPhaseLayer(kDynamicLayer, kMovingBroadPhase);
    MapObjectToBroadPhaseLayer(kCharacterLayer, kMovingBroadPhase);
}

LayerPairs::LayerPairs() : JPH::ObjectLayerPairFilterTable(kObjectLayerCount) {
    EnableCollision(kStaticLayer, kDynamicLayer);
    EnableCollision(kStaticLayer, kCharacterLayer);
    EnableCollision(kDynamicLayer, kDynamicLayer);
    EnableCollision(kDynamicLayer, kCharacterLayer);
}

PhysicsWorld::State::State(const PhysicsSettings& settings)
    : objectVsBroadPhase(broadPhaseLayers, kBroadPhaseLayerCount, layerPairs, kObjectLayerCount),
      tempAllocator(kTempAllocatorBytes), jobs(JPH::cMaxPhysicsJobs) {
    // Zero body mutexes: Jolt picks its default.
    system.Init(settings.maxBodies, 0, settings.maxBodyPairs, settings.maxContactConstraints,
                broadPhaseLayers, objectVsBroadPhase, layerPairs);
    system.SetGravity(toJolt(settings.gravity));
}

core::Result<std::unique_ptr<PhysicsWorld>> PhysicsWorld::create(const Runtime& /*runtime*/,
                                                                 const PhysicsSettings& settings) {
    if (settings.maxBodies == 0 || settings.maxBodyPairs == 0 || settings.maxContactConstraints == 0) {
        return core::makeError(core::ErrorCode::InvalidArgument, "Physics capacities must be positive");
    }
    return std::make_unique<PhysicsWorld>(ConstructionKey{}, std::make_unique<State>(settings));
}

PhysicsWorld::PhysicsWorld(ConstructionKey /*key*/, std::unique_ptr<State> state) noexcept
    : m_state(std::move(state)) {}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::step(double deltaSeconds) {
    APEX_PROFILE_SCOPE("Physics step");
    APEX_ASSERT(deltaSeconds > 0.0, "A physics step needs a positive duration");
    // One collision step per fixed step (spec 5.7: 1/60 s).
    const JPH::EPhysicsUpdateError error =
        m_state->system.Update(static_cast<float>(deltaSeconds), 1, &m_state->tempAllocator, &m_state->jobs);
    APEX_ASSERT(error == JPH::EPhysicsUpdateError::None, "A physics capacity was exceeded during the step");
    static_cast<void>(error);
}

glm::vec3 PhysicsWorld::gravity() const {
    return toGlm(m_state->system.GetGravity());
}

std::uint32_t PhysicsWorld::bodyCount() const {
    return m_state->system.GetNumBodies();
}

} // namespace apex::physics
