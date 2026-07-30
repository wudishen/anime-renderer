#version 450

in vec3 vKlm;
in float vFScale;

uniform vec3 uColor;
uniform float uHalfWidth;

out vec4 FragColor;

void main() {
    // Loop-Blinn integral cubic: k^3 - l m = 0
    float f = vKlm.x * vKlm.x * vKlm.x - vKlm.y * vKlm.z;
    // CPU per-leaf |df/dn| avoids unstable fwidth on tiny subdivided triangles.
    float pixelDist = abs(f) / max(vFScale, 1e-8);
    float halfWidthPx = max(uHalfWidth, 0.5);
    float alpha = 1.0 - smoothstep(0.0, halfWidthPx, pixelDist);
    if (alpha < 0.01) {
        discard;
    }
    FragColor = vec4(uColor, alpha);
}
