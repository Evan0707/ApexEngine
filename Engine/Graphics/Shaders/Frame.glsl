// Per-frame data, descriptor set 0 (decision 0011). Same layout as Graphics/FrameData.h.

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 viewProjection;
    mat4 inverseViewProjection;
    mat4 lightViewProjection;
    vec4 directionToLight;
    vec4 lightRadiance;
    vec4 ambient;
    // x: shadows on, y: shadow map texel, z: start of the fade at the border.
    vec4 shadow;
    // x: exposure multiplier, y: ACES curve on.
    vec4 toneMapping;
    // Irradiance of the environment: nine spherical harmonics coefficients in rgb (decision 0022).
    vec4 irradiance[9];
    // x: image-based lighting on, y: intensity, z: last level of the specular cubemap.
    vec4 environment;
} frame;

struct ObjectData {
    mat4 model;
    mat4 normalMatrix;
    uint materialIndex;
};

// One element per object drawn this frame, in draw order: read with gl_InstanceIndex (G-02).
layout(std430, set = 0, binding = 1) readonly buffer Objects {
    ObjectData objects[];
};
