#version 460

#include "Frame.glsl"

// resources::Vertex.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUv;

// Render space: the camera is at the origin (G-07).
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) flat out uint outMaterial;
layout(location = 3) out vec2 outUv;
layout(location = 4) out vec4 outTangent;

void main() {
    // firstInstance of each draw is the object's place in the per-frame buffer (G-02).
    ObjectData object = objects[gl_InstanceIndex];
    vec4 position = object.model * vec4(inPosition, 1.0);
    gl_Position = frame.viewProjection * position;
    outPosition = position.xyz;
    outNormal = mat3(object.normalMatrix) * inNormal;
    outMaterial = object.materialIndex;
    outUv = inUv;
    // The tangent follows the surface: the model matrix, not the normal matrix.
    outTangent = vec4(mat3(object.model) * inTangent.xyz, inTangent.w);
}
