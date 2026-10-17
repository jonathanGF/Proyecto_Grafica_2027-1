#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NR_POINT_LIGHTS 4

uniform vec3 viewPos;
uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform sampler2D texture_diffuse1;

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor);

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 texColor = vec3(texture(texture_diffuse1, TexCoords));

    // 1. Luz direccional (Luna)
    vec3 result = CalcDirLight(dirLight, norm, viewDir, texColor);

    // 2. Luces puntuales (Velas)
    for(int i = 0; i < NR_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
    }

    FragColor = vec4(result, 1.0);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 ambient  = light.ambient * texColor;
    vec3 diffuse  = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec * 0.2; // Intensidad especular base
    
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    

    vec3 ambient  = light.ambient * texColor;
    vec3 diffuse  = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec * 0.3;

    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}