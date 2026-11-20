#include "Graphics/MipLevels.h"

#include <catch2/catch_test_macros.hpp>

using apex::graphics::mipLevelCount;
using apex::graphics::mipSize;

TEST_CASE("A mip chain goes down to one pixel on the longest side", "[graphics]") {
    STATIC_REQUIRE(mipLevelCount(1, 1) == 1);
    STATIC_REQUIRE(mipLevelCount(256, 256) == 9);
    STATIC_REQUIRE(mipLevelCount(1024, 512) == 11);
    STATIC_REQUIRE(mipLevelCount(300, 7) == 9);
    STATIC_REQUIRE(mipLevelCount(0, 0) == 0);
}

TEST_CASE("Each mip level halves the size, never below one pixel", "[graphics]") {
    STATIC_REQUIRE(mipSize(256, 0) == 256);
    STATIC_REQUIRE(mipSize(256, 5) == 8);
    STATIC_REQUIRE(mipSize(300, 1) == 150);
    STATIC_REQUIRE(mipSize(512, 10) == 1);
    STATIC_REQUIRE(mipSize(512, 40) == 1);
}
