#pragma once

#include "Core/LogSink.h"

namespace apex::core {

// Warnings and errors go to stderr, everything else to stdout.
class ConsoleLogSink final : public LogSink {
public:
    void write(const LogRecord& record) override;
};

} // namespace apex::core
