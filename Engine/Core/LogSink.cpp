#include "Core/LogSink.h"

#include <format>

namespace apex::core {

std::string_view toString(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warning:
        return "warning";
    case LogLevel::Error:
        return "error";
    }
    return "unknown";
}

std::string formatRecord(const LogRecord& record) {
    const auto time = std::chrono::floor<std::chrono::milliseconds>(record.time);
    return std::format("{:%F %T} [{}] {}", time, toString(record.level), record.message);
}

} // namespace apex::core
