#include "Core/Error.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using apex::core::Error;
using apex::core::ErrorCode;
using apex::core::makeError;
using apex::core::Result;

namespace {

Result<int> parseDigit(char character) {
    if (character < '0' || character > '9') {
        return makeError(ErrorCode::InvalidArgument, std::string("Not a digit: ") + character);
    }
    return character - '0';
}

Result<void> requireNonEmpty(const std::string& path) {
    if (path.empty()) {
        return makeError(ErrorCode::NotFound, "Empty path");
    }
    return {};
}

} // namespace

TEST_CASE("Result holds the value on success", "[core]") {
    const Result<int> digit = parseDigit('7');
    REQUIRE(digit.has_value());
    REQUIRE(digit.value() == 7);
}

TEST_CASE("Result holds the error code and message on failure", "[core]") {
    const Result<int> digit = parseDigit('x');
    REQUIRE_FALSE(digit.has_value());
    REQUIRE(digit.error().code == ErrorCode::InvalidArgument);
    REQUIRE(digit.error().message == "Not a digit: x");
}

TEST_CASE("Result<void> reports success and failure", "[core]") {
    REQUIRE(requireNonEmpty("Models/cube.glb").has_value());

    const Result<void> missing = requireNonEmpty("");
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == ErrorCode::NotFound);
    REQUIRE(missing.error().message == "Empty path");
}

TEST_CASE("An error propagates between Result types", "[core]") {
    const Result<int> digit = parseDigit('x');
    const Result<std::string> text = std::unexpected(digit.error());
    REQUIRE(text.error().code == ErrorCode::InvalidArgument);
    REQUIRE(text.error().message == "Not a digit: x");
}
