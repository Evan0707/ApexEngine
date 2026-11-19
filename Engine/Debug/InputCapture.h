#pragma once

#include "Platform/Event.h"

#include <span>
#include <vector>

namespace apex::debug {

// Removes from the events what the debug interface consumes, so that the game does not react to it.
// Keyboard captured: key presses are removed, key releases are kept (a key held before the capture must
// still be released). Mouse captured: mouse motion is removed. Window events are always kept.
[[nodiscard]] std::vector<platform::Event> filterCapturedEvents(std::span<const platform::Event> events,
                                                                bool keyboardCaptured, bool mouseCaptured);

} // namespace apex::debug
