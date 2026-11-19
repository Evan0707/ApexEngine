#include "Resources/FileReader.h"

#include <format>
#include <fstream>
#include <system_error>

namespace apex::resources {

core::Result<std::vector<std::byte>> readFile(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return core::makeError(core::ErrorCode::NotFound,
                               std::format("File not found: '{}'", path.generic_string()));
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return core::makeError(core::ErrorCode::IoFailure,
                               std::format("Cannot open '{}'", path.generic_string()));
    }
    const std::streamoff size = file.tellg();
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        return core::makeError(core::ErrorCode::IoFailure,
                               std::format("Cannot read '{}'", path.generic_string()));
    }
    return bytes;
}

} // namespace apex::resources
