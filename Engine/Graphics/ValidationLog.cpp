#include "Graphics/ValidationLog.h"

namespace apex::graphics {

ValidationLog::ValidationLog(core::Logger& logger) noexcept : m_logger(&logger) {}

void ValidationLog::report(ValidationSeverity severity, std::string_view message) {
    switch (severity) {
    case ValidationSeverity::Info:
        m_logger->debug("[vulkan] {}", message);
        return;
    case ValidationSeverity::Warning:
        m_logger->warning("[vulkan] {}", message);
        break;
    case ValidationSeverity::Error:
        m_logger->error("[vulkan] {}", message);
        break;
    }
    m_messageCount.fetch_add(1, std::memory_order_relaxed);
}

std::uint32_t ValidationLog::messageCount() const noexcept {
    return m_messageCount.load(std::memory_order_relaxed);
}

} // namespace apex::graphics
