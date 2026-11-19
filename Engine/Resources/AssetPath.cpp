#include "Resources/AssetPath.h"

#include <format>
#include <utility>
#include <vector>

namespace apex::resources {

namespace {

[[nodiscard]] std::unexpected<core::Error> invalidPath(std::string_view path, std::string_view reason) {
    return core::makeError(core::ErrorCode::InvalidArgument,
                           std::format("Invalid asset path '{}': {}", path, reason));
}

} // namespace

core::Result<AssetPath> AssetPath::parse(std::string_view path) {
    if (path.empty()) {
        return invalidPath(path, "empty");
    }
    if (path.front() == '/' || path.front() == '\\' || path.find(':') != std::string_view::npos) {
        return invalidPath(path, "absolute paths are not allowed, paths are relative to Assets/");
    }

    std::vector<std::string_view> segments;
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find_first_of("/\\", start);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        const std::string_view segment = path.substr(start, end - start);
        if (segment == "..") {
            if (segments.empty()) {
                return invalidPath(path, "it leaves the Assets/ folder");
            }
            segments.pop_back();
        } else if (!segment.empty() && segment != ".") {
            segments.push_back(segment);
        }
        start = end + 1;
    }
    if (segments.empty()) {
        return invalidPath(path, "it names no file");
    }

    std::string normalized;
    for (const std::string_view segment : segments) {
        if (!normalized.empty()) {
            normalized += '/';
        }
        normalized += segment;
    }
    return AssetPath(std::move(normalized));
}

core::Result<AssetPath> AssetPath::resolveSibling(std::string_view relative) const {
    const std::size_t slash = m_path.rfind('/');
    if (slash == std::string::npos) {
        return parse(relative);
    }
    return parse(std::format("{}/{}", std::string_view(m_path).substr(0, slash), relative));
}

const std::string& AssetPath::str() const noexcept {
    return m_path;
}

AssetPath::AssetPath(std::string path) noexcept : m_path(std::move(path)) {}

} // namespace apex::resources
