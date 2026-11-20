#version 460

#include "Frame.glsl"

// Segments of the collider display (decision 0023), in render space.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

// The camera is at the origin of render space: scaling a point moves it towards the camera. An edge lying on
// a surface stays in front of it instead of flickering through it.
const float TOWARDS_CAMERA = 0.995;

void main() {
    gl_Position = frame.viewProjection * vec4(inPosition * TOWARDS_CAMERA, 1.0);
    outColor = inColor;
}
