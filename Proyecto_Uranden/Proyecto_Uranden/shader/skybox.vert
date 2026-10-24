#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    // Eliminamos la traslación de la matriz de vista usando mat3
    mat4 rotView = mat4(mat3(view)); 
    vec4 pos = projection * rotView * vec4(aPos, 1.0);
    
    // Forzamos la profundidad al valor máximo (1.0)
    gl_Position = pos.xyww;
}