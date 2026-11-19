#include "Graphics/MaterialTable.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

using apex::graphics::MaterialHandle;
using apex::graphics::MaterialTable;
using apex::resources::MaterialData;

namespace {

[[nodiscard]] MaterialData red() {
    return MaterialData{
        .baseColorFactor = {1.0f, 0.0f, 0.0f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.3f};
}

} // namespace

TEST_CASE("The table starts with the default material at index 0", "[graphics]") {
    const MaterialTable table(8);
    REQUIRE(table.size() == 0);
    REQUIRE(table.gpuData().size() == 1);
    REQUIRE(table.gpuData()[0].baseColor == MaterialData{}.baseColorFactor);
    REQUIRE(table.gpuData()[0].roughness == MaterialData{}.roughnessFactor);
}

TEST_CASE("An added material gets its own index and GPU data", "[graphics]") {
    MaterialTable table(8);
    const auto handle = table.add(red());
    REQUIRE(handle);
    REQUIRE(table.size() == 1);
    const std::uint32_t index = table.gpuIndex(*handle);
    REQUIRE(index != MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuData()[index].baseColor == glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(table.gpuData()[index].roughness == 0.3f);
}

TEST_CASE("Stale and default handles give the default material", "[graphics]") {
    MaterialTable table(8);
    const MaterialHandle handle = *table.add(red());
    REQUIRE(table.remove(handle));
    REQUIRE_FALSE(table.remove(handle));
    REQUIRE(table.gpuIndex(handle) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(MaterialHandle{}) == MaterialTable::kDefaultIndex);
    REQUIRE(table.size() == 0);
}

TEST_CASE("A removed slot is reused without reviving the old handle", "[graphics]") {
    MaterialTable table(8);
    const MaterialHandle first = *table.add(red());
    REQUIRE(table.remove(first));
    const MaterialHandle second = *table.add(MaterialData{.metallicFactor = 1.0f});
    REQUIRE(second.index == first.index);
    REQUIRE(table.gpuIndex(first) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(second) == second.index);
    REQUIRE(table.gpuData()[second.index].metallic == 1.0f);
}

TEST_CASE("A full table refuses a new material", "[graphics]") {
    MaterialTable table(3);
    REQUIRE(table.add(red()));
    REQUIRE(table.add(red()));
    REQUIRE_FALSE(table.add(red()));
}

TEST_CASE("Factors outside [0, 1] are refused", "[graphics]") {
    MaterialTable table(8);
    REQUIRE_FALSE(table.add(MaterialData{.metallicFactor = 1.5f}));
    REQUIRE_FALSE(table.add(MaterialData{.roughnessFactor = -0.1f}));
    REQUIRE_FALSE(table.add(MaterialData{.roughnessFactor = std::numeric_limits<float>::quiet_NaN()}));
    REQUIRE(table.size() == 0);
}
