#pragma once

#include "Core/Error.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace apex::resources {

// The whole file, in binary. NotFound when it does not exist, IoFailure when it cannot be read; the
// error quotes the path.
[[nodiscard]] core::Result<std::vector<std::byte>> readFile(const std::filesystem::path& path);

} // namespace apex::resources
