// Per-frame data, descriptor set 0 (decision 0011). Same layout as Graphics/FrameData.h.

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 viewProjection;
    vec4 directionToLight;
    vec4 lightRadiance;
    vec4 ambient;
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
