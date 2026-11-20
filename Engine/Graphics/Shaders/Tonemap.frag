#version 460

#include "Frame.glsl"

// The HDR image of the scene, same size as the target: read texel by texel.
layout(set = 1, binding = 0) uniform sampler2D sceneColor;

layout(location = 0) out vec4 outColor;

// Narkowicz's fit of the ACES filmic curve (spec 5.4): linear in, linear out, in [0, 1].
vec3 acesApproximation(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0).rgb * frame.toneMapping.x;
    vec3 color = frame.toneMapping.y > 0.5 ? acesApproximation(hdr) : clamp(hdr, 0.0, 1.0);
    // Linear color, encoded by the sRGB swapchain.
    outColor = vec4(color, 1.0);
}
