#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec2 TexCoord;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

void main()
{
    // Añadimos movimiento físico a la geometría del agua (Oleaje suave 3D)
    vec3 pos = aPos;
    float waveHeight = sin(pos.x * 2.0 + time * 3.0) * 0.02 + cos(pos.z * 1.5 + time * 2.0) * 0.02;
    pos.y += waveHeight;

    FragPos = vec3(model * vec4(pos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}