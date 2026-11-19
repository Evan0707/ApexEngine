#include "Core/Logger.h"
#include "Debug/LogHistory.h"
#include "Debug/LogHistorySink.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <format>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using apex::core::Logger;
using apex::core::LogLevel;
using apex::core::LogRecord;
using apex::debug::LogEntry;
using apex::debug::LogHistory;
using apex::debug::LogHistorySink;

namespace {

LogRecord makeRecord(LogLevel level, std::string_view message) {
    return {level, std::chrono::system_clock::now(), message};
}

} // namespace

TEST_CASE("The log history keeps warnings and errors by default", "[debug]") {
    LogHistory history;
    history.add(makeRecord(LogLevel::Info, "info"));
    history.add(makeRecord(LogLevel::Warning, "warning"));
    history.add(makeRecord(LogLevel::Debug, "debug"));
    history.add(makeRecord(LogLevel::Error, "error"));

    const std::vector<LogEntry> entries = history.snapshot();
    REQUIRE(history.capacity() == 100);
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].level == LogLevel::Warning);
    REQUIRE(entries[0].message == "warning");
    REQUIRE(entries[1].level == LogLevel::Error);
    REQUIRE(entries[1].message == "error");
}

TEST_CASE("The log history drops the oldest records beyond its capacity", "[debug]") {
    LogHistory history(3, LogLevel::Trace);
    for (int index = 0; index < 5; ++index) {
        const std::string message = std::format("message {}", index);
        history.add(makeRecord(LogLevel::Info, message));
    }

    const std::vector<LogEntry> entries = history.snapshot();
    REQUIRE(entries.size() == 3);
    REQUIRE(entries[0].message == "message 2");
    REQUIRE(entries[2].message == "message 4");
}

TEST_CASE("The log history copies the message out of the record", "[debug]") {
    LogHistory history;
    {
        std::string message = "temporary";
        history.add(makeRecord(LogLevel::Error, message));
        message = "overwritten";
    }
    REQUIRE(history.snapshot().front().message == "temporary");
}

TEST_CASE("The log history sink receives the logger's records", "[debug]") {
    LogHistory history;
    Logger logger;
    logger.addSink(std::make_unique<LogHistorySink>(history));

    logger.info("not kept");
    logger.warning("missing file {}", "Assets/a.glb");

    const std::vector<LogEntry> entries = history.snapshot();
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].message == "missing file Assets/a.glb");
}

TEST_CASE("The log history can be read while several threads log", "[debug]") {
    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 200;
    LogHistory history(kThreadCount * kMessagesPerThread);
    Logger logger;
    logger.addSink(std::make_unique<LogHistorySink>(history));

    {
        std::vector<std::jthread> threads;
        for (int thread = 0; thread < kThreadCount; ++thread) {
            threads.emplace_back([&logger, thread] {
                for (int message = 0; message < kMessagesPerThread; ++message) {
                    logger.warning("thread {} message {}", thread, message);
                }
            });
        }
        // Read concurrently, as the debug panel does on the main thread.
        for (int read = 0; read < 50; ++read) {
            REQUIRE(history.snapshot().size() <= static_cast<std::size_t>(kThreadCount * kMessagesPerThread));
        }
    }

    const std::vector<LogEntry> entries = history.snapshot();
    REQUIRE(entries.size() == static_cast<std::size_t>(kThreadCount * kMessagesPerThread));
    std::set<std::string> unique;
    for (const LogEntry& entry : entries) {
        unique.insert(entry.message);
    }
    REQUIRE(unique.size() == entries.size());
}
