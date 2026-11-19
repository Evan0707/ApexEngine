#pragma once

#include "Core/LogSink.h"

#include <atomic>
#include <format>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace apex::core {

// Passed by reference to every system that logs (G-06). Safe to use from several threads.
class Logger {
public:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void addSink(std::unique_ptr<LogSink> sink);

    void setMinimumLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel minimumLevel() const noexcept;

    template <typename... Args>
    void log(LogLevel level, std::format_string<Args...> format, Args&&... args) {
        if (level >= minimumLevel()) {
            write(level, std::format(format, std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    void trace(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Trace, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Debug, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Info, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Warning, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::format_string<Args...> format, Args&&... args) {
        log(LogLevel::Error, format, std::forward<Args>(args)...);
    }

private:
    void write(LogLevel level, std::string_view message);

    std::mutex m_mutex;
    std::vector<std::unique_ptr<LogSink>> m_sinks;
    std::atomic<LogLevel> m_minimumLevel{LogLevel::Info};
};

} // namespace apex::core
