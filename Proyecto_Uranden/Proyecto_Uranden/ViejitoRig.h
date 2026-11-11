#pragma once
// Rig del viejito con animacion esquelética EN GPU (OpenGL 3.3).
// Lee el mismo Models/ViejitoRig.bin. NO modifica Model.h, Mesh.h ni shader/.
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cmath>

// Angulos de articulaciones (grados). Una pose se calcula por fotograma.
// La deformacion permanece en GPU: aqui solo enviamos matrices de 66 huesos.
struct PoseViejito {
    float musloIzq = 0, musloDer = 0, musloIzqZ = 0, musloDerZ = 0;
    float rodillaIzq = 0, rodillaDer = 0;
    float pieIzq = 0, pieDer = 0;
    float caderaX = 0, caderaZ = 0;
    float espalda = 0, espaldaMedia = 0, espaldaAlta = 0, espaldaLateral = 0;
    float cuello = 0, cabezaY = 0;
    float brazoIzqX = 0, brazoIzqY = 0, brazoIzqZ = 0;
    float codoIzqX = 0, codoIzqY = 0, codoIzqZ = 0;
    float brazoDerX = 0, brazoDerY = 0, brazoDerZ = 0;
    float codoDerX = 0, codoDerY = 0, codoDerZ = 0;
    float munecaDerX = 0, munecaDerZ = 0, agarreDerecho = 0;
};

class ViejitoRig {
public:
    ViejitoRig(const char* rutaRig, const char* rutaTextura)
        : VAO(0), VBO(0), EBO(0), textura(0), programa(0), listo(false), raiz(0) {
        std::ifstream archivo(rutaRig, std::ios::binary);
        if (!archivo) {
            std::cerr << "ERROR VIEJITO RIG: No existe " << rutaRig << std::endl;
            return;
        }
        char firma[8] = {};
        archivo.read(firma, 8);
        if (!archivo || std::memcmp(firma, "VJRG0001", 8) != 0) {
            std::cerr << "ERROR VIEJITO RIG: Formato incompatible." << std::endl;
            return;
        }
        std::uint32_t nVertices = 0, nIndices = 0, nNodos = 0, nHuesos = 0;
        archivo.read(reinterpret_cast<char*>(&nVertices), 4);
        archivo.read(reinterpret_cast<char*>(&nIndices), 4);
        archivo.read(reinterpret_cast<char*>(&nNodos), 4);
        archivo.read(reinterpret_cast<char*>(&nHuesos), 4);
        if (!archivo || nVertices == 0 || nVertices > 1000000 ||
            nIndices == 0 || nIndices > 4000000 || nNodos == 0 || nNodos > 1000 ||
            nHuesos == 0 || nHuesos > MAX_HUESOS || nHuesos > nNodos) {
            std::cerr << "ERROR VIEJITO RIG: Cantidades invalidas." << std::endl;
            return;
        }

        vertices.resize(nVertices);
        archivo.read(reinterpret_cast<char*>(vertices.data()),
            static_cast<std::streamsize>(nVertices) * sizeof(VerticeGPU));
        indices.resize(nIndices);
        archivo.read(reinterpret_cast<char*>(indices.data()),
            static_cast<std::streamsize>(nIndices) * sizeof(std::uint32_t));
        nodos.resize(nNodos);
        for (std::uint32_t i = 0; i < nNodos; ++i) {
            std::int32_t padre = -1, hueso = -1;
            float local[16], inversa[16];
            char nombre[64] = {};
            archivo.read(reinterpret_cast<char*>(&padre), 4);
            archivo.read(reinterpret_cast<char*>(&hueso), 4);
            archivo.read(reinterpret_cast<char*>(local), sizeof(local));
            archivo.read(reinterpret_cast<char*>(inversa), sizeof(inversa));
            archivo.read(nombre, sizeof(nombre));
            nombre[63] = '\0';
            nodos[i].padre = padre;
            nodos[i].hueso = hueso;
            nodos[i].local = glm::make_mat4(local);
            nodos[i].inversa = glm::make_mat4(inversa);
            nodos[i].nombre = nombre;
            if (padre == -1) raiz = static_cast<int>(i);
            else if (padre < 0 || static_cast<std::uint32_t>(padre) >= nNodos) {
                std::cerr << "ERROR VIEJITO RIG: Jerarquia invalida." << std::endl;
                return;
            }
            if (hueso >= static_cast<int>(nHuesos)) {
                std::cerr << "ERROR VIEJITO RIG: Hueso invalido." << std::endl;
                return;
            }
        }
        if (!archivo) {
            std::cerr << "ERROR VIEJITO RIG: Archivo incompleto." << std::endl;
            return;
        }
        for (std::uint32_t i = 0; i < nNodos; ++i)
            if (nodos[i].padre >= 0)
                nodos[nodos[i].padre].hijos.push_back(static_cast<int>(i));
        paleta.assign(nHuesos, glm::mat4(1.0f));

        // El modelo se sube UNA SOLA VEZ a la GPU; no hay glBufferSubData cada paso.
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VerticeGPU), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VerticeGPU), (void*)offsetof(VerticeGPU, p));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VerticeGPU), (void*)offsetof(VerticeGPU, n));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VerticeGPU), (void*)offsetof(VerticeGPU, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, sizeof(VerticeGPU), (void*)offsetof(VerticeGPU, huesos));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VerticeGPU), (void*)offsetof(VerticeGPU, pesos));
        glBindVertexArray(0);

        GLuint vert = compilar(GL_VERTEX_SHADER, vertexSource());
        GLuint frag = compilar(GL_FRAGMENT_SHADER, fragmentSource());
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            std::cerr << "ERROR VIEJITO RIG: Shader GPU no compilado." << std::endl;
            return;
        }
        programa = glCreateProgram();
        glAttachShader(programa, vert);
        glAttachShader(programa, frag);
        glLinkProgram(programa);
        glDeleteShader(vert);
        glDeleteShader(frag);
        GLint ok = GL_FALSE;
        glGetProgramiv(programa, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048] = {};
            glGetProgramInfoLog(programa, sizeof(log), nullptr, log);
            std::cerr << "ERROR VIEJITO RIG: Enlace de shader: " << log << std::endl;
            glDeleteProgram(programa);
            programa = 0;
            return;
        }

        textura = SOIL_load_OGL_texture(rutaTextura, SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
        if (!textura)
            std::cerr << "AVISO VIEJITO RIG: No se cargo textura: " << rutaTextura << std::endl;
        listo = true;
        animarPiernas(0, 0, 0, 0, 0, 0);
        // Referencia de la mano en la pose de descanso original del GLB.
        manoReposo = matrizManoDerecha;
    }

    bool estaListo() const { return listo; }

    // Entrada principal: recibe una pose articulada y actualiza solo 66 matrices.
    void animarPose(const PoseViejito& pose) {
        if (!listo) return;
        for (auto& matriz : paleta) matriz = glm::mat4(1.0f);
        recorrer(raiz, glm::mat4(1.0f), pose);
    }

    // Compatibilidad con pruebas anteriores de piernas.
    void animarPiernas(float izquierda, float derecha,
        float rodillaIzquierda = 0.0f, float rodillaDerecha = 0.0f,
        float pieIzquierdo = 0.0f, float pieDerecho = 0.0f) {
        PoseViejito pose;
        pose.musloIzq = izquierda; pose.musloDer = derecha;
        pose.rodillaIzq = rodillaIzquierda; pose.rodillaDer = rodillaDerecha;
        pose.pieIzq = pieIzquierdo; pose.pieDer = pieDerecho;
        animarPose(pose);
    }

    // Punto de agarre del baston, en coordenadas del modelo sin transformacion global.
    // Se actualiza mientras recorremos el esqueleto, sin calcular vertices en CPU.
    glm::vec3 posicionManoDerecha() const { return manoDerecha; }

    // Transforma un punto en pose original del GLB a la pose actual de
    // la mano derecha, incluyendo rotacion de hombro/codo/muneca/espalda.
    // No recalcula vertices: reutiliza la matriz del hueso ya evaluada.
    glm::mat4 movimientoManoDerecha() const {
        return matrizManoDerecha * glm::inverse(manoReposo);
    }

    // El shader del viejito es propio. No interfiere con miShader del baston/canoa.
    void dibujar(const glm::mat4& modelo, const glm::mat4& vista,
        const glm::mat4& proyeccion) const {
        if (!listo) return;
        glUseProgram(programa);
        glUniformMatrix4fv(glGetUniformLocation(programa, "uModel"), 1, GL_FALSE, glm::value_ptr(modelo));
        glUniformMatrix4fv(glGetUniformLocation(programa, "uView"), 1, GL_FALSE, glm::value_ptr(vista));
        glUniformMatrix4fv(glGetUniformLocation(programa, "uProjection"), 1, GL_FALSE, glm::value_ptr(proyeccion));
        glUniformMatrix4fv(glGetUniformLocation(programa, "uBones[0]"),
            static_cast<GLsizei>(paleta.size()), GL_FALSE, glm::value_ptr(paleta[0]));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textura);
        glUniform1i(glGetUniformLocation(programa, "uTexture"), 0);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void liberar() {
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (textura) glDeleteTextures(1, &textura);
        if (programa) glDeleteProgram(programa);
        EBO = VBO = VAO = textura = programa = 0;
        listo = false;
    }

private:
    static constexpr std::uint32_t MAX_HUESOS = 66;
    struct VerticeGPU {
        float p[3], n[3], uv[2];
        unsigned char huesos[4];
        float pesos[4];
    };
    static_assert(sizeof(VerticeGPU) == 52, "El formato ViejitoRig.bin requiere vertices de 52 bytes");
    struct Nodo {
        int padre = -1, hueso = -1;
        std::string nombre;
        glm::mat4 local = glm::mat4(1.0f), inversa = glm::mat4(1.0f);
        std::vector<int> hijos;
    };
    GLuint VAO, VBO, EBO, textura, programa;
    bool listo;
    int raiz;
    std::vector<VerticeGPU> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Nodo> nodos;
    std::vector<glm::mat4> paleta;
    glm::vec3 manoDerecha = glm::vec3(-0.325f, 0.87f, 0.05f);
    glm::mat4 matrizManoDerecha = glm::mat4(1.0f);
    glm::mat4 manoReposo = glm::mat4(1.0f);

    // Rotaciones locales de los huesos; se mantienen exactamente las jerarquias del rig.
    void recorrer(int id, const glm::mat4& padre, const PoseViejito& p) {
        const Nodo& nodo = nodos[id];
        glm::mat4 local = nodo.local;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        const std::string& nombre = nodo.nombre;

        if (nombre == "mixamorig:Hips") { x = p.caderaX; z = p.caderaZ; }
        else if (nombre == "mixamorig:Spine") { x = p.espalda; }
        else if (nombre == "mixamorig:Spine1") { x = p.espaldaMedia; }
        else if (nombre == "mixamorig:Spine2") { x = p.espaldaAlta; z = p.espaldaLateral; }
        else if (nombre == "mixamorig:Neck") { x = p.cuello; }
        else if (nombre == "mixamorig:Head") { y = p.cabezaY; }
        else if (nombre == "mixamorig:LeftUpLeg") { x = p.musloIzq; z = p.musloIzqZ; }
        else if (nombre == "mixamorig:RightUpLeg") { x = p.musloDer; z = p.musloDerZ; }
        else if (nombre == "mixamorig:LeftLeg") { x = p.rodillaIzq; }
        else if (nombre == "mixamorig:RightLeg") { x = p.rodillaDer; }
        else if (nombre == "mixamorig:LeftFoot") { x = p.pieIzq; }
        else if (nombre == "mixamorig:RightFoot") { x = p.pieDer; }
        else if (nombre == "mixamorig:LeftArm") { x = p.brazoIzqX; y = p.brazoIzqY; z = p.brazoIzqZ; }
        else if (nombre == "mixamorig:LeftForeArm") { x = p.codoIzqX; y = p.codoIzqY; z = p.codoIzqZ; }
        else if (nombre == "mixamorig:RightArm") { x = p.brazoDerX; y = p.brazoDerY; z = p.brazoDerZ; }
        else if (nombre == "mixamorig:RightForeArm") { x = p.codoDerX; y = p.codoDerY; z = p.codoDerZ; }
        else if (nombre == "mixamorig:RightHand") { x = p.munecaDerX; z = p.munecaDerZ; }
        // Curl discreto de los dedos: rodean el eje fino del baston.
        // Se hace sobre sus huesos, sin modificar la malla ni el shader.
        else if (nombre == "mixamorig:RightHandIndex1" ||
            nombre == "mixamorig:RightHandMiddle1" ||
            nombre == "mixamorig:RightHandRing1" ||
            nombre == "mixamorig:RightHandPinky1") {
            x = -24.0f * p.agarreDerecho;
        }
        else if (nombre == "mixamorig:RightHandIndex2" ||
            nombre == "mixamorig:RightHandMiddle2" ||
            nombre == "mixamorig:RightHandRing2" ||
            nombre == "mixamorig:RightHandPinky2") {
            x = -34.0f * p.agarreDerecho;
        }
        else if (nombre == "mixamorig:RightHandThumb1") { x = 12.0f * p.agarreDerecho; }

        if (x != 0.0f) local = glm::rotate(local, glm::radians(x), glm::vec3(1, 0, 0));
        if (y != 0.0f) local = glm::rotate(local, glm::radians(y), glm::vec3(0, 1, 0));
        if (z != 0.0f) local = glm::rotate(local, glm::radians(z), glm::vec3(0, 0, 1));

        glm::mat4 global = padre * local;
        if (nombre == "mixamorig:RightHand") {
            manoDerecha = glm::vec3(global[3]);
            matrizManoDerecha = global;
        }
        if (nodo.hueso >= 0)
            paleta[nodo.hueso] = global * nodo.inversa;
        for (int hijo : nodo.hijos)
            recorrer(hijo, global, p);
    }

    static GLuint compilar(GLenum tipo, const char* fuente) {
        GLuint s = glCreateShader(tipo);
        glShaderSource(s, 1, &fuente, nullptr);
        glCompileShader(s);
        GLint ok = GL_FALSE;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048] = {};
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            std::cerr << "ERROR VIEJITO RIG: Compilar shader: " << log << std::endl;
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    static const char* vertexSource() {
        return R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in uvec4 aBone;
layout(location=4) in vec4 aWeight;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uBones[66];
out vec3 vNormal;
out vec2 vTexCoord;
void main() {
    mat4 skin = mat4(0.0);
    skin += aWeight.x * uBones[aBone.x];
    skin += aWeight.y * uBones[aBone.y];
    skin += aWeight.z * uBones[aBone.z];
    skin += aWeight.w * uBones[aBone.w];
    float sum = aWeight.x + aWeight.y + aWeight.z + aWeight.w;
    if (sum > 0.00001) skin *= (1.0/sum);
    else skin = mat4(1.0);
    vec4 position = uModel * skin * vec4(aPosition,1.0);
    gl_Position = uProjection * uView * position;
    vNormal = normalize(mat3(uModel) * mat3(skin) * aNormal);
    vTexCoord = aTexCoord;
}
)GLSL";
    }

    static const char* fragmentSource() {
        return R"GLSL(#version 330 core
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    vec3 color = texture(uTexture,vTexCoord).rgb;
    vec3 normal = normalize(vNormal);
    vec3 luz = normalize(vec3(0.45,1.0,0.65));
    float luzDifusa = max(dot(normal,luz),0.0);
    FragColor = vec4(color*(0.55 + 0.45*luzDifusa),1.0);
}
)GLSL";
    }
};
