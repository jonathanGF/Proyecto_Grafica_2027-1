#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 WorldPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float waveStrength;

void main() {
    vec3 p = vec3(model * vec4(aPos, 1.0));
    // Ondas largas: misma funcion que alturaOndaAgua() en WaterRenderer.h.
    float h = 0.018 * sin(dot(p.xz, vec2(1.12, 0.56)) - time * 1.15)
            + 0.011 * sin(dot(p.xz, vec2(-0.80, 1.80)) - time * 0.86 + 1.7)
            + 0.006 * sin(dot(p.xz, vec2(2.40, 1.50)) - time * 1.43 + 4.1);
    p.y += h * waveStrength;
    WorldPos = p;
    gl_Position = projection * view * vec4(p, 1.0);
}
