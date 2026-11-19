#include "Debug/LogHistorySink.h"

namespace apex::debug {

LogHistorySink::LogHistorySink(LogHistory& history) noexcept : m_history(&history) {}

void LogHistorySink::write(const core::LogRecord& record) {
    m_history->add(record);
}

} // namespace apex::debug
