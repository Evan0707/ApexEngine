#pragma once

#include "Core/LogSink.h"

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace apex::debug {

struct LogEntry {
    core::LogLevel level;
    std::chrono::system_clock::time_point time;
    std::string message;
};

// Keeps the last records for the debug console (spec 5.9: the last 100 warnings and errors).
class LogHistory {
public:
    explicit LogHistory(std::size_t capacity = 100, core::LogLevel minimumLevel = core::LogLevel::Warning);

    // Records below the minimum level are ignored.
    void add(const core::LogRecord& record);

    // Oldest first. Safe to call from any thread, also while the logger writes.
    [[nodiscard]] std::vector<LogEntry> snapshot() const;
    [[nodiscard]] std::size_t capacity() const noexcept;

private:
    std::size_t m_capacity;
    core::LogLevel m_minimumLevel;
    mutable std::mutex m_mutex;
    std::deque<LogEntry> m_entries;
};

} // namespace apex::debug
