// The material table (G-08, decision 0011): every shader reads materials through this file. Same layout
// as MaterialGpu in Graphics/MaterialTable.h.

struct MaterialData {
    vec4 baseColor;
    float metallic;
    float roughness;
};

layout(std430, set = 0, binding = 2) readonly buffer Materials {
    MaterialData materials[];
};
