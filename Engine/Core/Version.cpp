#include "Core/Version.h"

#include <format>

namespace apex::core {

std::string Version::toString() const {
    return std::format("{}.{}.{}", major, minor, patch);
}

Version engineVersion() noexcept {
    return Version{APEX_VERSION_MAJOR, APEX_VERSION_MINOR, APEX_VERSION_PATCH};
}

} // namespace apex::core
