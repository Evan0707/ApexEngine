#pragma once

#include "Core/Error.h"

#include <compare>
#include <string>
#include <string_view>

namespace apex::resources {

// A path relative to the Assets/ folder (spec 3.3), normalized: '/' separators, no "." or empty
// segment, ".." resolved, never absolute and never above Assets/. It is the key of the resource cache:
// two spellings of the same file give the same AssetPath. Case is kept as written.
class AssetPath {
public:
    // Accepts '\' as a separator. Fails on an empty, absolute or escaping path; the error quotes it.
    [[nodiscard]] static core::Result<AssetPath> parse(std::string_view path);

    // A path written inside the asset at this path (a texture referenced by a model): relative to this
    // asset's folder.
    [[nodiscard]] core::Result<AssetPath> resolveSibling(std::string_view relative) const;

    [[nodiscard]] const std::string& str() const noexcept;

    [[nodiscard]] bool operator==(const AssetPath&) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(const AssetPath&) const noexcept = default;

private:
    explicit AssetPath(std::string path) noexcept;

    std::string m_path;
};

} // namespace apex::resources
