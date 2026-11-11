#pragma once
// URANDEN - GESTO NATURAL V3: rig articulado y skinning en GPU OpenGL 3.3.
// Misma jerarquia de huesos del GLB original; el calculo por vertice
// se ejecuta en el vertex shader y NO en la CPU a cada fotograma.
// No requiere cambiar Shader.h, Model.h ni BaileViejitos.cpp.

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <cmath>

struct VerticeFisgaGPU {
    glm::vec3 posicion;
    glm::vec3 normal;
    glm::vec2 uv;
    unsigned char huesos[4];
    float pesos[4];
};

class PescadorFisgaRigGPU {
public:
    PescadorFisgaRigGPU(const char* archivoBin, const char* archivoTextura)
        : vao_(0), vbo_(0), ebo_(0), textura_(0), shader_(0), listo_(false),
        raiz_(-1), mano_(-1) {
        std::ifstream archivo(archivoBin, std::ios::binary);
        if (!archivo) { std::cerr << "RIG GPU: falta " << archivoBin << '\n'; return; }
        char firma[8] = {};
        archivo.read(firma, 8);
        if (!archivo || std::memcmp(firma, "PFRG0001", 8) != 0) {
            std::cerr << "RIG GPU: formato binario incorrecto\n"; return;
        }
        std::uint32_t nv = 0, ni = 0, nn = 0, nh = 0;
        archivo.read(reinterpret_cast<char*>(&nv), 4);
        archivo.read(reinterpret_cast<char*>(&ni), 4);
        archivo.read(reinterpret_cast<char*>(&nn), 4);
        archivo.read(reinterpret_cast<char*>(&nh), 4);
        if (!archivo || nv == 0 || nv > 500000 || ni == 0 || ni > 3000000 ||
            nn == 0 || nn > 200 || nh == 0 || nh > nn || nh > 32) {
            std::cerr << "RIG GPU: cantidades invalidas\n"; return;
        }
        std::vector<VerticeFisgaGPU> vertices(nv);
        for (std::uint32_t i = 0; i < nv; ++i) {
            float p[3], n[3], uv[2];
            archivo.read(reinterpret_cast<char*>(p), sizeof(p));
            archivo.read(reinterpret_cast<char*>(n), sizeof(n));
            archivo.read(reinterpret_cast<char*>(uv), sizeof(uv));
            archivo.read(reinterpret_cast<char*>(vertices[i].huesos), 4);
            archivo.read(reinterpret_cast<char*>(vertices[i].pesos), 4 * sizeof(float));
            vertices[i].posicion = glm::vec3(p[0], p[1], p[2]);
            vertices[i].normal = glm::vec3(n[0], n[1], n[2]);
            vertices[i].uv = glm::vec2(uv[0], uv[1]);
        }
        std::vector<std::uint32_t> indices(ni);
        archivo.read(reinterpret_cast<char*>(indices.data()), ni * sizeof(std::uint32_t));
        nodos_.resize(nn);
        for (std::uint32_t i = 0; i < nn; ++i) {
            std::int32_t padre = 0, hueso = 0;
            float local[16], inversa[16];
            char nombre[64];
            archivo.read(reinterpret_cast<char*>(&padre), 4);
            archivo.read(reinterpret_cast<char*>(&hueso), 4);
            archivo.read(reinterpret_cast<char*>(local), sizeof(local));
            archivo.read(reinterpret_cast<char*>(inversa), sizeof(inversa));
            archivo.read(nombre, sizeof(nombre));
            nombre[63] = '\0';
            if (padre < -1 || padre >= static_cast<int>(nn) ||
                hueso < -1 || hueso >= static_cast<int>(nh)) {
                std::cerr << "RIG GPU: jerarquia incorrecta\n"; return;
            }
            nodos_[i].padre = padre;
            nodos_[i].hueso = hueso;
            nodos_[i].local = glm::make_mat4(local);
            nodos_[i].inversa = glm::make_mat4(inversa);
            nodos_[i].nombre = nombre;
            if (padre == -1) raiz_ = static_cast<int>(i);
            if (nodos_[i].nombre == "mixamorig:RightHand") mano_ = static_cast<int>(i);
        }
        if (!archivo || raiz_ < 0 || mano_ < 0) {
            std::cerr << "RIG GPU: archivo incompleto o falta mano\n"; return;
        }
        for (std::uint32_t i = 0; i < nn; ++i)
            if (nodos_[i].padre >= 0)
                nodos_[nodos_[i].padre].hijos.push_back(static_cast<int>(i));
        paleta_.assign(nh, glm::mat4(1.0f));
        cantidadIndices_ = static_cast<GLsizei>(ni);

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        // Se carga UNA sola vez: no hay glBufferSubData durante la animacion.
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VerticeFisgaGPU),
            vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t),
            indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VerticeFisgaGPU),
            (void*)offsetof(VerticeFisgaGPU, posicion));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VerticeFisgaGPU),
            (void*)offsetof(VerticeFisgaGPU, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VerticeFisgaGPU),
            (void*)offsetof(VerticeFisgaGPU, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, sizeof(VerticeFisgaGPU),
            (void*)offsetof(VerticeFisgaGPU, huesos));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VerticeFisgaGPU),
            (void*)offsetof(VerticeFisgaGPU, pesos));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        shader_ = crearShaderGPU();
        if (!shader_) { std::cerr << "RIG GPU: error compilando shaders\n"; return; }
        textura_ = SOIL_load_OGL_texture(archivoTextura, SOIL_LOAD_AUTO,
            SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
        if (!textura_) {
            std::cerr << "RIG GPU: no se encontro textura " << archivoTextura << '\n';
        }
        listo_ = true;
        animar(0, 0, 0, 0, 0, 0);
    }

    bool estaListo() const { return listo_; }

    // SOLO recalcula 30 transformaciones, no los 128370 vertices.
    void animar(float hombroX, float hombroZ, float codoX,
        float munecaX, float pechoX, float rodillas) {
        if (!listo_) return;
        recorrer(raiz_, glm::mat4(1.0f), hombroX, hombroZ,
            codoX, munecaX, pechoX, rodillas);
    }

    // Compatibilidad: la version anterior de la practica envia 5 parametros.
    // La nueva animacion usa 6, incluyendo la flexion de rodillas.
    void animar(float hombroX, float hombroZ, float codoX,
        float munecaX, float pechoX) {
        animar(hombroX, hombroZ, codoX, munecaX, pechoX, 0.0f);
    }

    glm::vec3 puntoMano() const {
        if (mano_ < 0) return glm::vec3(0.0f);
        return glm::vec3(nodos_[mano_].global * glm::vec4(0.0f, 0.070f, 0.0f, 1.0f));
    }

    // Se usa un shader propio SOLO para el pescador, sin cambiar Shader.h.
    void dibujar(const glm::mat4& model, const glm::mat4& view,
        const glm::mat4& projection) const {
        if (!listo_) return;
        GLint programaAnterior = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &programaAnterior);
        glUseProgram(shader_);
        glUniformMatrix4fv(glGetUniformLocation(shader_, "uModel"), 1,
            GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shader_, "uView"), 1,
            GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader_, "uProjection"), 1,
            GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader_, "uBones[0]"),
            static_cast<GLsizei>(paleta_.size()), GL_FALSE,
            glm::value_ptr(paleta_[0]));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textura_);
        glUniform1i(glGetUniformLocation(shader_, "uTexture"), 0);
        glUniform1i(glGetUniformLocation(shader_, "uHasTexture"), textura_ ? 1 : 0);
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, cantidadIndices_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(static_cast<GLuint>(programaAnterior));
    }

    void liberar() {
        if (ebo_) glDeleteBuffers(1, &ebo_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (textura_) glDeleteTextures(1, &textura_);
        if (shader_) glDeleteProgram(shader_);
        vao_ = vbo_ = ebo_ = textura_ = shader_ = 0;
        listo_ = false;
    }

private:
    struct Nodo {
        int padre = -1;
        int hueso = -1;
        std::string nombre;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 inversa = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        std::vector<int> hijos;
    };
    GLuint vao_, vbo_, ebo_, textura_, shader_;
    bool listo_;
    int raiz_, mano_;
    GLsizei cantidadIndices_ = 0;
    std::vector<Nodo> nodos_;
    std::vector<glm::mat4> paleta_;

    // Gira alrededor de un eje del MUNDO, pero manteniendo la articulacion
    // en su pivote de la malla. Es importante POST-multiplicar la rotacion:
    // de otro modo el hombro/rodilla se desplazaria del sitio.
    static void giroMundo(glm::mat4& local, const glm::mat4& padre,
        float grados, const glm::vec3& ejeMundo) {
        if (std::abs(grados) < 0.0001f) return;
        const glm::mat3 orientacion = glm::mat3(padre * local);
        const glm::vec3 ejeLocal = glm::normalize(glm::transpose(orientacion) * ejeMundo);
        local = glm::rotate(local, glm::radians(grados), ejeLocal);
    }

    void recorrer(int id, const glm::mat4& padre,
        float hombroX, float hombroZ, float codoX,
        float munecaX, float pechoX, float rodillas) {
        Nodo& n = nodos_[id];
        glm::mat4 local = n.local;

        const float carga = std::max(0.0f, std::min(1.0f, (-hombroX - 10.0f) / 75.0f));
        // Impulso se calcula por la inclinacion hacia delante, no por el codo:
        // el calculo anterior dejaba las piernas en modo "impulso" hasta en reposo.
        const float impulso = std::max(0.0f, std::min(1.0f, (-pechoX - 2.0f) / 15.0f));
        const float apertura = std::max(-1.0f, std::min(1.0f, hombroZ / 18.0f));
        const float balanceBrazos = 0.40f * hombroX;
        // Al final del lanzamiento se inclina todavia mas para buscar la fisga.
        // Es 0 durante la carga/impulso y crece en las tres posturas finales.
        // En este modelo los valores POSITIVOS de pechoX inclinan hacia
        // delante (+Z). Antes se usaban negativos y la figura se iba ATRAS.
        const float buscarCaida = std::max(0.0f, std::min(1.0f,
            (pechoX - 8.0f) / 15.0f)) *
            std::max(0.0f, std::min(1.0f, (hombroX + 65.0f) / 30.0f));

        // Pelvis: baja con las rodillas, acompana la carga y luego el impulso.
        if (n.nombre == "mixamorig:Hips") {
            const float bajar = 0.000060f * rodillas * rodillas +
                0.0060f * carga + 0.012f * buscarCaida;
            // La pelvis se acerca ligeramente al agua cuando observa.
            local = glm::translate(local, glm::vec3(0.0f, -bajar,
                -0.008f * carga + 0.006f * impulso + 0.012f * buscarCaida));
            giroMundo(local, padre, 5.0f * carga - 6.0f * impulso +
                5.0f * buscarCaida, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -5.5f * apertura + 4.0f * impulso, glm::vec3(0, 1, 0));
        }

        // Piernas: mas asimetricas para que el gesto no se vea tieso.
        if (n.nombre == "mixamorig:LeftUpLeg")
            giroMundo(local, padre, -0.45f * rodillas + 14.0f * impulso - 4.0f * carga, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:RightUpLeg")
            giroMundo(local, padre, -0.40f * rodillas - 10.0f * impulso + 3.0f * carga, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:LeftLeg")
            giroMundo(local, padre, 0.95f * rodillas - 6.0f * impulso, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:RightLeg")
            giroMundo(local, padre, 1.05f * rodillas + 7.0f * impulso, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:LeftFoot")
            giroMundo(local, padre, -0.45f * rodillas - 7.0f * impulso, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:RightFoot")
            giroMundo(local, padre, -0.50f * rodillas + 6.0f * impulso, glm::vec3(1, 0, 0));

        // Columna: reparte la postura para que desde cintura a cabeza se vea vivo.
        if (n.nombre == "mixamorig:Spine") {
            giroMundo(local, padre, pechoX + 3.0f * carga - 2.0f * impulso, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -3.0f * apertura, glm::vec3(0, 1, 0));
        }
        if (n.nombre == "mixamorig:Spine1") {
            giroMundo(local, padre, 0.55f * pechoX + 1.5f * carga - 1.0f * impulso, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -5.0f * apertura, glm::vec3(0, 1, 0));
        }
        if (n.nombre == "mixamorig:Spine2") {
            giroMundo(local, padre, 0.35f * pechoX, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -4.0f * apertura + 2.0f * impulso, glm::vec3(0, 1, 0));
        }
        // V8: al agacharse para buscar la fisga, compensar la inclinacion
        // de la espalda con el cuello y levantar la cara HACIA DELANTE.
        // buscarCaida aumenta suavemente solo en los keyframes finales;
        // por eso no altera la preparacion, el lanzamiento ni la suelta.
        if (n.nombre == "mixamorig:Neck") {
            giroMundo(local, padre, -0.20f * pechoX - 0.8f * carga - 2.0f * impulso
                - 16.0f * buscarCaida, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -6.0f * apertura + 4.0f * impulso
                - 5.0f * buscarCaida, glm::vec3(0, 1, 0));
        }
        if (n.nombre == "mixamorig:Head") {
            giroMundo(local, padre, 2.0f * apertura - 1.5f * impulso
                - 5.0f * buscarCaida, glm::vec3(0, 1, 0));
            giroMundo(local, padre, -0.10f * pechoX - 6.0f * buscarCaida,
                glm::vec3(1, 0, 0));
        }

        // Brazo de lanzamiento: hombro, codo y mano acompanados por torso.
        if (n.nombre == "mixamorig:RightShoulder") {
            giroMundo(local, padre, 0.18f * hombroX, glm::vec3(1, 0, 0));
            giroMundo(local, padre, 0.35f * apertura * 10.0f, glm::vec3(0, 0, 1));
        }
        if (n.nombre == "mixamorig:RightArm") {
            giroMundo(local, padre, hombroZ, glm::vec3(0, 0, 1));
            giroMundo(local, padre, hombroX, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -3.5f * apertura, glm::vec3(0, 1, 0));
        }
        if (n.nombre == "mixamorig:RightForeArm")
            giroMundo(local, padre, codoX, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:RightHand") {
            giroMundo(local, padre, munecaX, glm::vec3(1, 0, 0));
            giroMundo(local, padre, 0.10f * codoX, glm::vec3(0, 1, 0));
        }

        // Brazo libre: contrapeso natural durante la carga y el lanzamiento.
        if (n.nombre == "mixamorig:LeftShoulder")
            giroMundo(local, padre, -0.10f * hombroX, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:LeftArm") {
            giroMundo(local, padre, -0.22f * hombroX + 0.18f * rodillas + 9.0f * impulso, glm::vec3(1, 0, 0));
            giroMundo(local, padre, -0.55f * hombroZ, glm::vec3(0, 0, 1));
        }
        if (n.nombre == "mixamorig:LeftForeArm")
            giroMundo(local, padre, -10.0f * carga + 6.0f * impulso, glm::vec3(1, 0, 0));
        if (n.nombre == "mixamorig:LeftHand")
            giroMundo(local, padre, 0.06f * balanceBrazos, glm::vec3(0, 1, 0));

        n.global = padre * local;
        if (n.hueso >= 0) paleta_[n.hueso] = n.global * n.inversa;
        for (int hijo : n.hijos)
            recorrer(hijo, n.global, hombroX, hombroZ, codoX, munecaX,
                pechoX, rodillas);
    }
    static GLuint compilar(GLenum tipo, const char* fuente) {
        GLuint s = glCreateShader(tipo);
        glShaderSource(s, 1, &fuente, nullptr);
        glCompileShader(s);
        GLint bien = GL_FALSE;
        glGetShaderiv(s, GL_COMPILE_STATUS, &bien);
        if (bien != GL_TRUE) {
            char error[2048] = {};
            glGetShaderInfoLog(s, sizeof(error), nullptr, error);
            std::cerr << "RIG GPU shader: " << error << '\n';
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    static GLuint crearShaderGPU() {
        const char* vs = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in ivec4 aHuesos;
layout(location=4) in vec4 aPesos;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uBones[32];
out vec3 vNormal;
out vec2 vUV;
void main() {
    mat4 skin = mat4(0.0);
    skin += uBones[aHuesos.x] * aPesos.x;
    skin += uBones[aHuesos.y] * aPesos.y;
    skin += uBones[aHuesos.z] * aPesos.z;
    skin += uBones[aHuesos.w] * aPesos.w;
    vec4 pos = uModel * skin * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * mat3(skin) * aNormal;
    vUV = aUV;
    gl_Position = uProjection * uView * pos;
})GLSL";
        const char* fs = R"GLSL(#version 330 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform int uHasTexture;
void main() {
    vec3 base = uHasTexture != 0 ? texture(uTexture, vUV).rgb : vec3(0.80,0.68,0.52);
    vec3 n = normalize(vNormal);
    float lambert = max(dot(n, normalize(vec3(-0.4,1.0,0.7))), 0.0);
    FragColor = vec4(base * (0.60 + 0.40 * lambert), 1.0);
})GLSL";
        GLuint vert = compilar(GL_VERTEX_SHADER, vs);
        if (!vert) return 0;
        GLuint frag = compilar(GL_FRAGMENT_SHADER, fs);
        if (!frag) { glDeleteShader(vert); return 0; }
        GLuint pr = glCreateProgram();
        glAttachShader(pr, vert);
        glAttachShader(pr, frag);
        glLinkProgram(pr);
        glDeleteShader(vert);
        glDeleteShader(frag);
        GLint ok = GL_FALSE;
        glGetProgramiv(pr, GL_LINK_STATUS, &ok);
        if (ok != GL_TRUE) {
            char error[2048] = {};
            glGetProgramInfoLog(pr, sizeof(error), nullptr, error);
            std::cerr << "RIG GPU link: " << error << '\n';
            glDeleteProgram(pr);
            return 0;
        }
        return pr;
    }
};
