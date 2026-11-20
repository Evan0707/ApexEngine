#version 460

// A full-screen triangle on the far plane: depth 1, behind everything drawn before it.
layout(location = 0) out vec2 outNdc;

void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outNdc = corner * 2.0 - 1.0;
    gl_Position = vec4(outNdc, 1.0, 1.0);
}
