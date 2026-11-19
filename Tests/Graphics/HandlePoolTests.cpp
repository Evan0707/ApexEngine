#include "Graphics/GpuHandles.h"
#include "Graphics/HandlePool.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using apex::graphics::HandlePool;
using apex::graphics::MeshHandle;

TEST_CASE("A handle finds the value it was given for", "[graphics]") {
    HandlePool<MeshHandle, std::string> pool;
    const MeshHandle cube = pool.add("cube");
    const MeshHandle sphere = pool.add("sphere");
    REQUIRE(pool.size() == 2);
    REQUIRE(*pool.find(cube) == "cube");
    REQUIRE(*pool.find(sphere) == "sphere");
}

TEST_CASE("A default handle finds nothing", "[graphics]") {
    HandlePool<MeshHandle, int> pool;
    (void)pool.add(1);
    REQUIRE(pool.find(MeshHandle{}) == nullptr);
    REQUIRE(pool.find(MeshHandle{0, 0}) == nullptr);
}

TEST_CASE("Removing returns the value and makes the handle stale", "[graphics]") {
    HandlePool<MeshHandle, int> pool;
    const MeshHandle handle = pool.add(42);
    REQUIRE(pool.remove(handle) == 42);
    REQUIRE(pool.find(handle) == nullptr);
    REQUIRE_FALSE(pool.remove(handle));
    REQUIRE(pool.size() == 0);
}

TEST_CASE("A reused slot does not revive a stale handle", "[graphics]") {
    HandlePool<MeshHandle, int> pool;
    const MeshHandle first = pool.add(1);
    (void)pool.remove(first);
    const MeshHandle second = pool.add(2);
    REQUIRE(second.index == first.index);
    REQUIRE(second.generation != first.generation);
    REQUIRE(pool.find(first) == nullptr);
    REQUIRE(*pool.find(second) == 2);
}

TEST_CASE("forEach visits the live values in slot order", "[graphics]") {
    HandlePool<MeshHandle, int> pool;
    (void)pool.add(1);
    const MeshHandle removed = pool.add(2);
    (void)pool.add(3);
    (void)pool.remove(removed);
    std::vector<int> visited;
    pool.forEach([&](int value) { visited.push_back(value); });
    REQUIRE(visited == std::vector<int>{1, 3});
}
