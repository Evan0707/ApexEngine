#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace apex::core {

struct Version {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    [[nodiscard]] std::string toString() const;

    friend constexpr auto operator<=>(const Version&, const Version&) = default;
};

/// Set by project(VERSION ...) in the root CMakeLists.
[[nodiscard]] Version engineVersion() noexcept;

} // namespace apex::core
