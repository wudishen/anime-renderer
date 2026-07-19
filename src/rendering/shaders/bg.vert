#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

uniform vec2 uOffset;
uniform vec2 uScale;

out vec2 vUV;

void main() {
    vec2 pos = aPos * uScale + uOffset;
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = aUV;
}
