#pragma once

#include "Core/Logger.h"

#include <atomic>
#include <cstdint>
#include <string_view>

namespace apex::graphics {

enum class ValidationSeverity {
    Info,
    Warning,
    Error,
};

// Receives every message of the Vulkan validation layers (decision 0006). Nothing is filtered.
// Created by the application before the renderer and read after it is destroyed: shutdown counts too.
class ValidationLog {
public:
    // The logger must outlive the validation log.
    explicit ValidationLog(core::Logger& logger) noexcept;
    ValidationLog(const ValidationLog&) = delete;
    ValidationLog& operator=(const ValidationLog&) = delete;

    // Safe to call from any thread: the layers report from the thread that made the call.
    void report(ValidationSeverity severity, std::string_view message);

    // Warnings and errors reported so far. Info messages are logged, not counted.
    [[nodiscard]] std::uint32_t messageCount() const noexcept;

private:
    core::Logger* m_logger;
    std::atomic<std::uint32_t> m_messageCount{0};
};

} // namespace apex::graphics
