// Materials (G-08): every shader reads them through this file. The factors are in the per-frame table
// (decision 0011), same layout as MaterialGpu in Graphics/MaterialTable.h; the textures are in descriptor
// set 1, one set per material (spec 5.4, decision 0014).

struct MaterialData {
    vec4 baseColor;
    float metallic;
    float roughness;
};

layout(std430, set = 0, binding = 2) readonly buffer Materials {
    MaterialData materials[];
};

// sRGB format: sampled as linear values. A material without a texture has a white one.
layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;
// glTF: metallic in blue, roughness in green. Linear.
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessTexture;

// The material at a point of the surface: factors times textures.
MaterialData sampleMaterial(uint index, vec2 uv) {
    MaterialData material = materials[index];
    material.baseColor *= texture(baseColorTexture, uv);
    vec4 metallicRoughness = texture(metallicRoughnessTexture, uv);
    material.metallic *= metallicRoughness.b;
    material.roughness *= metallicRoughness.g;
    return material;
}
