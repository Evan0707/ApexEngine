// Image-based lighting (spec 5.4, decision 0022), descriptor set 2. Needs Frame.glsl, Material.glsl and
// Pbr.glsl. Irradiance from nine spherical harmonics in the frame uniforms, specular from the prefiltered
// cubemap and the split-sum table (Graphics/EnvironmentLighting.h computes both tables' CPU part).

layout(set = 2, binding = 0) uniform samplerCube specularMap;
// x: n.v, y: perceptual roughness. Scale and bias of F0.
layout(set = 2, binding = 1) uniform sampler2D brdfTable;

// Same basis and order as graphics::shBasis.
vec3 environmentIrradiance(vec3 n) {
    vec3 irradiance = frame.irradiance[0].rgb * 0.282095 + frame.irradiance[1].rgb * (0.488603 * n.y) +
                      frame.irradiance[2].rgb * (0.488603 * n.z) + frame.irradiance[3].rgb * (0.488603 * n.x) +
                      frame.irradiance[4].rgb * (1.092548 * n.x * n.y) + frame.irradiance[5].rgb * (1.092548 * n.y * n.z) +
                      frame.irradiance[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) +
                      frame.irradiance[7].rgb * (1.092548 * n.x * n.z) +
                      frame.irradiance[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));
    return max(irradiance, vec3(0.0));
}

// Light of the environment reflected towards the viewer. n and v: unit normal and direction to the viewer.
vec3 shadeEnvironment(MaterialData material, vec3 n, vec3 v) {
    float nDotV = max(dot(n, v), 1e-4);
    float roughness = max(material.roughness, MIN_PERCEPTUAL_ROUGHNESS);
    vec3 reflected = reflect(-v, n);
    vec3 prefiltered = textureLod(specularMap, reflected, roughness * frame.environment.z).rgb;
    vec2 split = texture(brdfTable, vec2(nDotV, roughness)).rg;
    vec3 specular = prefiltered * (specularColor(material) * split.x + split.y);
    vec3 diffuse = diffuseColor(material) * environmentIrradiance(n) / PI;
    return (diffuse + specular) * frame.environment.y;
}
