#version 460

#include "Frame.glsl"
#include "Material.glsl"
#include "Pbr.glsl"
#include "Shadow.glsl"

// Render space: the camera is at the origin (G-07).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) flat in uint inMaterial;
layout(location = 3) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main() {
    MaterialData material = sampleMaterial(inMaterial, inUv);
    vec3 n = normalize(inNormal);
    vec3 v = normalize(-inPosition);
    vec3 radiance = frame.lightRadiance.rgb * shadowVisibility(inPosition);
    vec3 color = shadeDirectionalLight(material, n, v, frame.directionToLight.xyz, radiance) +
                 shadeAmbient(material, frame.ambient.rgb);
    // Linear HDR color: the tonemapping pass maps it to the swapchain (decision 0021).
    outColor = vec4(color, 1.0);
}
