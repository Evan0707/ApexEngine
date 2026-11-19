#version 460

#include "Frame.glsl"
#include "Material.glsl"

const float PI = 3.14159265358979;

layout(location = 0) in vec3 inNormal;
layout(location = 1) flat in uint inMaterial;

layout(location = 0) out vec4 outColor;

void main() {
    MaterialData material = materials[inMaterial];
    vec3 normal = normalize(inNormal);
    float lit = max(dot(normal, frame.directionToLight.xyz), 0.0);
    vec3 albedo = material.baseColor.rgb;
    // Lambert diffuse; linear color, encoded by the sRGB swapchain.
    vec3 color = albedo / PI * frame.lightRadiance.rgb * lit + albedo * frame.ambient.rgb;
    outColor = vec4(color, 1.0);
}
