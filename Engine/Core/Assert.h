#pragma once

namespace apex::core {

[[noreturn]] void reportAssertionFailure(const char* condition, const char* message, const char* file,
                                         int line) noexcept;

} // namespace apex::core

// For programming errors only; recoverable failures return core::Result.
// The condition is not evaluated in Release: it must have no side effect.
#ifndef NDEBUG
#define APEX_ASSERT(condition, message)                                                                      \
    static_cast<void>(                                                                                       \
        (condition) ||                                                                                       \
        (::apex::core::reportAssertionFailure(#condition, message, __FILE__, __LINE__), false))
#else
#define APEX_ASSERT(condition, message) static_cast<void>(sizeof(!(condition)))
#endif
