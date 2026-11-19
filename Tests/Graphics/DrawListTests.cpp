#include "Graphics/DrawList.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using apex::graphics::DrawItem;
using apex::graphics::sortDrawItems;

namespace {

[[nodiscard]] std::vector<std::uint32_t> objectOrder(const std::vector<DrawItem>& items) {
    std::vector<std::uint32_t> order;
    for (const DrawItem& item : items) {
        order.push_back(item.objectIndex);
    }
    return order;
}

} // namespace

TEST_CASE("Draw items are sorted by material, then by mesh", "[graphics]") {
    std::vector<DrawItem> items{
        {.objectIndex = 0, .materialIndex = 2, .meshIndex = 0},
        {.objectIndex = 1, .materialIndex = 1, .meshIndex = 1},
        {.objectIndex = 2, .materialIndex = 1, .meshIndex = 0},
        {.objectIndex = 3, .materialIndex = 0, .meshIndex = 5},
    };
    sortDrawItems(items);
    REQUIRE(objectOrder(items) == std::vector<std::uint32_t>{3, 2, 1, 0});
}

TEST_CASE("Sorting keeps the scene order inside a group", "[graphics]") {
    std::vector<DrawItem> items{
        {.objectIndex = 0, .materialIndex = 1, .meshIndex = 0},
        {.objectIndex = 1, .materialIndex = 0, .meshIndex = 0},
        {.objectIndex = 2, .materialIndex = 1, .meshIndex = 0},
        {.objectIndex = 3, .materialIndex = 0, .meshIndex = 0},
    };
    sortDrawItems(items);
    REQUIRE(objectOrder(items) == std::vector<std::uint32_t>{1, 3, 0, 2});
}

TEST_CASE("An empty draw list stays empty", "[graphics]") {
    std::vector<DrawItem> items;
    sortDrawItems(items);
    REQUIRE(items.empty());
}
