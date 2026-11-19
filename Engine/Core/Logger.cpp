#include "Core/Logger.h"

#include "Core/Assert.h"

#include <chrono>

namespace apex::core {

void Logger::addSink(std::unique_ptr<LogSink> sink) {
    APEX_ASSERT(sink != nullptr, "Logger::addSink needs a sink");
    const std::scoped_lock lock(m_mutex);
    m_sinks.push_back(std::move(sink));
}

void Logger::setMinimumLevel(LogLevel level) noexcept {
    m_minimumLevel.store(level, std::memory_order_relaxed);
}

LogLevel Logger::minimumLevel() const noexcept {
    return m_minimumLevel.load(std::memory_order_relaxed);
}

void Logger::write(LogLevel level, std::string_view message) {
    const LogRecord record{level, std::chrono::system_clock::now(), message};
    const std::scoped_lock lock(m_mutex);
    for (const std::unique_ptr<LogSink>& sink : m_sinks) {
        sink->write(record);
    }
}

} // namespace apex::core
