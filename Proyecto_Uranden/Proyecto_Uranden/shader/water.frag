#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 viewPos;
uniform vec3 dirLightDir;

// Parámetros de la niebla atmosférica
uniform vec3 fogColor;
uniform float fogDensity;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Tono azul oscuro del lago de Pátzcuaro en la noche
    vec3 waterColor = vec3(0.04, 0.12, 0.22);

    // Luz lunar difusa
    vec3 lightDir = normalize(-dirLightDir);
    float diff = max(dot(norm, lightDir), 0.0);

    // Reflejo especular (destello lunar sobre las crestas del oleaje)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);

    vec3 ambient = 0.4 * waterColor;
    vec3 diffuse = 0.6 * diff * waterColor;
    vec3 specular = vec3(0.6, 0.7, 0.8) * spec * 0.8;

    vec3 result = ambient + diffuse + specular;

    // Cálculo de niebla exponencial cuadrática para el agua
    float dist = length(viewPos - FragPos);
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 finalColor = mix(fogColor, result, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}