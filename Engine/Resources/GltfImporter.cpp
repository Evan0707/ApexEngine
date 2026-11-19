#include "Resources/GltfImporter.h"

#include "Core/Math.h"
#include "Resources/FileReader.h"

// The only file that includes cgltf (decision 0012).
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace apex::resources {

namespace {

// Deep enough for any real model; guards against a cycle in a malformed file.
constexpr int kMaxNodeDepth = 64;

struct DataDeleter {
    void operator()(cgltf_data* data) const noexcept { cgltf_free(data); }
};
using DataPtr = std::unique_ptr<cgltf_data, DataDeleter>;

[[nodiscard]] std::string_view describe(cgltf_result result) noexcept {
    switch (result) {
    case cgltf_result_success:
        return "success";
    case cgltf_result_data_too_short:
        return "the file is truncated";
    case cgltf_result_unknown_format:
        return "not a glTF or GLB file";
    case cgltf_result_invalid_json:
        return "invalid JSON";
    case cgltf_result_invalid_gltf:
        return "invalid glTF";
    case cgltf_result_invalid_options:
        return "invalid options";
    case cgltf_result_file_not_found:
        return "a file it refers to is missing";
    case cgltf_result_io_error:
        return "read error";
    case cgltf_result_out_of_memory:
        return "out of memory";
    case cgltf_result_legacy_gltf:
        return "glTF 1.0 is not supported";
    case cgltf_result_max_enum:
        break;
    }
    return "unknown error";
}

[[nodiscard]] std::string displayName(const char* name, std::string_view kind, std::size_t index) {
    return name != nullptr && name[0] != '\0' ? std::format("'{}'", name) : std::format("{} {}", kind, index);
}

[[nodiscard]] glm::vec3 anyPerpendicular(const glm::vec3& normal) noexcept {
    const glm::vec3 axis = std::abs(normal.y) < 0.999f ? core::kWorldUp : core::kWorldRight;
    return glm::normalize(glm::cross(axis, normal));
}

[[nodiscard]] bool isUsable(const glm::vec3& vector) noexcept {
    const float length = glm::length(vector);
    return std::isfinite(length) && length > 1e-12f;
}

// Converts the parsed file into ModelData: one pass over the scene's nodes, primitives merged by material.
class Converter {
public:
    Converter(const cgltf_data& data, std::string_view name) : m_data(data), m_name(name) {}

    [[nodiscard]] core::Result<ModelData> run() {
        m_model.materials.resize(m_data.materials_count);
        m_partOfMaterial.assign(m_data.materials_count + 1, std::nullopt);

        const cgltf_scene* scene = m_data.scene != nullptr   ? m_data.scene
                                   : m_data.scenes_count > 0 ? &m_data.scenes[0]
                                                             : nullptr;
        if (scene != nullptr) {
            for (std::size_t i = 0; i < scene->nodes_count; ++i) {
                if (core::Result<void> added = addNode(*scene->nodes[i], 0); !added) {
                    return std::unexpected(added.error());
                }
            }
        } else {
            for (std::size_t i = 0; i < m_data.nodes_count; ++i) {
                if (m_data.nodes[i].parent == nullptr) {
                    if (core::Result<void> added = addNode(m_data.nodes[i], 0); !added) {
                        return std::unexpected(added.error());
                    }
                }
            }
        }
        if (m_model.parts.empty()) {
            return error("no triangle to draw");
        }
        for (std::size_t i = 0; i < m_model.parts.size(); ++i) {
            if (core::Result<void> valid =
                    validate(m_model.parts[i].mesh, std::format("{} part {}", m_name, i));
                !valid) {
                return std::unexpected(valid.error());
            }
        }
        return std::move(m_model);
    }

private:
    [[nodiscard]] std::unexpected<core::Error> error(std::string_view reason) const {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("glTF '{}': {}", m_name, reason));
    }

    [[nodiscard]] core::Result<void> addNode(const cgltf_node& node, int depth) {
        if (depth > kMaxNodeDepth) {
            return error("the node hierarchy is too deep or has a cycle");
        }
        if (node.mesh != nullptr) {
            glm::mat4 world(1.0f);
            cgltf_node_transform_world(&node, &world[0][0]);
            const cgltf_mesh& mesh = *node.mesh;
            for (std::size_t i = 0; i < mesh.primitives_count; ++i) {
                if (core::Result<void> added = addPrimitive(mesh, mesh.primitives[i], world); !added) {
                    return added;
                }
            }
        }
        for (std::size_t i = 0; i < node.children_count; ++i) {
            if (core::Result<void> added = addNode(*node.children[i], depth + 1); !added) {
                return added;
            }
        }
        return {};
    }

    [[nodiscard]] core::Result<void> addPrimitive(const cgltf_mesh& mesh, const cgltf_primitive& primitive,
                                                  const glm::mat4& world) {
        // Points and lines are not surfaces: skipped.
        if (primitive.type != cgltf_primitive_type_triangles) {
            return {};
        }
        const std::string meshName =
            displayName(mesh.name, "mesh", static_cast<std::size_t>(&mesh - m_data.meshes));
        const cgltf_accessor* positions = nullptr;
        const cgltf_accessor* normals = nullptr;
        const cgltf_accessor* tangents = nullptr;
        const cgltf_accessor* uvs = nullptr;
        for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
            const cgltf_attribute& attribute = primitive.attributes[i];
            switch (attribute.type) {
            case cgltf_attribute_type_position:
                positions = attribute.data;
                break;
            case cgltf_attribute_type_normal:
                normals = attribute.data;
                break;
            case cgltf_attribute_type_tangent:
                tangents = attribute.data;
                break;
            case cgltf_attribute_type_texcoord:
                uvs = attribute.index == 0 ? attribute.data : uvs;
                break;
            default:
                break;
            }
        }
        if (positions == nullptr) {
            return error(std::format("mesh {} has a primitive without POSITION", meshName));
        }

        const std::size_t vertexCount = positions->count;
        std::vector<std::uint32_t> indices;
        if (primitive.indices != nullptr) {
            indices.reserve(primitive.indices->count);
            for (std::size_t i = 0; i < primitive.indices->count; ++i) {
                const std::size_t index = cgltf_accessor_read_index(primitive.indices, i);
                if (index >= vertexCount) {
                    return error(std::format("mesh {}: index {} is {}, but there are {} vertices", meshName,
                                             i, index, vertexCount));
                }
                indices.push_back(static_cast<std::uint32_t>(index));
            }
        } else {
            for (std::size_t i = 0; i < vertexCount; ++i) {
                indices.push_back(static_cast<std::uint32_t>(i));
            }
        }
        if (indices.empty() || indices.size() % 3 != 0) {
            return error(
                std::format("mesh {}: {} indices, not a list of triangles", meshName, indices.size()));
        }

        std::vector<Vertex> vertices(vertexCount);
        for (std::size_t i = 0; i < vertexCount; ++i) {
            cgltf_accessor_read_float(positions, i, &vertices[i].position[0], 3);
            if (uvs != nullptr) {
                cgltf_accessor_read_float(uvs, i, &vertices[i].uv[0], 2);
            }
            if (normals != nullptr) {
                cgltf_accessor_read_float(normals, i, &vertices[i].normal[0], 3);
            }
            if (tangents != nullptr) {
                cgltf_accessor_read_float(tangents, i, &vertices[i].tangent[0], 4);
            }
        }
        if (normals == nullptr) {
            computeNormals(vertices, indices);
        }
        if (tangents == nullptr) {
            computeTangents(vertices, indices);
        }
        transform(vertices, indices, world);

        const std::size_t material = primitive.material != nullptr
                                         ? static_cast<std::size_t>(primitive.material - m_data.materials)
                                         : m_data.materials_count;
        ModelPart& part = partFor(material);
        if (part.mesh.vertices.size() + vertices.size() > std::numeric_limits<std::uint32_t>::max()) {
            return error("too many vertices");
        }
        const auto offset = static_cast<std::uint32_t>(part.mesh.vertices.size());
        part.mesh.vertices.insert(part.mesh.vertices.end(), vertices.begin(), vertices.end());
        for (const std::uint32_t index : indices) {
            part.mesh.indices.push_back(offset + index);
        }
        return {};
    }

    // Area-weighted average of the faces around each vertex.
    static void computeNormals(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
        for (Vertex& vertex : vertices) {
            vertex.normal = glm::vec3(0.0f);
        }
        for (std::size_t i = 0; i < indices.size(); i += 3) {
            Vertex& a = vertices[indices[i]];
            Vertex& b = vertices[indices[i + 1]];
            Vertex& c = vertices[indices[i + 2]];
            const glm::vec3 face = glm::cross(b.position - a.position, c.position - a.position);
            a.normal += face;
            b.normal += face;
            c.normal += face;
        }
        for (Vertex& vertex : vertices) {
            vertex.normal = isUsable(vertex.normal) ? glm::normalize(vertex.normal) : core::kWorldUp;
        }
    }

    // From the UV gradients, with this engine's sign convention: cross(normal, tangent) * w points
    // towards the top of the texture (decreasing v), as glTF normal maps expect.
    static void computeTangents(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
        std::vector<glm::vec3> alongU(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> alongV(vertices.size(), glm::vec3(0.0f));
        for (std::size_t i = 0; i < indices.size(); i += 3) {
            const std::array<std::uint32_t, 3> corner{indices[i], indices[i + 1], indices[i + 2]};
            const glm::vec3 edge1 = vertices[corner[1]].position - vertices[corner[0]].position;
            const glm::vec3 edge2 = vertices[corner[2]].position - vertices[corner[0]].position;
            const glm::vec2 uv1 = vertices[corner[1]].uv - vertices[corner[0]].uv;
            const glm::vec2 uv2 = vertices[corner[2]].uv - vertices[corner[0]].uv;
            const float determinant = uv1.x * uv2.y - uv2.x * uv1.y;
            if (std::abs(determinant) < 1e-12f) {
                continue;
            }
            const glm::vec3 u = (edge1 * uv2.y - edge2 * uv1.y) / determinant;
            const glm::vec3 v = (edge2 * uv1.x - edge1 * uv2.x) / determinant;
            for (const std::uint32_t index : corner) {
                alongU[index] += u;
                alongV[index] += v;
            }
        }
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const glm::vec3& normal = vertices[i].normal;
            glm::vec3 tangent = alongU[i] - normal * glm::dot(normal, alongU[i]);
            tangent = isUsable(tangent) ? glm::normalize(tangent) : anyPerpendicular(normal);
            const float sign = glm::dot(glm::cross(normal, tangent), alongV[i]) > 0.0f ? -1.0f : 1.0f;
            vertices[i].tangent = glm::vec4(tangent, sign);
        }
    }

    // To the model's space. A mirroring transform reverses the winding: indices are swapped to keep
    // counter-clockwise front faces (spec 3.3).
    static void transform(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices,
                          const glm::mat4& world) {
        const glm::mat3 linear(world);
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(linear));
        const bool mirrored = glm::determinant(linear) < 0.0f;
        for (Vertex& vertex : vertices) {
            vertex.position = glm::vec3(world * glm::vec4(vertex.position, 1.0f));
            glm::vec3 normal = normalMatrix * vertex.normal;
            normal = isUsable(normal) ? glm::normalize(normal) : core::kWorldUp;
            glm::vec3 tangent = linear * glm::vec3(vertex.tangent);
            tangent -= normal * glm::dot(normal, tangent);
            tangent = isUsable(tangent) ? glm::normalize(tangent) : anyPerpendicular(normal);
            const float sign = (vertex.tangent.w < 0.0f ? -1.0f : 1.0f) * (mirrored ? -1.0f : 1.0f);
            vertex.normal = normal;
            vertex.tangent = glm::vec4(tangent, sign);
        }
        if (mirrored) {
            for (std::size_t i = 0; i < indices.size(); i += 3) {
                std::swap(indices[i + 1], indices[i + 2]);
            }
        }
    }

    // material == materials_count: primitives without a material, given the default one.
    [[nodiscard]] ModelPart& partFor(std::size_t material) {
        if (!m_partOfMaterial[material]) {
            std::uint32_t modelMaterial = static_cast<std::uint32_t>(material);
            if (material == m_data.materials_count) {
                modelMaterial = static_cast<std::uint32_t>(m_model.materials.size());
                m_model.materials.push_back(ModelMaterial{.name = "Default"});
            }
            m_partOfMaterial[material] = m_model.parts.size();
            m_model.parts.push_back(ModelPart{.material = modelMaterial});
        }
        return m_model.parts[*m_partOfMaterial[material]];
    }

    const cgltf_data& m_data;
    std::string_view m_name;
    ModelData m_model;
    // Part of each glTF material, and of "no material" at the end.
    std::vector<std::optional<std::size_t>> m_partOfMaterial;
};

} // namespace

core::Result<ModelData> importGltf(const std::filesystem::path& file, std::string_view name) {
    core::Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes) {
        return core::makeError(bytes.error().code, std::format("glTF '{}': {}", name, bytes.error().message));
    }

    const cgltf_options options{};
    cgltf_data* parsed = nullptr;
    if (const cgltf_result result = cgltf_parse(&options, bytes->data(), bytes->size(), &parsed);
        result != cgltf_result_success) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("glTF '{}': {}", name, describe(result)));
    }
    const DataPtr data(parsed);
    // External .bin files are resolved next to the model file.
    if (const cgltf_result result = cgltf_load_buffers(&options, data.get(), file.string().c_str());
        result != cgltf_result_success) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("glTF '{}': cannot load its buffers: {}", name, describe(result)));
    }
    if (const cgltf_result result = cgltf_validate(data.get()); result != cgltf_result_success) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("glTF '{}': {}", name, describe(result)));
    }
    if (data->extensions_required_count > 0) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("glTF '{}': requires the extension {}, which is not supported",
                                           name, data->extensions_required[0]));
    }
    return Converter(*data, name).run();
}

} // namespace apex::resources
