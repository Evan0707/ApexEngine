#pragma once

#include "Resources/Handles.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace apex::resources {

// Resources of one type, in reusable slots, each with its state. Handles are generational: a handle to
// a removed resource is stale, even after its slot is reused. Not thread-safe.
template <typename HandleType, typename T>
class ResourcePool {
public:
    // A slot in the Loading state, without a value yet.
    [[nodiscard]] HandleType reserve() {
        std::uint32_t index = 0;
        if (!m_free.empty()) {
            index = m_free.back();
            m_free.pop_back();
        } else {
            index = static_cast<std::uint32_t>(m_slots.size());
            m_slots.push_back(Slot{});
        }
        Slot& slot = m_slots[index];
        slot.live = true;
        slot.state = ResourceState::Loading;
        ++m_size;
        ++m_revision;
        return HandleType{index, slot.generation};
    }

    [[nodiscard]] HandleType add(T value) {
        const HandleType handle = reserve();
        setReady(handle, std::move(value));
        return handle;
    }

    // Ignored for a stale handle.
    void setReady(HandleType handle, T value) {
        if (Slot* slot = live(handle)) {
            slot->value = std::move(value);
            slot->state = ResourceState::Ready;
            ++m_revision;
        }
    }

    // Ignored for a stale handle.
    void setFailed(HandleType handle) {
        if (Slot* slot = live(handle)) {
            slot->value.reset();
            slot->state = ResourceState::Failed;
            ++m_revision;
        }
    }

    // False for a stale handle.
    bool remove(HandleType handle) {
        Slot* slot = live(handle);
        if (slot == nullptr) {
            return false;
        }
        slot->value.reset();
        slot->live = false;
        ++slot->generation;
        m_free.push_back(handle.index);
        --m_size;
        ++m_revision;
        return true;
    }

    // Every handle becomes stale.
    void clear() {
        for (std::uint32_t index = 0; index < m_slots.size(); ++index) {
            remove(HandleType{index, m_slots[index].generation});
        }
    }

    [[nodiscard]] ResourceState state(HandleType handle) const noexcept {
        const Slot* slot = live(handle);
        return slot != nullptr ? slot->state : ResourceState::Failed;
    }

    // Only for a Ready resource.
    [[nodiscard]] const T* find(HandleType handle) const noexcept {
        const Slot* slot = live(handle);
        return slot != nullptr && slot->value ? &*slot->value : nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_size; }

    // Changes on every reserve, state change and removal: a reader can skip a pool that did not change.
    [[nodiscard]] std::uint64_t revision() const noexcept { return m_revision; }

    // function(handle, state, value or nullptr) for every live slot, in slot order.
    template <typename Function>
    void forEach(Function&& function) const {
        for (std::uint32_t index = 0; index < m_slots.size(); ++index) {
            const Slot& slot = m_slots[index];
            if (slot.live) {
                function(HandleType{index, slot.generation}, slot.state, slot.value ? &*slot.value : nullptr);
            }
        }
    }

private:
    struct Slot {
        std::optional<T> value;
        ResourceState state = ResourceState::Loading;
        bool live = false;
        // Starts at 1: a default handle (generation 0) never matches.
        std::uint32_t generation = 1;
    };

    [[nodiscard]] Slot* live(HandleType handle) noexcept {
        return const_cast<Slot*>(std::as_const(*this).live(handle));
    }

    [[nodiscard]] const Slot* live(HandleType handle) const noexcept {
        if (handle.index >= m_slots.size()) {
            return nullptr;
        }
        const Slot& slot = m_slots[handle.index];
        return slot.live && slot.generation == handle.generation ? &slot : nullptr;
    }

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_free;
    std::size_t m_size = 0;
    std::uint64_t m_revision = 0;
};

} // namespace apex::resources
