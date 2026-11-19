#include "Core/Logger.h"
#include "Graphics/ValidationLog.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <thread>
#include <vector>

using apex::core::Logger;
using apex::core::LogLevel;
using apex::core::LogRecord;
using apex::core::LogSink;
using apex::graphics::ValidationLog;
using apex::graphics::ValidationSeverity;

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

} // namespace

TEST_CASE("Validation warnings and errors are logged and counted", "[graphics]") {
    std::vector<CapturedRecord> records;
    Logger logger;
    logger.setMinimumLevel(LogLevel::Trace);
    logger.addSink(std::make_unique<CaptureSink>(records));
    ValidationLog validation(logger);

    validation.report(ValidationSeverity::Warning, "first");
    validation.report(ValidationSeverity::Error, "second");

    REQUIRE(validation.messageCount() == 2);
    REQUIRE(records.size() == 2);
    REQUIRE(records[0].level == LogLevel::Warning);
    REQUIRE(records[0].message == "[vulkan] first");
    REQUIRE(records[1].level == LogLevel::Error);
    REQUIRE(records[1].message == "[vulkan] second");
}

TEST_CASE("Validation info messages are logged at debug level and not counted", "[graphics]") {
    std::vector<CapturedRecord> records;
    Logger logger;
    logger.setMinimumLevel(LogLevel::Trace);
    logger.addSink(std::make_unique<CaptureSink>(records));
    ValidationLog validation(logger);

    validation.report(ValidationSeverity::Info, "loader info");

    REQUIRE(validation.messageCount() == 0);
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == LogLevel::Debug);
}

TEST_CASE("Validation messages are counted even when the logger filters them", "[graphics]") {
    Logger logger;
    logger.setMinimumLevel(LogLevel::Error);
    ValidationLog validation(logger);

    validation.report(ValidationSeverity::Warning, "filtered by the logger");

    REQUIRE(validation.messageCount() == 1);
}

TEST_CASE("Validation messages can be reported from several threads", "[graphics]") {
    constexpr int kThreadCount = 8;
    constexpr int kMessagesPerThread = 250;
    Logger logger;
    logger.setMinimumLevel(LogLevel::Error);
    ValidationLog validation(logger);

    {
        std::vector<std::jthread> threads;
        for (int thread = 0; thread < kThreadCount; ++thread) {
            threads.emplace_back([&validation] {
                for (int message = 0; message < kMessagesPerThread; ++message) {
                    validation.report(ValidationSeverity::Warning, "message");
                }
            });
        }
    }

    REQUIRE(validation.messageCount() == kThreadCount * kMessagesPerThread);
}
