#version 460

// The specular cubemap of an environment (decision 0022): each level for a roughness, from 0 at the first
// level to 1 at the last, by GGX importance sampling with the view along the normal (Karis). Each sample
// reads a level of the source chosen from its solid angle (filtered importance sampling).

const float PI = 3.14159265358979;

layout(set = 0, binding = 0) uniform sampler2D source;
layout(set = 0, binding = 1) uniform PrefilterParams {
    uint faceSize;
    uint levels;
    uint samples;
    float sourceTexelSolidAngle;
} params;

layout(location = 0) flat in uint inDraw;
layout(location = 0) out vec4 outColor;

// Same convention as resources::environmentUv: u turns around +Y, 0.5 towards -Z; v from +Y to -Y.
vec2 environmentUv(vec3 direction) {
    return vec2(0.5 + atan(direction.x, -direction.z) / (2.0 * PI), acos(clamp(direction.y, -1.0, 1.0)) / PI);
}

// The direction through a point of a cube face, st in [-1, 1] with t growing down the face (Vulkan's cube
// map faces: +X, -X, +Y, -Y, +Z, -Z).
vec3 cubeFaceDirection(uint face, vec2 st) {
    switch (face) {
    case 0:
        return normalize(vec3(1.0, -st.y, -st.x));
    case 1:
        return normalize(vec3(-1.0, -st.y, st.x));
    case 2:
        return normalize(vec3(st.x, 1.0, st.y));
    case 3:
        return normalize(vec3(st.x, -1.0, -st.y));
    case 4:
        return normalize(vec3(st.x, -st.y, 1.0));
    default:
        return normalize(vec3(-st.x, -st.y, -1.0));
    }
}

void main() {
    uint level = inDraw / 6;
    uint face = inDraw % 6;
    float size = float(max(params.faceSize >> level, 1u));
    vec3 n = cubeFaceDirection(face, gl_FragCoord.xy / size * 2.0 - 1.0);
    if (level == 0) {
        outColor = vec4(textureLod(source, environmentUv(n), 0.0).rgb, 1.0);
        return;
    }
    float roughness = float(level) / float(params.levels - 1);
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, n));
    vec3 tangentY = cross(n, tangentX);

    vec3 sum = vec3(0.0);
    float weight = 0.0;
    for (uint i = 0; i < params.samples; ++i) {
        // Hammersley set.
        vec2 xi = vec2((float(i) + 0.5) / float(params.samples), float(bitfieldReverse(i)) * 2.3283064365386963e-10);
        float phi = 2.0 * PI * xi.x;
        float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha2 - 1.0) * xi.y));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        vec3 h = tangentX * (sinTheta * cos(phi)) + tangentY * (sinTheta * sin(phi)) + n * cosTheta;
        vec3 l = 2.0 * dot(n, h) * h - n;
        float nDotL = dot(n, l);
        if (nDotL <= 0.0) {
            continue;
        }
        // With the view along the normal, the probability of l is D(h) / 4.
        float denominator = cosTheta * cosTheta * (alpha2 - 1.0) + 1.0;
        float pdf = alpha2 / (PI * denominator * denominator) / 4.0;
        float sampleSolidAngle = 1.0 / (float(params.samples) * pdf + 1e-6);
        float lod = max(0.5 * log2(sampleSolidAngle / params.sourceTexelSolidAngle) + 1.0, 0.0);
        sum += textureLod(source, environmentUv(l), lod).rgb * nDotL;
        weight += nDotL;
    }
    outColor = vec4(sum / max(weight, 1e-4), 1.0);
}
