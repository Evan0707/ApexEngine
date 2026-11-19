// Metallic-roughness BRDF of glTF 2.0 (spec 5.5): Lambert diffuse, GGX distribution, height-correlated
// Smith visibility, Schlick Fresnel. Linear values throughout.

const float PI = 3.14159265358979;
// Reflectance of dielectrics at normal incidence (glTF).
const vec3 DIELECTRIC_F0 = vec3(0.04);
// A perfectly smooth surface would give an infinitely thin highlight from a directional light.
const float MIN_PERCEPTUAL_ROUGHNESS = 0.045;

float distributionGgx(float nDotH, float alpha) {
    float alpha2 = alpha * alpha;
    float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denominator * denominator);
}

float visibilitySmithGgxCorrelated(float nDotV, float nDotL, float alpha) {
    float alpha2 = alpha * alpha;
    float viewTerm = nDotL * sqrt(nDotV * nDotV * (1.0 - alpha2) + alpha2);
    float lightTerm = nDotV * sqrt(nDotL * nDotL * (1.0 - alpha2) + alpha2);
    return 0.5 / (viewTerm + lightTerm);
}

vec3 fresnelSchlick(float vDotH, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - vDotH, 5.0);
}

vec3 diffuseColor(MaterialData material) {
    return material.baseColor.rgb * (1.0 - material.metallic);
}

vec3 specularColor(MaterialData material) {
    return mix(DIELECTRIC_F0, material.baseColor.rgb, material.metallic);
}

// Light reflected towards the viewer by one directional light. n, v and l are unit vectors: normal,
// towards the viewer, towards the light.
vec3 shadeDirectionalLight(MaterialData material, vec3 n, vec3 v, vec3 l, vec3 radiance) {
    float nDotL = dot(n, l);
    if (nDotL <= 0.0) {
        return vec3(0.0);
    }
    vec3 h = normalize(v + l);
    float nDotV = max(dot(n, v), 1e-4);
    float nDotH = max(dot(n, h), 0.0);
    float vDotH = max(dot(v, h), 0.0);

    float perceptualRoughness = max(material.roughness, MIN_PERCEPTUAL_ROUGHNESS);
    float alpha = perceptualRoughness * perceptualRoughness;
    vec3 specular = distributionGgx(nDotH, alpha) * visibilitySmithGgxCorrelated(nDotV, nDotL, alpha) *
                    fresnelSchlick(vDotH, specularColor(material));
    vec3 diffuse = diffuseColor(material) / PI;
    return (diffuse + specular) * radiance * nDotL;
}

// Constant ambient term (spec 5.5), replaced by image-based lighting in P1.
vec3 shadeAmbient(MaterialData material, vec3 ambient) {
    return ambient * (diffuseColor(material) + specularColor(material));
}
