#include "Core/FileLogSink.h"
#include "Core/Logger.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using apex::core::FileLogSink;
using apex::core::Logger;
using apex::core::LogLevel;
using apex::core::LogRecord;
using apex::core::LogSink;

namespace {

struct CapturedRecord {
    LogLevel level;
    std::string message;
};

class CaptureSink final : public LogSink {
public:
    explicit CaptureSink(std::vector<CapturedRecord>& records) : m_records(records) {}

    void write(const LogRecord& record) override {
        m_records.push_back({record.level, std::string(record.message)});
    }

private:
    std::vector<CapturedRecord>& m_records;
};

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(std::string_view name)
        : m_path(std::filesystem::temp_directory_path() / std::format("ApexTests-{}", name)) {
        std::filesystem::remove_all(m_path);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(stream, line);) {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

TEST_CASE("Log levels have stable names", "[core]") {
    REQUIRE(apex::core::toString(LogLevel::Trace) == "trace");
    REQUIRE(apex::core::toString(LogLevel::Debug) == "debug");
    REQUIRE(apex::core::toString(LogLevel::Info) == "info");
    REQUIRE(apex::core::toString(LogLevel::Warning) == "warning");
    REQUIRE(apex::core::toString(LogLevel::Error) == "error");
}

TEST_CASE("A record formats as UTC time, level and message", "[core]") {
    const std::chrono::sys_days day = std::chrono::year{2026} / std::chrono::November / 19;
    const LogRecord record{LogLevel::Warning, day + 14h + 3min + 22s + 123ms + 456us, "Texture missing"};
    REQUIRE(apex::core::formatRecord(record) == "2026-11-19 14:03:22.123 [warning] Texture missing");
}

TEST_CASE("The logger formats messages with std::format", "[core]") {
    std::vector<CapturedRecord> records;
    Logger logger;
    logger.addSink(std::make_unique<CaptureSink>(records));

    logger.info("Window {}x{}", 1920, 1080);

    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == LogLevel::Info);
    REQUIRE(records[0].message == "Window 1920x1080");
}

TEST_CASE("Messages below the minimum level are dropped", "[core]") {
    std::vector<CapturedRecord> records;
    Logger logger;
    logger.addSink(std::make_unique<CaptureSink>(records));

    REQUIRE(logger.minimumLevel() == LogLevel::Info);
    logger.trace("dropped");
    logger.debug("dropped");
    logger.info("kept");
    logger.warning("kept");
    logger.error("kept");
    REQUIRE(records.size() == 3);

    records.clear();
    logger.setMinimumLevel(LogLevel::Error);
    logger.warning("dropped");
    logger.error("kept");
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == LogLevel::Error);
}

TEST_CASE("Every sink receives every message", "[core]") {
    std::vector<CapturedRecord> first;
    std::vector<CapturedRecord> second;
    Logger logger;
    logger.addSink(std::make_unique<CaptureSink>(first));
    logger.addSink(std::make_unique<CaptureSink>(second));

    logger.warning("one");
    logger.error("two");

    REQUIRE(first.size() == 2);
    REQUIRE(second.size() == 2);
    REQUIRE(second[1].message == "two");
}

TEST_CASE("Concurrent logging loses and mixes no message", "[core]") {
    constexpr int kThreadCount = 8;
    constexpr int kMessagesPerThread = 1000;

    std::vector<CapturedRecord> records;
    Logger logger;
    logger.addSink(std::make_unique<CaptureSink>(records));

    {
        std::vector<std::jthread> threads;
        for (int thread = 0; thread < kThreadCount; ++thread) {
            threads.emplace_back([&logger, thread] {
                for (int message = 0; message < kMessagesPerThread; ++message) {
                    logger.info("thread {} message {}", thread, message);
                }
            });
        }
    }

    REQUIRE(records.size() == static_cast<std::size_t>(kThreadCount * kMessagesPerThread));
    std::set<std::string> unique;
    for (const CapturedRecord& record : records) {
        unique.insert(record.message);
    }
    REQUIRE(unique.size() == records.size());
    for (int thread = 0; thread < kThreadCount; ++thread) {
        REQUIRE(unique.contains(std::format("thread {} message {}", thread, 0)));
        REQUIRE(unique.contains(std::format("thread {} message {}", thread, kMessagesPerThread - 1)));
    }
}

TEST_CASE("The file sink writes one line per message", "[core]") {
    const TemporaryDirectory directory("FileLogSink");
    const std::filesystem::path path = directory.path() / "Logs" / "apex.log";

    {
        auto sink = FileLogSink::open(path);
        REQUIRE(sink.has_value());
        Logger logger;
        logger.addSink(std::move(sink.value()));
        logger.info("first");
        logger.error("second {}", 2);
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0].ends_with("[info] first"));
    REQUIRE(lines[1].ends_with("[error] second 2"));
}

TEST_CASE("The file sink truncates the previous run", "[core]") {
    const TemporaryDirectory directory("FileLogSinkTruncate");
    const std::filesystem::path path = directory.path() / "apex.log";
    std::filesystem::create_directories(directory.path());
    std::ofstream(path) << "previous run\n";

    {
        auto sink = FileLogSink::open(path);
        REQUIRE(sink.has_value());
        Logger logger;
        logger.addSink(std::move(sink.value()));
        logger.info("new run");
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].ends_with("[info] new run"));
}

TEST_CASE("The file sink reports a path it cannot create", "[core]") {
    const TemporaryDirectory directory("FileLogSinkInvalid");
    std::filesystem::create_directories(directory.path());
    const std::filesystem::path blocker = directory.path() / "blocker";
    std::ofstream(blocker) << "a file, not a directory\n";

    const auto sink = FileLogSink::open(blocker / "apex.log");

    REQUIRE_FALSE(sink.has_value());
    REQUIRE(sink.error().code == apex::core::ErrorCode::IoFailure);
    REQUIRE(sink.error().message.find("blocker") != std::string::npos);
}
