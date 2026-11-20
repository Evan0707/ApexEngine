// Shadow of the directional light (spec 5.4, decision 0021). Needs Frame.glsl.

// Depth comparison sampler: 1 where the compared depth is not behind the stored one, 1 outside the map.
layout(set = 0, binding = 3) uniform sampler2DShadow shadowMap;

// 1 in full light, 0 in full shadow. position: render space. PCF 3 x 3, each tap itself a 2 x 2 comparison
// when the sampler filters linearly. Fades to 1 near the border of the area, and is 1 outside it.
float shadowVisibility(vec3 position) {
    if (frame.shadow.x == 0.0) {
        return 1.0;
    }
    vec4 clip = frame.lightViewProjection * vec4(position, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    float edge = max(abs(ndc.x), abs(ndc.y));
    if (edge >= 1.0 || ndc.z <= 0.0 || ndc.z >= 1.0) {
        return 1.0;
    }
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float texel = frame.shadow.y;
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            lit += texture(shadowMap, vec3(uv + vec2(x, y) * texel, ndc.z));
        }
    }
    return mix(lit / 9.0, 1.0, smoothstep(frame.shadow.z, 1.0, edge));
}
