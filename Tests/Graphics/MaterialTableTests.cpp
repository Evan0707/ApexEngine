#include "Graphics/MaterialTable.h"
#include "Resources/MaterialData.h"
#include "Resources/ResourcePool.h"

#include <catch2/catch_test_macros.hpp>

using apex::graphics::MaterialTable;
using apex::resources::MaterialData;
using apex::resources::MaterialHandle;
using apex::resources::ResourcePool;

namespace {

using MaterialPool = ResourcePool<MaterialHandle, MaterialData>;

[[nodiscard]] MaterialData red() {
    return MaterialData{
        .baseColorFactor = {1.0f, 0.0f, 0.0f, 1.0f}, .metallicFactor = 0.0f, .roughnessFactor = 0.3f};
}

} // namespace

TEST_CASE("The table starts with the default material at index 0", "[graphics]") {
    const MaterialTable table(8);
    REQUIRE(table.gpuData().size() == 1);
    REQUIRE(table.gpuData()[0].baseColor == MaterialData{}.baseColorFactor);
    REQUIRE(table.gpuData()[0].roughness == MaterialData{}.roughnessFactor);
}

TEST_CASE("A ready material of the cache gets its own entry", "[graphics]") {
    MaterialPool pool;
    MaterialTable table(8);
    const MaterialHandle handle = pool.add(red());
    REQUIRE(table.sync(pool) == 0);
    const std::uint32_t index = table.gpuIndex(handle);
    REQUIRE(index == handle.index + 1);
    REQUIRE(table.gpuData()[index].baseColor == glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(table.gpuData()[index].roughness == 0.3f);
}

TEST_CASE("Materials not ready, failed, stale or default give the default entry", "[graphics]") {
    MaterialPool pool;
    MaterialTable table(8);
    const MaterialHandle loading = pool.reserve();
    const MaterialHandle failed = pool.reserve();
    pool.setFailed(failed);
    const MaterialHandle removed = pool.add(red());
    (void)pool.remove(removed);
    table.sync(pool);
    REQUIRE(table.gpuIndex(loading) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(failed) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(removed) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(MaterialHandle{}) == MaterialTable::kDefaultIndex);
}

TEST_CASE("A reused cache slot does not revive the old material", "[graphics]") {
    MaterialPool pool;
    MaterialTable table(8);
    const MaterialHandle first = pool.add(red());
    table.sync(pool);
    (void)pool.remove(first);
    const MaterialHandle second = pool.add(MaterialData{.metallicFactor = 1.0f});
    table.sync(pool);
    REQUIRE(table.gpuIndex(first) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(second) == second.index + 1);
    REQUIRE(table.gpuData()[second.index + 1].metallic == 1.0f);
}

TEST_CASE("Materials beyond the capacity get the default entry", "[graphics]") {
    MaterialPool pool;
    MaterialTable table(3);
    const MaterialHandle a = pool.add(red());
    const MaterialHandle b = pool.add(red());
    const MaterialHandle c = pool.add(red());
    REQUIRE(table.sync(pool) == 1);
    REQUIRE(table.gpuIndex(a) != MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(b) != MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuIndex(c) == MaterialTable::kDefaultIndex);
    REQUIRE(table.gpuData().size() == 3);
}
