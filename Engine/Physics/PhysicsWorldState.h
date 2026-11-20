#pragma once

// Internal to Physics: the private state of PhysicsWorld, shared by its source files.

#include "Physics/JoltInclude.h"
#include "Physics/PhysicsWorld.h"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

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

// A character and how Jolt updates it. The slot is free when `character` is empty; `generation` counts its
// reuses so that an old handle does not name a new character.
struct CharacterSlot {
    std::unique_ptr<JPH::CharacterVirtual> character;
    JPH::CharacterVirtual::ExtendedUpdateSettings update;
    std::uint32_t generation = 0;
};

// Members are destroyed in reverse order: the characters, the system, then what it points to.
struct PhysicsWorld::State {
    explicit State(const PhysicsSettings& settings);

    BroadPhaseLayers broadPhaseLayers;
    LayerPairs layerPairs;
    JPH::ObjectVsBroadPhaseLayerFilterTable objectVsBroadPhase;
    JPH::TempAllocatorImpl tempAllocator;
    // One thread: multithreading is a deferred optimization (spec 4.5, decision 0018).
    JPH::JobSystemSingleThreaded jobs;
    JPH::PhysicsSystem system;
    std::vector<CharacterSlot> characters;

    // Null for an invalid or stale handle.
    [[nodiscard]] JPH::CharacterVirtual* findCharacter(CharacterHandle handle) const noexcept;
    void updateCharacters(float deltaSeconds);
};

} // namespace apex::physics
