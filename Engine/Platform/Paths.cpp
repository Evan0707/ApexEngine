#include "Platform/Paths.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>

#include <format>
#include <string_view>

namespace apex::platform {

core::Result<std::filesystem::path> executableDirectory() {
    // Owned by SDL, UTF-8, ends with a separator.
    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        return core::makeError(core::ErrorCode::PlatformFailure,
                               std::format("Cannot find the executable's folder: {}", SDL_GetError()));
    }
    const std::string_view utf8(basePath);
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

} // namespace apex::platform
