#pragma once

#include "Input/ActionMap.h"
#include "Serialization/InputConfig.h"

namespace apex::game {

// The bindings of Config/input.json in the form the action map is built from (decision 0017):
// Serialization and Input do not know each other. Key names are checked by input::ActionMap::create.
[[nodiscard]] input::InputBindings toInputBindings(const serialization::InputConfig& config);

} // namespace apex::game
