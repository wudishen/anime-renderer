#version 450

in vec3 vKlm;

uniform vec3 uColor;
uniform float uHalfWidth;

out vec4 FragColor;

void main() {
    // Loop-Blinn integral cubic: k^3 - l m = 0
    float f = vKlm.x * vKlm.x * vKlm.x - vKlm.y * vKlm.z;
    float af = fwidth(f);
    float halfWidth = max(uHalfWidth, 0.5) * max(af, 1e-8);
    float alpha = 1.0 - smoothstep(0.0, halfWidth, abs(f));
    if (alpha < 0.01) {
        discard;
    }
    FragColor = vec4(uColor, alpha);
}
