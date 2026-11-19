#include "Platform/Window.h"

#include "Core/Assert.h"
#include "Platform/Key.h"

#include <SDL3/SDL.h>
// Declares SDL_SetMainReady() only: Game keeps its standard main (decisions 0002 and 0005).
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace apex::platform {

namespace {

struct ScancodePair {
    Key key;
    SDL_Scancode scancode;
};

// Every named Key must equal its SDL scancode: events are converted by a plain cast.
constexpr std::array kScancodePairs{
    ScancodePair{Key::A, SDL_SCANCODE_A},
    ScancodePair{Key::B, SDL_SCANCODE_B},
    ScancodePair{Key::C, SDL_SCANCODE_C},
    ScancodePair{Key::D, SDL_SCANCODE_D},
    ScancodePair{Key::E, SDL_SCANCODE_E},
    ScancodePair{Key::F, SDL_SCANCODE_F},
    ScancodePair{Key::G, SDL_SCANCODE_G},
    ScancodePair{Key::H, SDL_SCANCODE_H},
    ScancodePair{Key::I, SDL_SCANCODE_I},
    ScancodePair{Key::J, SDL_SCANCODE_J},
    ScancodePair{Key::K, SDL_SCANCODE_K},
    ScancodePair{Key::L, SDL_SCANCODE_L},
    ScancodePair{Key::M, SDL_SCANCODE_M},
    ScancodePair{Key::N, SDL_SCANCODE_N},
    ScancodePair{Key::O, SDL_SCANCODE_O},
    ScancodePair{Key::P, SDL_SCANCODE_P},
    ScancodePair{Key::Q, SDL_SCANCODE_Q},
    ScancodePair{Key::R, SDL_SCANCODE_R},
    ScancodePair{Key::S, SDL_SCANCODE_S},
    ScancodePair{Key::T, SDL_SCANCODE_T},
    ScancodePair{Key::U, SDL_SCANCODE_U},
    ScancodePair{Key::V, SDL_SCANCODE_V},
    ScancodePair{Key::W, SDL_SCANCODE_W},
    ScancodePair{Key::X, SDL_SCANCODE_X},
    ScancodePair{Key::Y, SDL_SCANCODE_Y},
    ScancodePair{Key::Z, SDL_SCANCODE_Z},
    ScancodePair{Key::Digit1, SDL_SCANCODE_1},
    ScancodePair{Key::Digit2, SDL_SCANCODE_2},
    ScancodePair{Key::Digit3, SDL_SCANCODE_3},
    ScancodePair{Key::Digit4, SDL_SCANCODE_4},
    ScancodePair{Key::Digit5, SDL_SCANCODE_5},
    ScancodePair{Key::Digit6, SDL_SCANCODE_6},
    ScancodePair{Key::Digit7, SDL_SCANCODE_7},
    ScancodePair{Key::Digit8, SDL_SCANCODE_8},
    ScancodePair{Key::Digit9, SDL_SCANCODE_9},
    ScancodePair{Key::Digit0, SDL_SCANCODE_0},
    ScancodePair{Key::Return, SDL_SCANCODE_RETURN},
    ScancodePair{Key::Escape, SDL_SCANCODE_ESCAPE},
    ScancodePair{Key::Backspace, SDL_SCANCODE_BACKSPACE},
    ScancodePair{Key::Tab, SDL_SCANCODE_TAB},
    ScancodePair{Key::Space, SDL_SCANCODE_SPACE},
    ScancodePair{Key::F1, SDL_SCANCODE_F1},
    ScancodePair{Key::F2, SDL_SCANCODE_F2},
    ScancodePair{Key::F3, SDL_SCANCODE_F3},
    ScancodePair{Key::F4, SDL_SCANCODE_F4},
    ScancodePair{Key::F5, SDL_SCANCODE_F5},
    ScancodePair{Key::F6, SDL_SCANCODE_F6},
    ScancodePair{Key::F7, SDL_SCANCODE_F7},
    ScancodePair{Key::F8, SDL_SCANCODE_F8},
    ScancodePair{Key::F9, SDL_SCANCODE_F9},
    ScancodePair{Key::F10, SDL_SCANCODE_F10},
    ScancodePair{Key::F11, SDL_SCANCODE_F11},
    ScancodePair{Key::F12, SDL_SCANCODE_F12},
    ScancodePair{Key::Right, SDL_SCANCODE_RIGHT},
    ScancodePair{Key::Left, SDL_SCANCODE_LEFT},
    ScancodePair{Key::Down, SDL_SCANCODE_DOWN},
    ScancodePair{Key::Up, SDL_SCANCODE_UP},
    ScancodePair{Key::LeftCtrl, SDL_SCANCODE_LCTRL},
    ScancodePair{Key::LeftShift, SDL_SCANCODE_LSHIFT},
    ScancodePair{Key::LeftAlt, SDL_SCANCODE_LALT},
    ScancodePair{Key::RightCtrl, SDL_SCANCODE_RCTRL},
    ScancodePair{Key::RightShift, SDL_SCANCODE_RSHIFT},
    ScancodePair{Key::RightAlt, SDL_SCANCODE_RALT},
};

// Must match the number of named keys in Key.h.
static_assert(kScancodePairs.size() == 63);
static_assert(std::ranges::all_of(kScancodePairs, [](const ScancodePair& pair) {
    return static_cast<int>(std::to_underlying(pair.key)) == static_cast<int>(pair.scancode);
}));

[[nodiscard]] std::optional<Key> keyFromScancode(SDL_Scancode scancode) noexcept {
    const auto value = static_cast<int>(scancode);
    if (value <= 0 || value > std::numeric_limits<std::underlying_type_t<Key>>::max()) {
        return std::nullopt;
    }
    const auto key = static_cast<Key>(value);
    if (keyName(key) == "Unknown") {
        return std::nullopt;
    }
    return key;
}

[[nodiscard]] bool fitsInInt(std::uint32_t value) noexcept {
    return value > 0 && value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max());
}

[[nodiscard]] std::uint32_t toPixelCount(Sint32 value) noexcept {
    return static_cast<std::uint32_t>(std::max<Sint32>(value, 0));
}

} // namespace

core::Result<Window> Window::create(const WindowConfig& config, core::Logger& logger) {
    if (!fitsInInt(config.width) || !fitsInInt(config.height)) {
        auto message = std::format("Invalid window size {}x{}", config.width, config.height);
        logger.error("{}", message);
        return core::makeError(core::ErrorCode::InvalidArgument, std::move(message));
    }

    SDL_SetMainReady();
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        auto message = std::format("Cannot initialize the SDL video subsystem: {}", SDL_GetError());
        logger.error("{}", message);
        return core::makeError(core::ErrorCode::PlatformFailure, std::move(message));
    }

    const SDL_WindowFlags flags = config.resizable ? SDL_WINDOW_RESIZABLE : 0;
    SDL_Window* window = SDL_CreateWindow(config.title.c_str(), static_cast<int>(config.width),
                                          static_cast<int>(config.height), flags);
    if (window == nullptr) {
        auto message = std::format("Cannot create the window: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        logger.error("{}", message);
        return core::makeError(core::ErrorCode::PlatformFailure, std::move(message));
    }

    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight)) {
        logger.warning("Cannot read the window size in pixels: {}", SDL_GetError());
    }
    logger.info("Window created: {}x{} requested, {}x{} pixels", config.width, config.height, pixelWidth,
                pixelHeight);
    return Window(window, logger);
}

Window::Window(SDL_Window* window, core::Logger& logger) noexcept : m_window(window), m_logger(&logger) {}

Window::~Window() {
    destroy();
}

Window::Window(Window&& other) noexcept
    : m_window(std::exchange(other.m_window, nullptr)), m_logger(other.m_logger),
      m_events(std::move(other.m_events)), m_minimized(other.m_minimized),
      m_cursorCaptured(other.m_cursorCaptured) {}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();
        m_window = std::exchange(other.m_window, nullptr);
        m_logger = other.m_logger;
        m_events = std::move(other.m_events);
        m_minimized = other.m_minimized;
        m_cursorCaptured = other.m_cursorCaptured;
    }
    return *this;
}

void Window::destroy() noexcept {
    if (m_window == nullptr) {
        return;
    }
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

std::span<const Event> Window::pollEvents() {
    APEX_ASSERT(m_window != nullptr, "pollEvents called on a moved-from Window");
    m_events.clear();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            m_events.emplace_back(CloseRequested{});
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            m_events.emplace_back(
                WindowResized{toPixelCount(event.window.data1), toPixelCount(event.window.data2)});
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            m_minimized = true;
            m_events.emplace_back(WindowMinimized{});
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            // Also sent when leaving the maximized state: only the end of a minimization is reported.
            if (m_minimized) {
                m_minimized = false;
                m_events.emplace_back(WindowRestored{});
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            m_events.emplace_back(FocusLost{});
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (const auto key = keyFromScancode(event.key.scancode)) {
                m_events.emplace_back(KeyChanged{*key, event.key.down, event.key.repeat});
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            m_events.emplace_back(MouseMoved{event.motion.xrel, event.motion.yrel});
            break;
        default:
            break;
        }
    }
    return m_events;
}

bool Window::isMinimized() const noexcept {
    return m_minimized;
}

void Window::setCursorCaptured(bool captured) {
    APEX_ASSERT(m_window != nullptr, "setCursorCaptured called on a moved-from Window");
    if (captured == m_cursorCaptured) {
        return;
    }
    if (!SDL_SetWindowRelativeMouseMode(m_window, captured)) {
        m_logger->warning("Cannot {} the cursor: {}", captured ? "capture" : "release", SDL_GetError());
        return;
    }
    m_cursorCaptured = captured;
}

bool Window::isCursorCaptured() const noexcept {
    return m_cursorCaptured;
}

} // namespace apex::platform
