#include "Serialization/SceneJson.h"

#include "Resources/AssetPath.h"
#include "Serialization/Json.h"

#include <array>
#include <charconv>
#include <format>
#include <set>
#include <string>

namespace apex::serialization {

namespace {

// Reads the optional field `key` of `object` with `read(value, context, path)`. Nothing happens when the
// field is absent.
template <typename T, typename Reader>
[[nodiscard]] core::Result<void> readOptional(const Json& object, std::string_view key,
                                              const JsonContext& context, std::string_view parent, T& target,
                                              Reader&& read) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return {};
    }
    auto value = read(*found, context, joinField(parent, key));
    if (!value) {
        return std::unexpected(value.error());
    }
    target = std::move(*value);
    return {};
}

[[nodiscard]] core::Result<float> readPositive(const Json& value, const JsonContext& context,
                                               std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (number && !(*number > 0.0f)) {
        return jsonError(context, field, "must be greater than 0");
    }
    return number;
}

[[nodiscard]] core::Result<float> readNonNegative(const Json& value, const JsonContext& context,
                                                  std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (number && *number < 0.0f) {
        return jsonError(context, field, "must not be negative");
    }
    return number;
}

[[nodiscard]] core::Result<float> readFactor(const Json& value, const JsonContext& context,
                                             std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (number && (*number < 0.0f || *number > 1.0f)) {
        return jsonError(context, field, "must be between 0 and 1");
    }
    return number;
}

[[nodiscard]] core::Result<glm::vec3> readColor(const Json& value, const JsonContext& context,
                                                std::string_view field) {
    const core::Result<glm::vec3> color = readVec3(value, context, field);
    if (color && (color->x < 0.0f || color->y < 0.0f || color->z < 0.0f)) {
        return jsonError(context, field, "a color component must not be negative");
    }
    return color;
}

[[nodiscard]] core::Result<TransformDesc> readTransform(const Json& value, const JsonContext& context,
                                                        std::string_view field) {
    if (core::Result<void> valid = requireObject(value, context, field); !valid) {
        return std::unexpected(valid.error());
    }
    if (core::Result<void> known =
            rejectUnknownFields(value, {"position", "rotation_deg", "scale"}, context, field);
        !known) {
        return std::unexpected(known.error());
    }
    TransformDesc transform;
    for (const auto& [key, target] :
         {std::pair{"position", &transform.position}, std::pair{"rotation_deg", &transform.rotationDegrees},
          std::pair{"scale", &transform.scale}}) {
        if (core::Result<void> read = readOptional(value, key, context, field, *target, readVec3); !read) {
            return std::unexpected(read.error());
        }
    }
    if (!(transform.scale.x > 0.0f && transform.scale.y > 0.0f && transform.scale.z > 0.0f)) {
        return jsonError(context, joinField(field, "scale"), "every component must be greater than 0");
    }
    return transform;
}

[[nodiscard]] core::Result<MaterialDesc> readMaterial(const Json& value, const JsonContext& context,
                                                      std::string_view field) {
    if (core::Result<void> valid = requireObject(value, context, field); !valid) {
        return std::unexpected(valid.error());
    }
    if (core::Result<void> known =
            rejectUnknownFields(value, {"base_color", "metallic", "roughness"}, context, field);
        !known) {
        return std::unexpected(known.error());
    }
    MaterialDesc material;
    if (core::Result<void> read =
            readOptional(value, "base_color", context, field, material.baseColor, readColor);
        !read) {
        return std::unexpected(read.error());
    }
    if (core::Result<void> read =
            readOptional(value, "metallic", context, field, material.metallic, readFactor);
        !read) {
        return std::unexpected(read.error());
    }
    if (core::Result<void> read =
            readOptional(value, "roughness", context, field, material.roughness, readFactor);
        !read) {
        return std::unexpected(read.error());
    }
    return material;
}

// Each shape has its own required fields; the fields of another shape are unknown fields.
[[nodiscard]] core::Result<world::Collider> readCollider(const Json& value, const JsonContext& context,
                                                         std::string_view field) {
    if (core::Result<void> valid = requireObject(value, context, field); !valid) {
        return std::unexpected(valid.error());
    }
    const auto shapeField = value.find("shape");
    if (shapeField == value.end()) {
        return jsonError(context, joinField(field, "shape"),
                         "missing; expected \"box\", \"sphere\" or \"capsule\"");
    }
    const core::Result<std::string> shape = readString(*shapeField, context, joinField(field, "shape"));
    if (!shape) {
        return std::unexpected(shape.error());
    }

    world::Collider collider;
    const auto require = [&](std::string_view key, auto& target, auto&& read) -> core::Result<void> {
        if (!value.contains(key)) {
            return jsonError(context, joinField(field, key), std::format("missing; a {} needs it", *shape));
        }
        return readOptional(value, key, context, field, target, read);
    };
    const auto readHalfExtents = [](const Json& json, const JsonContext& where,
                                    std::string_view path) -> core::Result<glm::vec3> {
        const core::Result<glm::vec3> extents = readVec3(json, where, path);
        if (extents && !(extents->x > 0.0f && extents->y > 0.0f && extents->z > 0.0f)) {
            return jsonError(where, path, "every component must be greater than 0");
        }
        return extents;
    };

    core::Result<void> known;
    core::Result<void> read;
    if (*shape == "box") {
        collider.shape = world::ColliderShape::Box;
        known = rejectUnknownFields(value, {"shape", "half_extents"}, context, field);
        read = require("half_extents", collider.halfExtents, readHalfExtents);
    } else if (*shape == "sphere") {
        collider.shape = world::ColliderShape::Sphere;
        known = rejectUnknownFields(value, {"shape", "radius"}, context, field);
        read = require("radius", collider.radius, readPositive);
    } else if (*shape == "capsule") {
        collider.shape = world::ColliderShape::Capsule;
        known = rejectUnknownFields(value, {"shape", "radius", "half_height"}, context, field);
        read = require("radius", collider.radius, readPositive);
        if (read) {
            read = require("half_height", collider.halfHeight, readPositive);
        }
    } else {
        return jsonError(
            context, joinField(field, "shape"),
            std::format("unknown shape \"{}\"; expected \"box\", \"sphere\" or \"capsule\"", *shape));
    }
    if (!known) {
        return std::unexpected(known.error());
    }
    if (!read) {
        return std::unexpected(read.error());
    }
    return collider;
}

[[nodiscard]] core::Result<world::RigidBody> readRigidBody(const Json& value, const JsonContext& context,
                                                           std::string_view field) {
    if (core::Result<void> valid = requireObject(value, context, field); !valid) {
        return std::unexpected(valid.error());
    }
    if (core::Result<void> known = rejectUnknownFields(value, {"type", "mass"}, context, field); !known) {
        return std::unexpected(known.error());
    }
    const auto typeField = value.find("type");
    if (typeField == value.end()) {
        return jsonError(context, joinField(field, "type"), "missing; expected \"static\" or \"dynamic\"");
    }
    const core::Result<std::string> type = readString(*typeField, context, joinField(field, "type"));
    if (!type) {
        return std::unexpected(type.error());
    }
    world::RigidBody body;
    if (*type == "dynamic") {
        body.type = world::BodyType::Dynamic;
        if (!value.contains("mass")) {
            return jsonError(context, joinField(field, "mass"),
                             "missing; a dynamic body needs a mass in kilograms");
        }
    } else if (*type != "static") {
        return jsonError(context, joinField(field, "type"),
                         std::format("unknown type \"{}\"; expected \"static\" or \"dynamic\"", *type));
    }
    if (core::Result<void> read = readOptional(value, "mass", context, field, body.mass, readPositive);
        !read) {
        return std::unexpected(read.error());
    }
    return body;
}

[[nodiscard]] core::Result<CameraDesc> readCamera(const Json& value, const JsonContext& context,
                                                  std::string_view field) {
    if (core::Result<void> valid = requireObject(value, context, field); !valid) {
        return std::unexpected(valid.error());
    }
    if (core::Result<void> known = rejectUnknownFields(value, {"fov_deg", "near", "far"}, context, field);
        !known) {
        return std::unexpected(known.error());
    }
    CameraDesc camera;
    for (const auto& [key, target] :
         {std::pair{"fov_deg", &camera.fovDegrees}, std::pair{"near", &camera.nearPlane},
          std::pair{"far", &camera.farPlane}}) {
        if (core::Result<void> read = readOptional(value, key, context, field, *target, readPositive);
            !read) {
            return std::unexpected(read.error());
        }
    }
    if (camera.fovDegrees >= 180.0f) {
        return jsonError(context, joinField(field, "fov_deg"), "must be less than 180 degrees");
    }
    if (camera.farPlane <= camera.nearPlane) {
        return jsonError(context, joinField(field, "far"), "must be greater than near");
    }
    return camera;
}

[[nodiscard]] core::Result<LightDesc> readLight(const Json& value, const JsonContext& context,
                                                std::string_view field) {
    if (core::Result<void> valid = requireObject(value, context, field); !valid) {
        return std::unexpected(valid.error());
    }
    if (core::Result<void> known =
            rejectUnknownFields(value, {"direction", "color", "intensity"}, context, field);
        !known) {
        return std::unexpected(known.error());
    }
    LightDesc light;
    if (!value.contains("direction")) {
        return jsonError(context, joinField(field, "direction"), "missing; the way the light travels");
    }
    if (core::Result<void> read = readOptional(value, "direction", context, field, light.direction, readVec3);
        !read) {
        return std::unexpected(read.error());
    }
    if (glm::length(light.direction) < 1e-6f) {
        return jsonError(context, joinField(field, "direction"), "must not be the zero vector");
    }
    if (core::Result<void> read = readOptional(value, "color", context, field, light.color, readColor);
        !read) {
        return std::unexpected(read.error());
    }
    if (core::Result<void> read =
            readOptional(value, "intensity", context, field, light.intensity, readNonNegative);
        !read) {
        return std::unexpected(read.error());
    }
    return light;
}

[[nodiscard]] core::Result<std::string> readMesh(const Json& value, const JsonContext& context,
                                                 std::string_view field) {
    const core::Result<std::string> path = readString(value, context, field);
    if (!path) {
        return path;
    }
    const core::Result<resources::AssetPath> assetPath = resources::AssetPath::parse(*path);
    if (!assetPath) {
        return jsonError(context, field, assetPath.error().message);
    }
    return assetPath->str();
}

// Stores a component read into an optional member of the entity.
template <typename T, typename Reader>
[[nodiscard]] core::Result<void> readComponent(const Json& entity, std::string_view key,
                                               const JsonContext& context, std::optional<T>& target,
                                               Reader&& read) {
    T value{};
    if (!entity.contains(key)) {
        return {};
    }
    if (core::Result<void> result = readOptional(entity, key, context, {}, value, read); !result) {
        return result;
    }
    target = std::move(value);
    return {};
}

[[nodiscard]] core::Result<EntityDesc> readEntity(const Json& value, JsonContext& context) {
    if (core::Result<void> valid = requireObject(value, context, {}); !valid) {
        return std::unexpected(valid.error());
    }
    const auto idField = value.find("id");
    if (idField == value.end()) {
        return jsonError(context, "id", "missing; every entity needs a unique id");
    }
    core::Result<std::string> id = readString(*idField, context, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    // From here on, errors name the entity by its id rather than by its place in the file.
    context.entity = *id;
    if (core::Result<void> known = rejectUnknownFields(
            value,
            {"id", "name", "transform", "mesh", "material", "collider", "rigid_body", "camera", "light"},
            context, {});
        !known) {
        return std::unexpected(known.error());
    }

    EntityDesc entity;
    entity.id = std::move(*id);
    core::Result<void> read = readComponent(value, "name", context, entity.name, readString);
    if (read) {
        read = readComponent(value, "transform", context, entity.transform, readTransform);
    }
    if (read) {
        read = readComponent(value, "mesh", context, entity.mesh, readMesh);
    }
    if (read) {
        read = readComponent(value, "material", context, entity.material, readMaterial);
    }
    if (read) {
        read = readComponent(value, "collider", context, entity.collider, readCollider);
    }
    if (read) {
        read = readComponent(value, "rigid_body", context, entity.rigidBody, readRigidBody);
    }
    if (read) {
        read = readComponent(value, "camera", context, entity.camera, readCamera);
    }
    if (read) {
        read = readComponent(value, "light", context, entity.light, readLight);
    }
    if (!read) {
        return std::unexpected(read.error());
    }
    if (entity.material && !entity.mesh) {
        return jsonError(context, "material", "needs a mesh to apply to");
    }
    if (entity.rigidBody && !entity.collider) {
        return jsonError(context, "rigid_body", "needs a collider");
    }
    return entity;
}

} // namespace

core::Result<SceneDescription> parseScene(std::string_view text, std::string_view sourceName) {
    JsonContext context{.kind = "Scene", .source = sourceName, .entity = {}};
    const core::Result<Json> root = parseJson(text, context);
    if (!root) {
        return std::unexpected(root.error());
    }
    if (core::Result<void> valid = requireObject(*root, context, {}); !valid) {
        return std::unexpected(valid.error());
    }

    const auto version = root->find("version");
    if (version == root->end()) {
        return jsonError(context, "version",
                         std::format("missing; this build reads version {}", kSceneVersion));
    }
    if (!version->is_number_integer()) {
        return jsonError(context, "version", "expected an integer");
    }
    if (const auto number = version->get<long long>(); number != kSceneVersion) {
        return jsonError(
            context, "version",
            std::format("version {} is not supported; this build reads version {}", number, kSceneVersion));
    }
    if (core::Result<void> known =
            rejectUnknownFields(*root, {"version", "ambient", "entities"}, context, {});
        !known) {
        return std::unexpected(known.error());
    }

    SceneDescription scene;
    if (core::Result<void> read = readOptional(*root, "ambient", context, {}, scene.ambient, readColor);
        !read) {
        return std::unexpected(read.error());
    }
    const auto entities = root->find("entities");
    if (entities == root->end() || !entities->is_array()) {
        return jsonError(context, "entities", "missing or not an array");
    }

    std::set<std::string> ids;
    for (std::size_t index = 0; index < entities->size(); ++index) {
        // Until its id is read, an entity is named by its place: "#3" is the fourth of the file.
        context.entity = std::format("#{}", index);
        core::Result<EntityDesc> entity = readEntity((*entities)[index], context);
        if (!entity) {
            return std::unexpected(entity.error());
        }
        if (!ids.insert(entity->id).second) {
            return jsonError(context, "id", "this id is already used by another entity");
        }
        scene.entities.push_back(std::move(*entity));
    }
    return scene;
}

namespace {

using OrderedJson = nlohmann::ordered_json;

// The shortest decimal that reads back as the same float: 0.1f is written "0.1", not "0.10000000149".
[[nodiscard]] double tidy(float value) {
    std::array<char, 32> buffer{};
    const std::to_chars_result written = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    double result = 0.0;
    std::from_chars(buffer.data(), written.ptr, result);
    return result;
}

[[nodiscard]] OrderedJson toJson(const glm::vec3& vector) {
    return OrderedJson::array({tidy(vector.x), tidy(vector.y), tidy(vector.z)});
}

[[nodiscard]] OrderedJson toJson(const world::Collider& collider) {
    OrderedJson json;
    switch (collider.shape) {
    case world::ColliderShape::Box:
        json["shape"] = "box";
        json["half_extents"] = toJson(collider.halfExtents);
        break;
    case world::ColliderShape::Sphere:
        json["shape"] = "sphere";
        json["radius"] = tidy(collider.radius);
        break;
    case world::ColliderShape::Capsule:
        json["shape"] = "capsule";
        json["radius"] = tidy(collider.radius);
        json["half_height"] = tidy(collider.halfHeight);
        break;
    }
    return json;
}

[[nodiscard]] OrderedJson toJson(const EntityDesc& entity) {
    OrderedJson json;
    json["id"] = entity.id;
    if (entity.name) {
        json["name"] = *entity.name;
    }
    if (entity.transform) {
        json["transform"] = {{"position", toJson(entity.transform->position)},
                             {"rotation_deg", toJson(entity.transform->rotationDegrees)},
                             {"scale", toJson(entity.transform->scale)}};
    }
    if (entity.mesh) {
        json["mesh"] = *entity.mesh;
    }
    if (entity.material) {
        json["material"] = {{"base_color", toJson(entity.material->baseColor)},
                            {"metallic", tidy(entity.material->metallic)},
                            {"roughness", tidy(entity.material->roughness)}};
    }
    if (entity.collider) {
        json["collider"] = toJson(*entity.collider);
    }
    if (entity.rigidBody) {
        const bool dynamic = entity.rigidBody->type == world::BodyType::Dynamic;
        json["rigid_body"] = {{"type", dynamic ? "dynamic" : "static"},
                              {"mass", tidy(entity.rigidBody->mass)}};
    }
    if (entity.camera) {
        json["camera"] = {{"fov_deg", tidy(entity.camera->fovDegrees)},
                          {"near", tidy(entity.camera->nearPlane)},
                          {"far", tidy(entity.camera->farPlane)}};
    }
    if (entity.light) {
        json["light"] = {{"direction", toJson(entity.light->direction)},
                         {"color", toJson(entity.light->color)},
                         {"intensity", tidy(entity.light->intensity)}};
    }
    return json;
}

} // namespace

std::string writeScene(const SceneDescription& scene) {
    OrderedJson root;
    root["version"] = kSceneVersion;
    root["ambient"] = toJson(scene.ambient);
    OrderedJson entities = OrderedJson::array();
    for (const EntityDesc& entity : scene.entities) {
        entities.push_back(toJson(entity));
    }
    root["entities"] = std::move(entities);
    return root.dump(2) + "\n";
}

} // namespace apex::serialization
