#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

// samplerCube es un tipo especial para leer mapas de cubos
uniform samplerCube skybox;

void main()
{    
    FragColor = texture(skybox, TexCoords);
}