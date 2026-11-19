#version 460

#include "Frame.glsl"

// resources::Vertex. Tangent (location 2) and UV (location 3) are not read before textures (phase 4).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// Render space: the camera is at the origin (G-07).
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) flat out uint outMaterial;

void main() {
    // firstInstance of each draw is the object's place in the per-frame buffer (G-02).
    ObjectData object = objects[gl_InstanceIndex];
    vec4 position = object.model * vec4(inPosition, 1.0);
    gl_Position = frame.viewProjection * position;
    outPosition = position.xyz;
    outNormal = mat3(object.normalMatrix) * inNormal;
    outMaterial = object.materialIndex;
}
