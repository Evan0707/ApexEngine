#pragma once

#include "Graphics/GpuHandles.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. Stores values in reusable slots and hands out generational
// handles: a handle to a removed value is stale, even after its slot is reused.
template <typename Handle, typename T>
class HandlePool {
public:
    [[nodiscard]] Handle add(T value) {
        std::uint32_t index = 0;
        if (!m_free.empty()) {
            index = m_free.back();
            m_free.pop_back();
        } else {
            index = static_cast<std::uint32_t>(m_slots.size());
            m_slots.push_back(Slot{});
        }
        Slot& slot = m_slots[index];
        slot.value = std::move(value);
        ++m_size;
        return Handle{index, slot.generation};
    }

    // Returns the removed value, for a deferred destruction. Nothing for a stale handle.
    [[nodiscard]] std::optional<T> remove(Handle handle) {
        Slot* slot = live(handle);
        if (slot == nullptr) {
            return std::nullopt;
        }
        std::optional<T> removed = std::exchange(slot->value, std::nullopt);
        ++slot->generation;
        m_free.push_back(handle.index);
        --m_size;
        return removed;
    }

    [[nodiscard]] T* find(Handle handle) noexcept {
        Slot* slot = live(handle);
        return slot != nullptr ? &*slot->value : nullptr;
    }

    [[nodiscard]] const T* find(Handle handle) const noexcept {
        return const_cast<HandlePool*>(this)->find(handle);
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_size; }

    // Every live value, in slot order.
    template <typename Function>
    void forEach(Function&& function) {
        for (Slot& slot : m_slots) {
            if (slot.value) {
                function(*slot.value);
            }
        }
    }

private:
    struct Slot {
        std::optional<T> value;
        // Starts at 1: a default handle (generation 0) never matches.
        std::uint32_t generation = 1;
    };

    [[nodiscard]] Slot* live(Handle handle) noexcept {
        if (handle.index >= m_slots.size()) {
            return nullptr;
        }
        Slot& slot = m_slots[handle.index];
        return slot.value && slot.generation == handle.generation ? &slot : nullptr;
    }

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_free;
    std::size_t m_size = 0;
};

} // namespace apex::graphics
