#include "Resources/Handles.h"
#include "Resources/ResourcePool.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace apex::resources;

TEST_CASE("A reserved resource is loading until it is ready or failed", "[resources]") {
    ResourcePool<TextureHandle, std::string> pool;
    const TextureHandle ready = pool.reserve();
    const TextureHandle failed = pool.reserve();
    REQUIRE(pool.state(ready) == ResourceState::Loading);
    REQUIRE(pool.find(ready) == nullptr);

    pool.setReady(ready, "pixels");
    pool.setFailed(failed);
    REQUIRE(pool.state(ready) == ResourceState::Ready);
    REQUIRE(*pool.find(ready) == "pixels");
    REQUIRE(pool.state(failed) == ResourceState::Failed);
    REQUIRE(pool.find(failed) == nullptr);
    REQUIRE(pool.size() == 2);
}

TEST_CASE("Stale and default handles read as failed", "[resources]") {
    ResourcePool<MeshHandle, int> pool;
    const MeshHandle handle = pool.add(7);
    REQUIRE(pool.remove(handle));
    REQUIRE_FALSE(pool.remove(handle));
    REQUIRE(pool.state(handle) == ResourceState::Failed);
    REQUIRE(pool.find(handle) == nullptr);
    REQUIRE(pool.state(MeshHandle{}) == ResourceState::Failed);
}

TEST_CASE("A reused slot does not revive a stale resource handle", "[resources]") {
    ResourcePool<MeshHandle, int> pool;
    const MeshHandle first = pool.add(1);
    (void)pool.remove(first);
    const MeshHandle second = pool.add(2);
    REQUIRE(second.index == first.index);
    REQUIRE(pool.find(first) == nullptr);
    REQUIRE(*pool.find(second) == 2);
    // Setting a stale handle does not touch the new resource.
    pool.setFailed(first);
    REQUIRE(pool.state(second) == ResourceState::Ready);
}

TEST_CASE("clear makes every resource handle stale", "[resources]") {
    ResourcePool<ModelHandle, int> pool;
    const ModelHandle a = pool.add(1);
    const ModelHandle b = pool.reserve();
    pool.clear();
    REQUIRE(pool.size() == 0);
    REQUIRE(pool.find(a) == nullptr);
    REQUIRE(pool.state(b) == ResourceState::Failed);
}

TEST_CASE("The revision changes with every change of the pool", "[resources]") {
    ResourcePool<MeshHandle, int> pool;
    const auto start = pool.revision();
    const MeshHandle handle = pool.reserve();
    const auto reserved = pool.revision();
    REQUIRE(reserved != start);
    pool.setReady(handle, 3);
    REQUIRE(pool.revision() != reserved);
    const auto ready = pool.revision();
    (void)pool.find(handle);
    REQUIRE(pool.revision() == ready);
}

TEST_CASE("forEach visits the live resources with their state", "[resources]") {
    ResourcePool<MeshHandle, int> pool;
    (void)pool.add(1);
    const MeshHandle removed = pool.add(2);
    const MeshHandle loading = pool.reserve();
    (void)pool.remove(removed);
    std::vector<ResourceState> states;
    std::vector<MeshHandle> handles;
    pool.forEach([&](MeshHandle handle, ResourceState state, const int* value) {
        handles.push_back(handle);
        states.push_back(state);
        REQUIRE((value != nullptr) == (state == ResourceState::Ready));
    });
    REQUIRE(states == std::vector<ResourceState>{ResourceState::Ready, ResourceState::Loading});
    REQUIRE(handles.back() == loading);
}
