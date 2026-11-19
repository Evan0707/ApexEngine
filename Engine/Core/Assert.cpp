#include "Core/Assert.h"

#include <cstdio>
#include <cstdlib>

namespace apex::core {

void reportAssertionFailure(const char* condition, const char* message, const char* file, int line) noexcept {
    // No logger here: it may be the failing code, and an assertion must not allocate.
    std::fprintf(stderr, "Assertion failed: %s\n  %s\n  at %s:%d\n", condition, message, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace apex::core
