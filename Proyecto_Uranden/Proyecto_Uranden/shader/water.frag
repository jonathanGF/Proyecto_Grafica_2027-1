#version 330 core
in vec3 WorldPos;
out vec4 FragColor;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 inverseProjection;
uniform vec3 viewPos;
uniform vec3 dirLightDir;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float time;
uniform float waveStrength;
uniform float clarity;
uniform float reflectionStrength;
uniform float waterLevel;
uniform vec2 screenSize;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform samplerCube skybox;
uniform bool hasSkybox;
uniform int reflectionSteps;
uniform int lightCount;
uniform vec3 lightPositions[20];
uniform vec3 lightColors[20];
const float PI = 3.14159265;

vec3 positionAt(vec2 uv, float depth) {
    vec4 p = inverseProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}
float linearDepth(float d) {
    // Planos de recorte del Main: near=0.1, far=100.
    return 20.0 / (100.1 - (d * 2.0 - 1.0) * 99.9);
}
vec2 slope(vec2 p, vec2 k, float amplitude, float speed, float phase) {
    float a = dot(p, k) - time * speed + phase;
    // Filtra las ondulaciones que ya ocupan menos de unos pixeles.
    float footprint = length(vec2(dFdx(a), dFdy(a)));
    float filterWeight = 1.0 - smoothstep(0.65, 2.8, footprint);
    return k * amplitude * cos(a) * filterWeight;
}
vec3 surfaceNormal() {
    vec2 p = WorldPos.xz;
    vec2 g = slope(p, vec2(1.12, 0.56), 0.018, 1.15, 0.0)
           + slope(p, vec2(-0.80, 1.80), 0.011, 0.86, 1.7)
           + slope(p, vec2(2.40, 1.50), 0.006, 1.43, 4.1);
    // Detalle de superficie independiente de la cantidad de triangulos.
    vec2 warp = 0.065 * vec2(sin(p.y * 1.7 + time * 0.3), cos(p.x * 1.4 - time * 0.2));
    g += slope(p + warp, vec2(6.8, 3.1), 0.010, 1.7, 0.9);
    g += slope(p - warp, vec2(-10.2, 7.6), 0.006, 2.1, 2.8);
    g += slope(p, vec2(18.5, 11.2), 0.0026, 2.8, 4.2);
    g += slope(p, vec2(-25.3, 20.7), 0.0012, 3.3, 1.3);
    return normalize(vec3(-g.x * waveStrength, 1.0, -g.y * waveStrength));
}
vec3 environment(vec3 r) {
    // Relleno tenue: el cubemap original puede estar casi negro.
    float up = smoothstep(-0.12, 0.85, r.y);
    vec3 sky = mix(vec3(0.105, 0.125, 0.130), vec3(0.025, 0.040, 0.060), up);
    if (hasSkybox) sky = mix(sky, texture(skybox, r).rgb, 0.72);
    return sky;
}
// Reflejos de geometria visible. No vuelve a dibujar el mapa.
vec4 screenReflection(vec3 origin, vec3 direction) {
    if (reflectionSteps == 0) return vec4(0.0);
    vec3 start = origin + direction * 0.10;
    float previousT = 0.0;
    float previousDelta = -1.0;
    for (int i = 0; i < 40; ++i) {
        if (i >= reflectionSteps) break;
        float f = float(i + 1) / float(reflectionSteps);
        float t = 0.06 + f * f * 22.0;
        vec3 p = start + direction * t;
        if (p.z >= -0.11) break;
        vec4 clip = projection * vec4(p, 1.0);
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.005))) || any(greaterThan(uv, vec2(0.995)))) break;
        float depth = texture(sceneDepth, uv).r;
        float delta = -p.z - linearDepth(depth);
        if (depth < 0.99999 && delta > 0.0 && previousDelta <= 0.0) {
            float lo = previousT, hi = t;
            for (int j = 0; j < 5; ++j) {
                float mid = (lo + hi) * 0.5;
                vec3 q = start + direction * mid;
                vec4 c = projection * vec4(q, 1.0);
                vec2 v = c.xy / c.w * 0.5 + 0.5;
                float dz = -q.z - linearDepth(texture(sceneDepth, v).r);
                if (dz > 0.0) hi = mid; else lo = mid;
            }
            vec3 q = start + direction * hi;
            vec4 c = projection * vec4(q, 1.0);
            vec2 v = c.xy / c.w * 0.5 + 0.5;
            float sampledDepth = texture(sceneDepth, v).r;
            float error = -q.z - linearDepth(sampledDepth);
            float edge = min(min(v.x, 1.0-v.x), min(v.y, 1.0-v.y));
            float valid = (sampledDepth < 0.99999 && error >= -0.015 && error < 0.16) ? 1.0 : 0.0;
            vec3 hitWorld = transpose(mat3(view)) * positionAt(v, sampledDepth) + viewPos;
            valid *= step(waterLevel - 0.015, hitWorld.y); // No reflejar el fondo sumergido.
            float confidence = valid * smoothstep(0.01, 0.12, edge) * (1.0 - smoothstep(16.0, 22.0, hi));
            return vec4(texture(sceneColor, clamp(v, vec2(0.001), vec2(0.999))).rgb, confidence);
        }
        previousDelta = delta;
        previousT = t;
    }
    return vec4(0.0);
}
float highlight(vec3 n, vec3 v, vec3 l, float exponent) {
    vec3 h = normalize(v + l);
    return pow(max(dot(n, h), 0.0), exponent) * max(dot(n, l), 0.0);
}
void main() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    vec3 pv = (view * vec4(WorldPos, 1.0)).xyz;
    float depth = texture(sceneDepth, uv).r;
    vec3 background = texture(sceneColor, uv).rgb;
    float thickness = depth < 0.99999 ? max(length(positionAt(uv, depth) - pv), 0.0) : 30.0;
    float edgeBlend = smoothstep(0.0, 0.085, thickness);
    vec3 n = surfaceNormal();
    vec3 v = normalize(viewPos - WorldPos);
    if (dot(n, v) < 0.0) n = -n;
    vec3 nv = mat3(view) * n;
    vec2 refractionUV = uv + nv.xy * (0.020 / max(-pv.z * 0.12, 1.0)) * smoothstep(0.0, 0.35, thickness);
    refractionUV = clamp(refractionUV, vec2(0.001), vec2(0.999));
    float refractedDepth = texture(sceneDepth, refractionUV).r;
    // No arrastrar la orilla ni objetos que esten delante de la superficie.
    if (linearDepth(refractedDepth) <= -pv.z + 0.025 || refractedDepth >= 0.99999) {
        refractionUV = uv;
        refractedDepth = depth;
    }
    float pathLength = refractedDepth < 0.99999 ? length(positionAt(refractionUV, refractedDepth) - pv) : 30.0;
    // Agua turbia: oculta el fondo y conserva los reflejos de la superficie.
    vec3 transmission = vec3(0.0);
    // Verde grisaceo de lago; se evita el azul saturado uniforme.
    // Verde oscuro ligeramente grisaceo, apropiado para el lago nocturno.
    vec3 deepColor = vec3(0.035, 0.075, 0.065);
    vec3 refracted = texture(sceneColor, refractionUV).rgb * transmission + deepColor * (1.0 - transmission);
    float fresnel = 0.020 + 0.980 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
    vec3 r = reflect(-v, n);
    vec3 reflected = environment(r);
    vec4 hit = screenReflection(pv + nv * 0.045, normalize(mat3(view) * r));
    reflected = mix(reflected, hit.rgb, hit.a);
    vec3 color = mix(refracted, reflected, clamp(fresnel * reflectionStrength, 0.0, 0.96));
    vec3 moonDir = normalize(-dirLightDir);
    color += vec3(0.30, 0.33, 0.35) * highlight(n, v, moonDir, 160.0) * 0.65;
    color += vec3(0.045, 0.052, 0.055) * highlight(n, v, moonDir, 24.0);
    for (int i = 0; i < 20; ++i) {
        if (i >= lightCount) break;
        vec3 toLight = lightPositions[i] - WorldPos;
        float d2 = dot(toLight, toLight);
        if (d2 > 81.0 || lightPositions[i].y < waterLevel - 0.02) continue;
        float d = sqrt(max(d2, 0.0001));
        vec3 l = toLight / d;
        float attenuation = (1.0 - smoothstep(36.0, 81.0, d2)) / (1.0 + 0.32 * d + 0.38 * d2);
        // Dos lobulos para destellos finos y una estela de luz menos rigida.
        float spec = 0.85 * highlight(n, v, l, 100.0) + 0.12 * highlight(n, v, l, 18.0);
        color += lightColors[i] * spec * attenuation;
    }
    // Banda tenue e irregular de contacto, no espuma oceanica blanca.
    float shore = (1.0 - smoothstep(0.04, 0.23, thickness)) * smoothstep(0.0, 0.035, thickness);
    float broken = 0.5 + 0.5 * sin(WorldPos.x * 17.0 + sin(WorldPos.z * 13.0) + time * 0.8);
    color += vec3(0.065, 0.075, 0.068) * shore * broken * 0.35;
    float fog = exp(-pow(length(viewPos - WorldPos) * fogDensity, 2.0));
    color = mix(fogColor, color, clamp(fog, 0.0, 1.0));
    // Composicion ya calculada con la escena; alpha=1 evita mezclarla dos veces.
    FragColor = vec4(mix(background, color, edgeBlend), 1.0);
}
