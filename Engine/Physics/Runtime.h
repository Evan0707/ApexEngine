#pragma once

#include "Core/Error.h"

namespace apex::physics {

// Jolt keeps process-wide state: its allocator, its type factory and its type table. This object creates it
// and tears it down (decision 0019). The application owns exactly one and passes it to PhysicsWorld::create;
// it must outlive every PhysicsWorld. A second one alive at the same time is a programming error.
class Runtime final {
public:
    // Fails when the Jolt library was built with other options than this code.
    [[nodiscard]] static core::Result<Runtime> create();

    Runtime(Runtime&& other) noexcept;
    Runtime& operator=(Runtime&& other) noexcept;
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    ~Runtime();

private:
    Runtime() noexcept = default;
    void shutdown() noexcept;

    bool m_active = false;
};

} // namespace apex::physics
