#pragma once

// Internal to Physics: the private state of PhysicsWorld, shared by its source files.

#include "Physics/JoltInclude.h"
#include "Physics/PhysicsWorld.h"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstddef>

namespace apex::physics {

// Object layers (spec 5.7). Static touches dynamic and character; dynamic touches everything; character
// touches static and dynamic (decision 0019).
inline constexpr JPH::ObjectLayer kStaticLayer = 0;
inline constexpr JPH::ObjectLayer kDynamicLayer = 1;
inline constexpr JPH::ObjectLayer kCharacterLayer = 2;
inline constexpr JPH::uint kObjectLayerCount = 3;

// Broad phase trees: what never moves, and the rest.
inline constexpr JPH::BroadPhaseLayer kNonMovingBroadPhase{0};
inline constexpr JPH::BroadPhaseLayer kMovingBroadPhase{1};
inline constexpr JPH::uint kBroadPhaseLayerCount = 2;

// Scratch memory of one step.
inline constexpr std::size_t kTempAllocatorBytes = 8 * 1024 * 1024;

// Jolt's tables cannot be copied: each one is filled by its own constructor.
struct BroadPhaseLayers final : JPH::BroadPhaseLayerInterfaceTable {
    BroadPhaseLayers();
};

struct LayerPairs final : JPH::ObjectLayerPairFilterTable {
    LayerPairs();
};

// Members are destroyed in reverse order: the system first, then what it points to.
struct PhysicsWorld::State {
    explicit State(const PhysicsSettings& settings);

    BroadPhaseLayers broadPhaseLayers;
    LayerPairs layerPairs;
    JPH::ObjectVsBroadPhaseLayerFilterTable objectVsBroadPhase;
    JPH::TempAllocatorImpl tempAllocator;
    // One thread: multithreading is a deferred optimization (spec 4.5, decision 0018).
    JPH::JobSystemSingleThreaded jobs;
    JPH::PhysicsSystem system;
};

} // namespace apex::physics
