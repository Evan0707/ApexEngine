#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Platform/Event.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Opaque handle: no SDL header is included here.
struct SDL_Window;

namespace apex::platform {

struct WindowConfig {
    std::string title = "Apex Engine";
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    bool resizable = true;
};

// Owns the SDL video subsystem for its lifetime: one Window per process, created on the main thread.
// The logger must outlive the window.
class Window {
public:
    [[nodiscard]] static core::Result<Window> create(const WindowConfig& config, core::Logger& logger);

    ~Window();
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Drains the system queue. The span stays valid until the next call.
    [[nodiscard]] std::span<const Event> pollEvents();

    [[nodiscard]] bool isMinimized() const noexcept;

    // Captured: relative mouse mode, cursor hidden and confined to the window.
    void setCursorCaptured(bool captured);
    [[nodiscard]] bool isCursorCaptured() const noexcept;

private:
    Window(SDL_Window* window, core::Logger& logger) noexcept;

    void destroy() noexcept;

    SDL_Window* m_window = nullptr;
    core::Logger* m_logger = nullptr;
    std::vector<Event> m_events;
    bool m_minimized = false;
    bool m_cursorCaptured = false;
};

} // namespace apex::platform
