#pragma once

#include "Platform/Key.h"

#include <cstdint>
#include <variant>

namespace apex::platform {

struct CloseRequested {};

// Size of the drawable area, in pixels.
struct WindowResized {
    std::uint32_t width;
    std::uint32_t height;
};

struct WindowMinimized {};

struct WindowRestored {};

struct FocusLost {};

struct KeyChanged {
    Key key;
    bool pressed;
    bool repeat;
};

// Relative motion in pixels since the previous event, +Y downwards as reported by the system.
struct MouseMoved {
    float deltaX;
    float deltaY;
};

using Event = std::variant<CloseRequested, WindowResized, WindowMinimized, WindowRestored, FocusLost,
                           KeyChanged, MouseMoved>;

} // namespace apex::platform
