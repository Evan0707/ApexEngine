#pragma once

#include "Input/ActionMap.h"

namespace apex::game {

// Spec 5.2 defaults, as physical key names (AZERTY Z is "W"). Replaced by Config/input.json in phase 5.
[[nodiscard]] input::InputBindings defaultBindings();

} // namespace apex::game
