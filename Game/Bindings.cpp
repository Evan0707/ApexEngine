#include "Bindings.h"

namespace apex::game {

input::InputBindings toInputBindings(const serialization::InputConfig& config) {
    input::InputBindings bindings;
    for (const serialization::ButtonConfig& button : config.buttons) {
        bindings.buttons.push_back({button.action, button.key});
    }
    for (const serialization::KeyAxisConfig& axis : config.keyAxes) {
        bindings.keyAxes.push_back({axis.action, axis.negativeKey, axis.positiveKey});
    }
    for (const serialization::MouseAxisConfig& axis : config.mouseAxes) {
        const input::MouseAxis mouseAxis =
            axis.axis == serialization::MouseAxisName::X ? input::MouseAxis::X : input::MouseAxis::Y;
        bindings.mouseAxes.push_back({axis.action, mouseAxis, axis.scale});
    }
    return bindings;
}

} // namespace apex::game
