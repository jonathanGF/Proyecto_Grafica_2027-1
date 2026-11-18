#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// 1. Luz Direccional (Luna)
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// 2. Luz Puntual (Velas, LEDs y focos de los faroles)
struct PointLight {
    vec3 position;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// 3. Reflector (Spotlight)
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NR_POINT_LIGHTS 50

uniform vec3 viewPos;
uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];

// Reflector que ya existia en la escena.
uniform SpotLight spotLight;

// Reflector adicional colocado sobre el mercadito.
// Main.cpp ya envia sus parametros con el prefijo marketSpotLight.
uniform SpotLight marketSpotLight;

uniform bool isLightSource;
uniform bool useTexture;
uniform vec3 objectColor;
uniform float materialAlpha;
uniform sampler2D texture_diffuse1;

// Parametros de niebla
uniform vec3 fogColor;
uniform float fogDensity;

// Prototipos
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor);

void main()
{
    // Los focos visibles se dibujan como emisores y no reciben iluminacion de la escena.
    if (isLightSource) {
        FragColor = vec4(objectColor, 1.0);
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec4 texColorSample = useTexture ? texture(texture_diffuse1, TexCoords) : vec4(1.0);

    if (useTexture && texColorSample.a < 0.10)
        discard;

    vec3 texColor = texColorSample.rgb * objectColor;

    // A. Iluminacion lunar.
    vec3 result = CalcDirLight(dirLight, norm, viewDir, texColor);

    // B. Luces puntuales existentes y cinco nuevos focos calidos de los faroles.
    for (int i = 0; i < NR_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
    }

    // C. Reflector original de la escena.
    result += CalcSpotLight(spotLight, norm, FragPos, viewDir, texColor);

    // D. Nuevo reflector cenital del mercadito.
    result += CalcSpotLight(marketSpotLight, norm, FragPos, viewDir, texColor);

    // E. Niebla atmosferica.
    float dist = length(viewPos - FragPos);
    float fogFactor = exp(-dist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 finalColor = mix(fogColor, result, fogFactor);
    FragColor = vec4(finalColor, 1.0);
}

// Luz direccional
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 ambient  = light.ambient * texColor;
    vec3 diffuse  = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec * 0.2;
    return ambient + diffuse + specular;
}

// Luz puntual
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    float distance = length(light.position - fragPos);
    float denom = light.constant + light.linear * distance + light.quadratic * (distance * distance);
    if (denom <= 0.0001) denom = 1.0;
    float attenuation = 1.0 / denom;

    vec3 ambient  = light.ambient * texColor;
    vec3 diffuse  = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec * 0.2;

    return (ambient + diffuse + specular) * attenuation;
}

// Reflector
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    float distance = length(light.position - fragPos);
    float denom = light.constant + light.linear * distance + light.quadratic * (distance * distance);
    if (denom <= 0.0001) denom = 1.0;
    float attenuation = 1.0 / denom;

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / (abs(epsilon) > 0.0001 ? epsilon : 1.0), 0.0, 1.0);

    vec3 ambient  = light.ambient * texColor;
    vec3 diffuse  = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec * 0.5;

    return (ambient + diffuse + specular) * attenuation * intensity;
}
