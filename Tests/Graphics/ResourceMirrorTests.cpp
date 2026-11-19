#include "Graphics/ResourceMirror.h"
#include "Resources/Handles.h"
#include "Resources/ResourcePool.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using apex::graphics::ResourceMirror;
using apex::resources::MeshHandle;
using apex::resources::ResourcePool;

namespace {

// Stands for the GPU side: "uploads" a string, records what was released.
struct Recorder {
    int created = 0;
    std::vector<std::string> released;
    bool failNext = false;

    auto create() {
        return [this](MeshHandle, const std::string& value) -> std::optional<std::string> {
            ++created;
            if (failNext) {
                failNext = false;
                return std::nullopt;
            }
            return "gpu:" + value;
        };
    }
    auto release() {
        return [this](std::string&& value) { released.push_back(std::move(value)); };
    }
};

} // namespace

TEST_CASE("Ready resources are mirrored once", "[graphics]") {
    ResourcePool<MeshHandle, std::string> pool;
    ResourceMirror<MeshHandle, std::string, std::string> mirror;
    Recorder recorder;
    const MeshHandle cube = pool.add("cube");
    const MeshHandle loading = pool.reserve();

    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(*mirror.find(cube) == "gpu:cube");
    REQUIRE(mirror.find(loading) == nullptr);
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(recorder.created == 1);

    pool.setReady(loading, "sphere");
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(*mirror.find(loading) == "gpu:sphere");
    REQUIRE(recorder.created == 2);
}

TEST_CASE("Unloaded resources are released, and their handles find nothing", "[graphics]") {
    ResourcePool<MeshHandle, std::string> pool;
    ResourceMirror<MeshHandle, std::string, std::string> mirror;
    Recorder recorder;
    const MeshHandle cube = pool.add("cube");
    mirror.sync(pool, recorder.create(), recorder.release());

    (void)pool.remove(cube);
    const MeshHandle reused = pool.add("crate");
    REQUIRE(reused.index == cube.index);
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(recorder.released == std::vector<std::string>{"gpu:cube"});
    REQUIRE(mirror.find(cube) == nullptr);
    REQUIRE(*mirror.find(reused) == "gpu:crate");

    pool.clear();
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(recorder.released.back() == "gpu:crate");
    REQUIRE(mirror.find(reused) == nullptr);
}

TEST_CASE("A failed GPU creation is not retried while the resource lives", "[graphics]") {
    ResourcePool<MeshHandle, std::string> pool;
    ResourceMirror<MeshHandle, std::string, std::string> mirror;
    Recorder recorder;
    recorder.failNext = true;
    const MeshHandle broken = pool.add("broken");
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(mirror.find(broken) == nullptr);

    (void)pool.add("other");
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(recorder.created == 2);
    REQUIRE(mirror.find(broken) == nullptr);
}

TEST_CASE("releaseAll releases every entry", "[graphics]") {
    ResourcePool<MeshHandle, std::string> pool;
    ResourceMirror<MeshHandle, std::string, std::string> mirror;
    Recorder recorder;
    (void)pool.add("a");
    (void)pool.add("b");
    mirror.sync(pool, recorder.create(), recorder.release());
    mirror.releaseAll(recorder.release());
    REQUIRE(recorder.released.size() == 2);
    // Synced again from scratch afterwards.
    mirror.sync(pool, recorder.create(), recorder.release());
    REQUIRE(recorder.created == 4);
}
