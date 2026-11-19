#pragma once

#include "Core/LogSink.h"
#include "Debug/LogHistory.h"

namespace apex::debug {

// The logger owns its sinks: this one only forwards to a history owned by the application.
class LogHistorySink final : public core::LogSink {
public:
    // The history must outlive the logger that owns this sink.
    explicit LogHistorySink(LogHistory& history) noexcept;

    void write(const core::LogRecord& record) override;

private:
    LogHistory* m_history;
};

} // namespace apex::debug
