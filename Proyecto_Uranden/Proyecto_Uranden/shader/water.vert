#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

void main()
{
    vec3 pos = aPos;

    // --- ONDAS SUAVES Y NOCTURNAS ---
    // Amplitudes muy bajas (0.015 y 0.01) para un movimiento apenas perceptible y elegante
    float onda1 = sin(pos.x * 0.3 + time * 0.8) * 0.015;
    float onda2 = cos(pos.z * 0.4 + time * 0.6) * 0.010;

    pos.y += (onda1 + onda2);

    // Derivadas suaves para que la luz no queme los colores
    float dYdx = 0.3 * 0.015 * cos(pos.x * 0.3 + time * 0.8);
    float dYdz = -0.4 * 0.010 * sin(pos.z * 0.4 + time * 0.6);

    vec3 newNormal = normalize(vec3(-dYdx, 1.0, -dYdz));

    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = mat3(transpose(inverse(model))) * newNormal;
    TexCoords = aTexCoords;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}