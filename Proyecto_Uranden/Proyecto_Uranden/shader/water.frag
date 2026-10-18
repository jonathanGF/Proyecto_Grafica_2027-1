#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 viewPos;
uniform vec3 dirLightDir;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Tono azul oscuro del lago de Pátzcuaro en la noche
    vec3 waterColor = vec3(0.04, 0.12, 0.22);

    // Luz lunar difusa
    vec3 lightDir = normalize(-dirLightDir);
    float diff = max(dot(norm, lightDir), 0.0);

    // Reflejo especular (bajamos el exponente a 32.0 para que el destello lunar se disperse en las olas)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);

    vec3 ambient = 0.4 * waterColor;
    vec3 diffuse = 0.6 * diff * waterColor;
    vec3 specular = vec3(0.6, 0.7, 0.8) * spec * 0.8;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}