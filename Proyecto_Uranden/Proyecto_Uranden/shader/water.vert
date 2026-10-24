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

    // Superposición de ondas senoidales
    float onda1 = sin(pos.x * 1.2 + time * 1.8) * 0.05;
    float onda2 = cos(pos.z * 0.9 + time * 1.2) * 0.04;
    pos.y += (onda1 + onda2);

    // Derivadas para reconstruir el vector normal según la pendiente del agua
    float dYdx = 1.2 * cos(pos.x * 1.2 + time * 1.8) * 0.05;
    float dYdz = -0.9 * sin(pos.z * 0.9 + time * 1.2) * 0.04;
    vec3 newNormal = normalize(vec3(-dYdx, 1.0, -dYdz));

    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = mat3(transpose(inverse(model))) * newNormal;
    TexCoords = aTexCoords;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}