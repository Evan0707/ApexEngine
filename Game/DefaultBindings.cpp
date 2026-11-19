#include "DefaultBindings.h"

namespace apex::game {

input::InputBindings defaultBindings() {
    return input::InputBindings{
        .buttons =
            {
                {"Jump", "Space"},
                {"Sprint", "LeftShift"},
                {"ToggleCursor", "Escape"},
                {"ToggleDebug", "F1"},
                {"ToggleColliders", "F2"},
                {"SwitchCamera", "F3"},
                {"ToggleShadows", "F4"},
            },
        .keyAxes =
            {
                {"MoveForward", "S", "W"},
                {"MoveRight", "A", "D"},
                {"MoveUp", "Q", "E"},
            },
        .mouseAxes =
            {
                {"LookX", input::MouseAxis::X, 1.0f},
                {"LookY", input::MouseAxis::Y, 1.0f},
            },
    };
}

} // namespace apex::game
