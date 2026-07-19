#version 450

in vec2 vUV;
uniform sampler2D uTexture;
uniform float uOpacity;

out vec4 FragColor;

void main() {
    vec4 color = texture(uTexture, vUV);
    FragColor = vec4(color.rgb, color.a * uOpacity);
}
