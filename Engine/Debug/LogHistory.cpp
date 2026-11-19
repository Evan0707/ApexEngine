#include "Debug/LogHistory.h"

#include "Core/Assert.h"

#include <utility>

namespace apex::debug {

LogHistory::LogHistory(std::size_t capacity, core::LogLevel minimumLevel)
    : m_capacity(capacity), m_minimumLevel(minimumLevel) {
    APEX_ASSERT(capacity > 0, "A log history keeps at least one record");
}

void LogHistory::add(const core::LogRecord& record) {
    if (record.level < m_minimumLevel) {
        return;
    }
    // The message is copied: the record's view dies with the call to LogSink::write.
    LogEntry entry{record.level, record.time, std::string(record.message)};
    const std::scoped_lock lock(m_mutex);
    if (m_entries.size() == m_capacity) {
        m_entries.pop_front();
    }
    m_entries.push_back(std::move(entry));
}

std::vector<LogEntry> LogHistory::snapshot() const {
    const std::scoped_lock lock(m_mutex);
    return {m_entries.begin(), m_entries.end()};
}

std::size_t LogHistory::capacity() const noexcept {
    return m_capacity;
}

} // namespace apex::debug
