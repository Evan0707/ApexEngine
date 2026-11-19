#pragma once

#include "Core/Error.h"
#include "Serialization/SceneDescription.h"

#include <string>
#include <string_view>

namespace apex::serialization {

// Reads a scene file's text (spec 5.6, decision 0017). Everything is checked: an error names the file
// (`sourceName`, its asset path), the entity and the field, and nothing is returned. Unknown fields are
// errors too.
[[nodiscard]] core::Result<SceneDescription> parseScene(std::string_view text, std::string_view sourceName);

// The scene as text in the same format: parseScene reads it back to an equal description. Fields are
// written in a fixed order and numbers in their shortest exact form, so that the same scene always gives
// the same text.
[[nodiscard]] std::string writeScene(const SceneDescription& scene);

} // namespace apex::serialization
