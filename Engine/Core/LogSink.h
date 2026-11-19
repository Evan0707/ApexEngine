#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace apex::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
};

[[nodiscard]] std::string_view toString(LogLevel level) noexcept;

struct LogRecord {
    LogLevel level;
    std::chrono::system_clock::time_point time;
    // Only valid during the call to LogSink::write.
    std::string_view message;
};

// "2026-11-19 14:03:22.123 [warning] message", time in UTC, no trailing newline.
[[nodiscard]] std::string formatRecord(const LogRecord& record);

class LogSink {
public:
    virtual ~LogSink() = default;

    // Called under the logger's lock: never from two threads at once.
    virtual void write(const LogRecord& record) = 0;
};

} // namespace apex::core
