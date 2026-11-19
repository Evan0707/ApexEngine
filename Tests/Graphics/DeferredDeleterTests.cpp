#include "Graphics/DeferredDeleter.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using apex::graphics::DeferredDeleter;

TEST_CASE("A deferred destruction runs after as many frames as there are frames in flight", "[graphics]") {
    DeferredDeleter deleter(2);
    int destroyed = 0;
    deleter.enqueue([&destroyed] { ++destroyed; });

    REQUIRE(deleter.pendingCount() == 1);
    deleter.advanceFrame();
    REQUIRE(destroyed == 0);
    deleter.advanceFrame();
    REQUIRE(destroyed == 1);
    REQUIRE(deleter.pendingCount() == 0);
    deleter.advanceFrame();
    REQUIRE(destroyed == 1);
}

TEST_CASE("Deferred destructions keep the frame they were enqueued in", "[graphics]") {
    DeferredDeleter deleter(2);
    std::vector<int> order;
    deleter.enqueue([&order] { order.push_back(1); });
    deleter.advanceFrame();
    deleter.enqueue([&order] { order.push_back(2); });
    deleter.enqueue([&order] { order.push_back(3); });

    deleter.advanceFrame();
    REQUIRE(order == std::vector<int>{1});
    deleter.advanceFrame();
    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("flushAll runs every pending destruction, oldest first", "[graphics]") {
    DeferredDeleter deleter(2);
    std::vector<int> order;
    deleter.enqueue([&order] { order.push_back(1); });
    deleter.advanceFrame();
    deleter.enqueue([&order] { order.push_back(2); });

    deleter.flushAll();
    REQUIRE(order == std::vector<int>{1, 2});
    REQUIRE(deleter.pendingCount() == 0);
}

TEST_CASE("A destruction enqueued by a destruction waits for later frames", "[graphics]") {
    DeferredDeleter deleter(1);
    std::vector<int> order;
    deleter.enqueue([&] {
        order.push_back(1);
        deleter.enqueue([&order] { order.push_back(2); });
    });

    deleter.advanceFrame();
    REQUIRE(order == std::vector<int>{1});
    REQUIRE(deleter.pendingCount() == 1);
    deleter.advanceFrame();
    REQUIRE(order == std::vector<int>{1, 2});
}

TEST_CASE("flushAll also runs destructions enqueued while flushing", "[graphics]") {
    DeferredDeleter deleter(2);
    std::vector<int> order;
    deleter.enqueue([&] {
        order.push_back(1);
        deleter.enqueue([&order] { order.push_back(2); });
    });

    deleter.flushAll();
    REQUIRE(order == std::vector<int>{1, 2});
}

TEST_CASE("The deferred deleter runs what is left when destroyed", "[graphics]") {
    int destroyed = 0;
    {
        DeferredDeleter deleter(2);
        deleter.enqueue([&destroyed] { ++destroyed; });
    }
    REQUIRE(destroyed == 1);
}
