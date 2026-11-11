#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;

uniform vec3 viewPos;
uniform vec3 dirLightDir;
uniform float time;
uniform samplerCube skybox; 

uniform vec3 fogColor;
uniform float fogDensity;

// Función matemática corregida: Frecuencias más bajas y cruce en diagonal (adiós ajedrez)
vec3 calcularOndas(vec2 uv, float t) {
    // Redujimos los multiplicadores de 15.0 a valores entre 1.5 y 2.5
    // También mezclamos (uv.x + uv.y) para que el oleaje fluya en diagonal
    float perturbacionX = sin((uv.x + uv.y * 0.5) * 2.2 + t * 1.5) * 0.05
                        + sin((uv.x * 0.8 - uv.y) * 1.6 - t * 1.1) * 0.03;
    
    float perturbacionZ = cos((uv.y + uv.x * 0.3) * 2.4 + t * 1.4) * 0.05
                        + cos((uv.y * 0.9 - uv.x) * 1.7 - t * 1.2) * 0.03;
                        
    return normalize(vec3(perturbacionX, 1.0, perturbacionZ));
}

void main()
{
    // 1. Generar distorsión procedural natural
    vec3 normalModificada = calcularOndas(FragPos.xz, time);
    
    // 2. Vectores básicos
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(-dirLightDir);
    
    // 3. Efecto Fresnel (Refleja el cielo a lo lejos, transparente cerca)
    float factorFresnel = pow(1.0 - max(dot(normalModificada, viewDir), 0.0), 3.0);
    factorFresnel = clamp(factorFresnel + 0.1, 0.0, 1.0); 
    
    // 4. Reflejo del Skybox (Con distorsión del oleaje)
    vec3 reflectDir = reflect(-viewDir, normalModificada);
    vec3 colorReflejo = texture(skybox, reflectDir).rgb;
    // Tinte cian/azulado oscuro para integrarlo con la noche
    colorReflejo *= vec3(0.4, 0.6, 0.85);

    // 5. Brillo de la luz sobre las olas (Highlights especulares más suaves)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normalModificada, halfwayDir), 0.0), 128.0); 
    vec3 brilloSol = vec3(1.0, 0.95, 0.8) * spec * 1.2; 

    // 6. Color base del agua (Profundo)
    vec3 colorBaseAgua = vec3(0.01, 0.12, 0.18);

    // 7. Mezclar todo
    vec3 colorFinal = mix(colorBaseAgua, colorReflejo, factorFresnel) + brilloSol;

    // 8. Aplicar la niebla atmosférica
    float distance = length(viewPos - FragPos);
    float fogFactor = exp(-pow((distance * fogDensity), 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    colorFinal = mix(fogColor, colorFinal, fogFactor);

    FragColor = vec4(colorFinal, 0.85);
}