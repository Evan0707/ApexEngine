#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Platform/Event.h"

#include <cstdint>
#include <functional>
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

struct PixelSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] bool operator==(const PixelSize&) const noexcept = default;
};

// Receives a const SDL_Event*, valid during the call only (decision 0009).
using NativeEventObserver = std::function<void(const void* nativeEvent)>;

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

    // Size of the drawable area. {0, 0} while minimized, or when the system cannot report it.
    [[nodiscard]] PixelSize sizeInPixels() const noexcept;

    // Captured: relative mouse mode, cursor hidden and confined to the window.
    void setCursorCaptured(bool captured);
    [[nodiscard]] bool isCursorCaptured() const noexcept;

    // Bridge to Graphics, through opaque handles only (decision 0009).
    // Instance extensions required to present to this window. The names are owned by SDL.
    [[nodiscard]] core::Result<std::span<const char* const>> vulkanInstanceExtensions() const;
    // Takes a VkInstance, returns a VkSurfaceKHR. The caller destroys the surface before the instance.
    [[nodiscard]] core::Result<std::uint64_t> createVulkanSurface(void* vulkanInstance);

    // Bridge to the ImGui backend of Debug (decision 0009).
    // The SDL_Window*.
    [[nodiscard]] void* nativeHandle() const noexcept;
    // Called by pollEvents for every system event, before its translation. Empty to remove.
    void setNativeEventObserver(NativeEventObserver observer);

private:
    Window(SDL_Window* window, core::Logger& logger) noexcept;

    void destroy() noexcept;

    SDL_Window* m_window = nullptr;
    core::Logger* m_logger = nullptr;
    std::vector<Event> m_events;
    NativeEventObserver m_nativeEventObserver;
    bool m_minimized = false;
    bool m_cursorCaptured = false;
};

} // namespace apex::platform
