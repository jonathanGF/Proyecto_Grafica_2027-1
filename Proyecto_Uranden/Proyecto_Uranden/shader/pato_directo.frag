#version 330 core
in vec3 Normal;
in vec2 TexCoord;
uniform sampler2D colorTexture;
out vec4 FragColor;
void main() {
    vec3 color = texture(colorTexture,TexCoord).rgb;
    vec3 light = normalize(vec3(-0.4,0.75,0.6));
    float diffuse=max(dot(normalize(Normal),light),0.0);
    FragColor=vec4(color*(0.48 + 0.52*diffuse),1.0);
}
