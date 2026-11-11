#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 VertexColor;
uniform bool streamMap;
uniform int mapAlphaMode; // 0 opaco, 1 recorte, 2 mezcla
uniform float mapAlphaCutoff;
uniform float streamFogStart;
uniform float streamFogEnd;

// 1. Luz Direccional (Luna)
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// 2. Luz Puntual (Velas y LEDs)
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
uniform SpotLight spotLight;
uniform bool isLightSource;

uniform bool useTexture;
uniform vec3 objectColor;
uniform float materialAlpha;
uniform sampler2D texture_diffuse1;

// Parámetros de niebla
uniform vec3 fogColor;
uniform float fogDensity;

// Prototipos
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor);

void main()
{
    // --- EVALUACIÓN DE FUENTE EMISIVA ---
    // Si el objeto es un foco físico, emite su color directo sin cálculos de luz ni sombras
    if (isLightSource) {
        FragColor = vec4(objectColor, 1.0);
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Lectura de textura segura
    vec4 texColorSample = useTexture ? texture(texture_diffuse1, TexCoords) : vec4(1.0);
    
    // Descartar fragmentos con transparencia real
    float alpha = texColorSample.a * materialAlpha;
    if (streamMap) {
        alpha *= VertexColor.a;
        if (mapAlphaMode == 1 && alpha < mapAlphaCutoff) discard;
        if (mapAlphaMode != 2) alpha = 1.0;
    } else {
        if (useTexture && texColorSample.a < 0.10) discard;
        alpha = 1.0; // Comportamiento previo de canoa/focos.
    }

    vec3 texColor = texColorSample.rgb * objectColor * (streamMap ? VertexColor.rgb : vec3(1.0));

    // A. Luz lunar
    vec3 result = CalcDirLight(dirLight, norm, viewDir, texColor);

    // B. Luces puntuales
    for(int i = 0; i < NR_POINT_LIGHTS; i++) {
        // Las cuatro luces apagadas y las lejanas no calculan especular.
        vec3 intensity = pointLights[i].ambient + pointLights[i].diffuse + pointLights[i].specular;
        vec3 delta = pointLights[i].position - FragPos;
        float d2 = dot(delta, delta);
        if (dot(intensity, intensity) > 0.000001 && d2 < 256.0) {
            float fade = 1.0 - smoothstep(10.0, 16.0, sqrt(d2));
            result += fade * CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
        }
    }

    // C. Reflector cenital
    result += CalcSpotLight(spotLight, norm, FragPos, viewDir, texColor);

    // D. Niebla atmosférica
    float dist = length(viewPos - FragPos);
    float fogFactor = exp(-dist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    if (streamMap) {
        // Transicion hacia el color de fondo antes del limite de dibujado.
        fogFactor *= 1.0 - smoothstep(streamFogStart, streamFogEnd, dist);
    }
    vec3 finalColor = mix(fogColor, result, fogFactor);
    FragColor = vec4(finalColor, alpha);
}

// Luz Direccional
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 ambient  = light.ambient  * texColor;
    vec3 diffuse  = light.diffuse  * diff * texColor;
    vec3 specular = light.specular * spec * 0.2;
    return (ambient + diffuse + specular);
}

// Luz Puntual
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

    vec3 ambient  = light.ambient  * texColor;
    vec3 diffuse  = light.diffuse  * diff * texColor;
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

    vec3 ambient  = light.ambient  * texColor;
    vec3 diffuse  = light.diffuse  * diff * texColor;
    vec3 specular = light.specular * spec * 0.5;

    return (ambient + diffuse + specular) * attenuation * intensity;
}