#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. Delays the destruction of a GPU object until no frame in flight
// can still use it (spec 5.4).
// Must be destroyed before the device: the destructor runs every pending destruction.
class DeferredDeleter {
public:
    explicit DeferredDeleter(std::uint32_t framesInFlight);
    ~DeferredDeleter();
    DeferredDeleter(const DeferredDeleter&) = delete;
    DeferredDeleter& operator=(const DeferredDeleter&) = delete;

    // Runs after framesInFlight more calls to advanceFrame.
    void enqueue(std::move_only_function<void()> destroy);

    // Called once per frame, after waiting for the fence of the frame slot about to be reused: the frame
    // submitted framesInFlight frames ago has finished on the GPU. Called again for that slot only after
    // a new submission: a skipped frame must not age the pending destructions.
    void advanceFrame();

    // Runs every pending destruction, oldest first. The caller has waited for the GPU to be idle.
    void flushAll();

    [[nodiscard]] std::size_t pendingCount() const noexcept;

private:
    struct Pending {
        std::uint64_t frame;
        std::move_only_function<void()> destroy;
    };

    std::uint32_t m_framesInFlight;
    std::uint64_t m_frame = 0;
    std::deque<Pending> m_pending;
};

} // namespace apex::graphics
