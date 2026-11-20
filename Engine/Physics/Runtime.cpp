#include "Physics/Runtime.h"

#include "Core/Assert.h"
#include "Physics/JoltInclude.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <memory>
#include <utility>

namespace apex::physics {

core::Result<Runtime> Runtime::create() {
    APEX_ASSERT(JPH::Factory::sInstance == nullptr, "A physics Runtime is already alive");
    if (!JPH::VerifyJoltVersionID()) {
        return core::makeError(core::ErrorCode::PhysicsFailure,
                               "The Jolt library was built with other options than the engine");
    }
    JPH::RegisterDefaultAllocator();
    // Jolt reads the factory through this pointer; shutdown() takes it back.
    JPH::Factory::sInstance = std::make_unique<JPH::Factory>().release();
    JPH::RegisterTypes();

    Runtime runtime;
    runtime.m_active = true;
    return runtime;
}

Runtime::Runtime(Runtime&& other) noexcept : m_active(std::exchange(other.m_active, false)) {}

Runtime& Runtime::operator=(Runtime&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_active = std::exchange(other.m_active, false);
    }
    return *this;
}

Runtime::~Runtime() {
    shutdown();
}

void Runtime::shutdown() noexcept {
    if (!m_active) {
        return;
    }
    JPH::UnregisterTypes();
    const std::unique_ptr<JPH::Factory> factory(std::exchange(JPH::Factory::sInstance, nullptr));
    m_active = false;
}

} // namespace apex::physics
