#version 460

// One full-screen triangle per face and level of the cubemap: firstInstance is level * 6 + face.
layout(location = 0) flat out uint outDraw;

void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    outDraw = gl_InstanceIndex;
}
