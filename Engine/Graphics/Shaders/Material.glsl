// Materials (G-08): every shader reads them through this file. The factors are in the per-frame table
// (decision 0011), same layout as MaterialGpu in Graphics/MaterialTable.h; the textures are in descriptor
// set 1, one set per material (spec 5.4, decision 0014).

struct MaterialData {
    vec4 baseColor;
    float metallic;
    float roughness;
    float normalScale;
};

layout(std430, set = 0, binding = 2) readonly buffer Materials {
    MaterialData materials[];
};

// sRGB format: sampled as linear values. A material without a texture has a white one.
layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;
// glTF: metallic in blue, roughness in green. Linear.
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessTexture;
// Tangent space, glTF: +X right, +Y towards the top of the texture, +Z out of the surface. Linear. A material
// without one has a flat one.
layout(set = 1, binding = 2) uniform sampler2D normalTexture;

// The material at a point of the surface: factors times textures.
MaterialData sampleMaterial(uint index, vec2 uv) {
    MaterialData material = materials[index];
    material.baseColor *= texture(baseColorTexture, uv);
    vec4 metallicRoughness = texture(metallicRoughnessTexture, uv);
    material.metallic *= metallicRoughness.b;
    material.roughness *= metallicRoughness.g;
    return material;
}

// The normal of the surface, bent by the material's normal map. n: unit normal of the geometry; tangent: xyz
// along increasing u, w the sign of the bitangent (resources::Vertex).
vec3 sampleNormal(uint index, vec2 uv, vec3 n, vec4 tangent) {
    vec3 mapped = texture(normalTexture, uv).xyz * 2.0 - 1.0;
    mapped.xy *= materials[index].normalScale;
    // The interpolated tangent drifts from the normal: made perpendicular again.
    vec3 t = normalize(tangent.xyz - n * dot(n, tangent.xyz));
    vec3 b = cross(n, t) * (tangent.w < 0.0 ? -1.0 : 1.0);
    return normalize(mat3(t, b, n) * mapped);
}
