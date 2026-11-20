#include "Physics/PhysicsWorldState.h"

#include <Jolt/Geometry/Plane.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>

#include <cmath>
#include <format>
#include <memory>
#include <optional>
#include <string>

namespace apex::physics {

namespace {

[[nodiscard]] bool isPositive(float value) noexcept {
    return std::isfinite(value) && value > 0.0f;
}

[[nodiscard]] std::optional<std::string> validate(const CharacterDesc& desc) {
    if (!isPositive(desc.radius) || !isPositive(desc.height)) {
        return "the radius and the height must be positive";
    }
    if (desc.height <= 2.0f * desc.radius) {
        return "the height must be more than twice the radius";
    }
    if (!std::isfinite(desc.maxSlopeRadians) || desc.maxSlopeRadians < 0.0f ||
        desc.maxSlopeRadians >= glm::half_pi<float>()) {
        return "the maximum slope must be between 0 and 90 degrees";
    }
    if (!std::isfinite(desc.stepHeight) || desc.stepHeight < 0.0f) {
        return "the step height must not be negative";
    }
    if (!isPositive(desc.mass) || !std::isfinite(desc.maxPushForce) || desc.maxPushForce < 0.0f) {
        return "the mass must be positive and the push force must not be negative";
    }
    if (!std::isfinite(desc.position.x) || !std::isfinite(desc.position.y) ||
        !std::isfinite(desc.position.z)) {
        return "the position must be finite";
    }
    return std::nullopt;
}

[[nodiscard]] GroundState toGroundState(JPH::CharacterBase::EGroundState state) noexcept {
    switch (state) {
    case JPH::CharacterBase::EGroundState::OnGround:
        return GroundState::OnGround;
    case JPH::CharacterBase::EGroundState::OnSteepGround:
        return GroundState::OnSteepGround;
    case JPH::CharacterBase::EGroundState::NotSupported:
    case JPH::CharacterBase::EGroundState::InAir:
        break;
    }
    return GroundState::InAir;
}

} // namespace

JPH::CharacterVirtual* PhysicsWorld::State::findCharacter(CharacterHandle handle) const noexcept {
    if (handle.index >= characters.size() || characters[handle.index].generation != handle.generation) {
        return nullptr;
    }
    return characters[handle.index].character.get();
}

void PhysicsWorld::State::updateCharacters(float deltaSeconds) {
    const JPH::DefaultBroadPhaseLayerFilter broadPhaseFilter =
        system.GetDefaultBroadPhaseLayerFilter(kCharacterLayer);
    const JPH::DefaultObjectLayerFilter layerFilter = system.GetDefaultLayerFilter(kCharacterLayer);
    const JPH::BodyFilter bodyFilter;
    const JPH::ShapeFilter shapeFilter;
    for (CharacterSlot& slot : characters) {
        if (slot.character) {
            // Jolt's own update: slide, stick to the floor, walk up stairs, push bodies (spec 5.8).
            slot.character->ExtendedUpdate(deltaSeconds, system.GetGravity(), slot.update, broadPhaseFilter,
                                           layerFilter, bodyFilter, shapeFilter, tempAllocator);
        }
    }
}

core::Result<CharacterHandle> PhysicsWorld::createCharacter(const CharacterDesc& desc) {
    if (const std::optional<std::string> problem = validate(desc)) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("Invalid character: {}", *problem));
    }
    // The capsule is lifted so that the character's position is at its feet.
    JPH::CapsuleShapeSettings capsule(0.5f * desc.height - desc.radius, desc.radius);
    capsule.SetEmbedded();
    JPH::RotatedTranslatedShapeSettings lifted(JPH::Vec3(0.0f, 0.5f * desc.height, 0.0f),
                                               JPH::Quat::sIdentity(), &capsule);
    lifted.SetEmbedded();
    const JPH::ShapeSettings::ShapeResult shape = lifted.Create();
    if (shape.HasError()) {
        return core::makeError(
            core::ErrorCode::PhysicsFailure,
            std::format("Jolt refused the character's shape: {}", shape.GetError().c_str()));
    }

    JPH::CharacterVirtualSettings settings;
    settings.SetEmbedded();
    settings.mShape = shape.Get();
    settings.mMaxSlopeAngle = desc.maxSlopeRadians;
    settings.mMass = desc.mass;
    settings.mMaxStrength = desc.maxPushForce;
    // Only contacts on the lower half sphere of the capsule can carry the character.
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -desc.radius);

    CharacterSlot* slot = nullptr;
    std::uint32_t index = 0;
    for (; index < m_state->characters.size(); ++index) {
        if (!m_state->characters[index].character) {
            slot = &m_state->characters[index];
            break;
        }
    }
    if (slot == nullptr) {
        slot = &m_state->characters.emplace_back();
    }
    slot->character = std::make_unique<JPH::CharacterVirtual>(&settings, toJolt(desc.position),
                                                              JPH::Quat::sIdentity(), &m_state->system);
    // Owned by the slot, not by Jolt's reference count.
    slot->character->SetEmbedded();
    slot->update = {};
    slot->update.mWalkStairsStepUp = JPH::Vec3(0.0f, desc.stepHeight, 0.0f);
    slot->update.mStickToFloorStepDown = JPH::Vec3(0.0f, -desc.stepHeight, 0.0f);
    return CharacterHandle{.index = index, .generation = slot->generation};
}

void PhysicsWorld::destroyCharacter(CharacterHandle character) {
    if (m_state->findCharacter(character) == nullptr) {
        return;
    }
    CharacterSlot& slot = m_state->characters[character.index];
    slot.character.reset();
    ++slot.generation;
}

bool PhysicsWorld::contains(CharacterHandle character) const {
    return m_state->findCharacter(character) != nullptr;
}

std::uint32_t PhysicsWorld::characterCount() const {
    std::uint32_t count = 0;
    for (const CharacterSlot& slot : m_state->characters) {
        count += slot.character ? 1u : 0u;
    }
    return count;
}

void PhysicsWorld::setCharacterVelocity(CharacterHandle character, const glm::vec3& velocity) {
    if (JPH::CharacterVirtual* found = m_state->findCharacter(character)) {
        found->SetLinearVelocity(toJolt(velocity));
    }
}

void PhysicsWorld::setCharacterPosition(CharacterHandle character, const core::WorldPosition& position) {
    if (JPH::CharacterVirtual* found = m_state->findCharacter(character)) {
        found->SetPosition(toJolt(position));
    }
}

std::optional<CharacterState> PhysicsWorld::characterState(CharacterHandle character) const {
    const JPH::CharacterVirtual* found = m_state->findCharacter(character);
    if (found == nullptr) {
        return std::nullopt;
    }
    return CharacterState{.position = toGlm(found->GetPosition()),
                          .velocity = toGlm(found->GetLinearVelocity()),
                          .ground = toGroundState(found->GetGroundState()),
                          .groundNormal = toGlm(found->GetGroundNormal()),
                          .groundVelocity = toGlm(found->GetGroundVelocity())};
}

} // namespace apex::physics
