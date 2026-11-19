#include "Core/Assert.h"

#include <catch2/catch_test_macros.hpp>

// A failing assertion aborts the process, so only the passing path is testable.
TEST_CASE("A passing assertion has no effect", "[core]") {
    int evaluations = 0;
    const auto isPositive = [&evaluations](int value) {
        ++evaluations;
        return value > 0;
    };

    APEX_ASSERT(isPositive(4), "Value must be positive");
    APEX_ASSERT(true, "Constant conditions compile without warning");

    REQUIRE(evaluations <= 1);
}
