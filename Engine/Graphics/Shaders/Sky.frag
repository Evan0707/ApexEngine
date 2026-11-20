#version 460

#include "Frame.glsl"
#include "Material.glsl"
#include "Pbr.glsl"
#include "Environment.glsl"

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

// The environment seen behind the scene (spec 5.4): the sharpest level of the specular cubemap.
void main() {
    // Render space is centered on the camera: the point of the far plane gives the direction.
    vec4 far = frame.inverseViewProjection * vec4(inNdc, 1.0, 1.0);
    vec3 direction = normalize(far.xyz / far.w);
    outColor = vec4(textureLod(specularMap, direction, 0.0).rgb * frame.environment.y, 1.0);
}
