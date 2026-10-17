#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform vec3 viewPos;
uniform DirLight dirLight;
uniform vec3 objectColor;

void main()
{
    // Vectores unitarios de normal y vista
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // 1. Componente Ambiental (luz residual nocturna)
    vec3 ambient = dirLight.ambient * objectColor;

    // 2. Componente Difusa (incidencia de la luz lunar)
    vec3 lightDir = normalize(-dirLight.direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = dirLight.diffuse * diff * objectColor;

    // 3. Componente Especular (reflejo Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular = dirLight.specular * spec;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}