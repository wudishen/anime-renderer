#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aKlm;
layout(location = 2) in float aFScale;

uniform mat4 uMVP;

out vec3 vKlm;
out float vFScale;

void main() {
    vKlm = aKlm;
    vFScale = aFScale;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
