#pragma once

// Internal to Serialization, and the only include of nlohmann/json (decision 0016). No public header of
// the module includes this file. Implicit conversions are off: every field is read with its expected type.
#include "Core/Error.h"
#include "Core/Math.h"

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include <initializer_list>
#include <string>
#include <string_view>

namespace apex::serialization {

using Json = nlohmann::json;

// Where a value sits, for error messages: "Scene 'Scenes/demo.json', entity 'crate_01', field
// 'transform.position': ...". `entity` is empty outside the entities.
struct JsonContext {
    // "Scene" or "Configuration".
    std::string_view kind;
    std::string_view source;
    std::string entity;
};

[[nodiscard]] std::unexpected<core::Error> jsonError(const JsonContext& context, std::string_view field,
                                                     std::string_view reason);

// The text as JSON; a syntax error becomes an Error that says where (no exception leaves Serialization).
[[nodiscard]] core::Result<Json> parseJson(std::string_view text, const JsonContext& context);

// `field` is the path of `value` in the file ("transform.position"), empty for the root.
[[nodiscard]] core::Result<void> requireObject(const Json& value, const JsonContext& context,
                                               std::string_view field);
// Every key of the object must be in `allowed`: a typo is an error that names it (decision 0017).
[[nodiscard]] core::Result<void> rejectUnknownFields(const Json& object,
                                                     std::initializer_list<std::string_view> allowed,
                                                     const JsonContext& context, std::string_view field);

// Finite, and representable as a float.
[[nodiscard]] core::Result<float> readNumber(const Json& value, const JsonContext& context,
                                             std::string_view field);
[[nodiscard]] core::Result<glm::vec3> readVec3(const Json& value, const JsonContext& context,
                                               std::string_view field);
// Not empty.
[[nodiscard]] core::Result<std::string> readString(const Json& value, const JsonContext& context,
                                                   std::string_view field);

// "transform" + "position" -> "transform.position".
[[nodiscard]] std::string joinField(std::string_view parent, std::string_view key);

} // namespace apex::serialization
