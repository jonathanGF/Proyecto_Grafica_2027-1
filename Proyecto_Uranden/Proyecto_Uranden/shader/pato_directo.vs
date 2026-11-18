#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;
layout(location=3) in ivec4 aBoneIds;
layout(location=4) in vec4 aWeights;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 bones[64];
out vec3 Normal;
out vec2 TexCoord;
void main() {
    mat4 skin = mat4(0.0);
    float total = 0.0;
    for (int i=0; i<4; ++i) {
        if (aWeights[i]>0.0) {
            skin += bones[aBoneIds[i]] * aWeights[i];
            total += aWeights[i];
        }
    }
    if (total < 0.0001) skin=mat4(1.0);
    vec4 p = skin * vec4(aPos,1.0);
    vec4 world = model * p;
    Normal = normalize(mat3(model * skin) * aNormal);
    TexCoord = aTex;
    gl_Position = projection * view * world;
}
