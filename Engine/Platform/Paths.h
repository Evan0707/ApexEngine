#pragma once

#include "Core/Error.h"

#include <filesystem>

namespace apex::platform {

// The folder that holds the running executable, where the build copies Assets/ (decision 0014). Works
// before any window exists. PlatformFailure when the system cannot tell.
[[nodiscard]] core::Result<std::filesystem::path> executableDirectory();

} // namespace apex::platform
