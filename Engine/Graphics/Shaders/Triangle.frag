#version 460

// Linear color: the sRGB swapchain encodes it on write.
layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inColor, 1.0);
}
