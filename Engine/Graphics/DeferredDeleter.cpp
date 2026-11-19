#include "Graphics/DeferredDeleter.h"

#include "Core/Assert.h"

#include <utility>
#include <vector>

namespace apex::graphics {

DeferredDeleter::DeferredDeleter(std::uint32_t framesInFlight) : m_framesInFlight(framesInFlight) {
    APEX_ASSERT(framesInFlight > 0, "At least one frame is in flight");
}

DeferredDeleter::~DeferredDeleter() {
    flushAll();
}

void DeferredDeleter::enqueue(std::move_only_function<void()> destroy) {
    APEX_ASSERT(static_cast<bool>(destroy), "Cannot defer an empty destruction");
    m_pending.push_back({m_frame, std::move(destroy)});
}

void DeferredDeleter::advanceFrame() {
    ++m_frame;
    // Taken out before running: a destruction may enqueue another one, kept for a later frame.
    std::vector<std::move_only_function<void()>> ready;
    while (!m_pending.empty() && m_pending.front().frame + m_framesInFlight <= m_frame) {
        ready.push_back(std::move(m_pending.front().destroy));
        m_pending.pop_front();
    }
    for (std::move_only_function<void()>& destroy : ready) {
        destroy();
    }
}

void DeferredDeleter::flushAll() {
    // A destruction may enqueue another one: loop until nothing is left.
    while (!m_pending.empty()) {
        std::move_only_function<void()> destroy = std::move(m_pending.front().destroy);
        m_pending.pop_front();
        destroy();
    }
}

std::size_t DeferredDeleter::pendingCount() const noexcept {
    return m_pending.size();
}

} // namespace apex::graphics
