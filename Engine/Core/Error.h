#pragma once

#include <expected>
#include <string>
#include <utility>

namespace apex::core {

enum class ErrorCode {
    InvalidArgument,
    NotFound,
    IoFailure,
    PlatformFailure,
    // A GPU or graphics API call failed, or no GPU meets the requirements.
    GpuFailure,
};

struct Error {
    ErrorCode code;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] inline std::unexpected<Error> makeError(ErrorCode code, std::string message) {
    return std::unexpected(Error{code, std::move(message)});
}

} // namespace apex::core
