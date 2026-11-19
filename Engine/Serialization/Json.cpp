#include "Serialization/Json.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace apex::serialization {

std::unexpected<core::Error> jsonError(const JsonContext& context, std::string_view field,
                                       std::string_view reason) {
    std::string where = std::format("{} '{}'", context.kind, context.source);
    if (!context.entity.empty()) {
        where += std::format(", entity '{}'", context.entity);
    }
    if (!field.empty()) {
        where += std::format(", field '{}'", field);
    }
    return core::makeError(core::ErrorCode::InvalidArgument, std::format("{}: {}", where, reason));
}

core::Result<Json> parseJson(std::string_view text, const JsonContext& context) {
    try {
        return Json::parse(text);
    } catch (const Json::exception& exception) {
        // The library's message gives the line, the column and what it expected.
        return jsonError(context, {}, std::format("invalid JSON: {}", exception.what()));
    }
}

core::Result<void> requireObject(const Json& value, const JsonContext& context, std::string_view field) {
    if (!value.is_object()) {
        return jsonError(context, field, "expected an object");
    }
    return {};
}

core::Result<void> rejectUnknownFields(const Json& object, std::initializer_list<std::string_view> allowed,
                                       const JsonContext& context, std::string_view field) {
    for (const auto& [key, value] : object.items()) {
        if (std::ranges::find(allowed, std::string_view(key)) == allowed.end()) {
            return jsonError(context, joinField(field, key), "unknown field");
        }
    }
    return {};
}

core::Result<float> readNumber(const Json& value, const JsonContext& context, std::string_view field) {
    if (!value.is_number()) {
        return jsonError(context, field, "expected a number");
    }
    const double number = value.get<double>();
    if (!std::isfinite(number) || std::abs(number) > static_cast<double>(std::numeric_limits<float>::max())) {
        return jsonError(context, field, "the number is too large");
    }
    return static_cast<float>(number);
}

core::Result<glm::vec3> readVec3(const Json& value, const JsonContext& context, std::string_view field) {
    if (!value.is_array() || value.size() != 3) {
        return jsonError(context, field, "expected an array of 3 numbers");
    }
    glm::vec3 vector(0.0f);
    for (glm::length_t i = 0; i < 3; ++i) {
        const core::Result<float> component = readNumber(value[static_cast<std::size_t>(i)], context, field);
        if (!component) {
            return jsonError(context, field, "expected an array of 3 numbers");
        }
        vector[i] = *component;
    }
    return vector;
}

core::Result<std::string> readString(const Json& value, const JsonContext& context, std::string_view field) {
    if (!value.is_string()) {
        return jsonError(context, field, "expected a string");
    }
    std::string text = value.get<std::string>();
    if (text.empty()) {
        return jsonError(context, field, "the string is empty");
    }
    return text;
}

std::string joinField(std::string_view parent, std::string_view key) {
    return parent.empty() ? std::string(key) : std::format("{}.{}", parent, key);
}

} // namespace apex::serialization
