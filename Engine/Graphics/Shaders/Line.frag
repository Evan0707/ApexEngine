#version 460

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Linear color, encoded by the sRGB swapchain: drawn after tonemapping, unchanged by the exposure.
    outColor = inColor;
}
