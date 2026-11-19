#include "Debug/InputCapture.h"

#include <variant>

namespace apex::debug {

std::vector<platform::Event> filterCapturedEvents(std::span<const platform::Event> events,
                                                  bool keyboardCaptured, bool mouseCaptured) {
    std::vector<platform::Event> kept;
    kept.reserve(events.size());
    for (const platform::Event& event : events) {
        if (const auto* key = std::get_if<platform::KeyChanged>(&event); key != nullptr) {
            if (keyboardCaptured && key->pressed) {
                continue;
            }
        } else if (std::holds_alternative<platform::MouseMoved>(event) && mouseCaptured) {
            continue;
        }
        kept.push_back(event);
    }
    return kept;
}

} // namespace apex::debug
