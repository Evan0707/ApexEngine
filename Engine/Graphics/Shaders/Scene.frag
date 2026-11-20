#version 460

#include "Frame.glsl"
#include "Material.glsl"
#include "Pbr.glsl"
#include "Shadow.glsl"
#include "Environment.glsl"

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
    // The environment replaces the constant ambient term (spec 5.5) when there is one.
    vec3 surrounding =
        frame.environment.x > 0.5 ? shadeEnvironment(material, n, v) : shadeAmbient(material, frame.ambient.rgb);
    vec3 color = shadeDirectionalLight(material, n, v, frame.directionToLight.xyz, radiance) + surrounding;
    // Linear HDR color: the tonemapping pass maps it to the swapchain (decision 0021).
    outColor = vec4(color, 1.0);
}
