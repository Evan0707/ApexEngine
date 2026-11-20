#version 460

#include "Frame.glsl"

// Depth only: the position is all the shadow pass reads (decision 0021).
layout(location = 0) in vec3 inPosition;

void main() {
    // firstInstance of each draw is the object's place in the per-frame buffer (G-02).
    ObjectData object = objects[gl_InstanceIndex];
    gl_Position = frame.lightViewProjection * (object.model * vec4(inPosition, 1.0));
}
