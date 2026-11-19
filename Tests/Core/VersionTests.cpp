#include "Core/Version.h"

#include <catch2/catch_test_macros.hpp>

using apex::core::Version;

TEST_CASE("Version formats as major.minor.patch", "[core]") {
    REQUIRE(Version{1, 2, 3}.toString() == "1.2.3");
    REQUIRE(Version{}.toString() == "0.0.0");
}

TEST_CASE("Versions compare in semantic order", "[core]") {
    // Extra parentheses: Version has no stream operator.
    REQUIRE((Version{1, 0, 0} < Version{1, 0, 1}));
    REQUIRE((Version{1, 9, 9} < Version{2, 0, 0}));
    REQUIRE((Version{0, 3, 0} == Version{0, 3, 0}));
}

TEST_CASE("Engine version is set by the build", "[core]") {
    REQUIRE((apex::core::engineVersion() > Version{0, 0, 0}));
}
