#pragma once

// Modulo header-only para integrar la secuencia automatica del pato
// dentro del ciclo de renderizado de Main.cpp.
// No contiene main() ni crea una ventana GLFW independiente.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"

namespace PatoSecuencia {

    inline constexpr unsigned int MAX_HUESOS = 64;

    inline bool ExisteArchivo(const std::string& nombre) {
        std::ifstream f(nombre.c_str(), std::ios::binary);
        return f.good();
    }

    inline std::string RutaRecurso(const std::string& nombre) {
        const char* prefijos[] = {
            "",
            "Proyecto_Uranden/",
            "../Proyecto_Uranden/",
            "../",
            "../../Proyecto_Uranden/"
        };

        for (const char* prefijo : prefijos) {
            const std::string candidato = std::string(prefijo) + nombre;
            if (ExisteArchivo(candidato)) {
                return candidato;
            }
        }

        std::cerr << "[PATO] Falta archivo: " << nombre << '\n';
        return nombre;
    }

    struct VerticePato {
        float posicion[3];
        float normal[3];
        float uv[2];
        int idHuesos[4];
        float pesos[4];
    };

    static_assert(sizeof(VerticePato) == 64,
        "Formato de vertice incompatible; se requieren 64 bytes");
    static_assert(sizeof(glm::mat4) == 64,
        "Formato de matrices incompatible; se requieren 64 bytes");

    inline std::string LeerTextoShader(const std::string& archivo) {
        std::ifstream f(archivo.c_str(), std::ios::binary);
        if (!f) {
            std::cerr << "[PATO] No se pudo abrir shader: " << archivo << '\n';
            return {};
        }
        return std::string(
            std::istreambuf_iterator<char>(f),
            std::istreambuf_iterator<char>());
    }

    inline GLuint CompilarShader(GLenum tipo, const std::string& codigo) {
        GLuint shader = glCreateShader(tipo);
        const char* ptr = codigo.c_str();
        glShaderSource(shader, 1, &ptr, nullptr);
        glCompileShader(shader);

        GLint correcto = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &correcto);
        if (correcto != GL_TRUE) {
            GLint largo = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &largo);
            std::vector<char> log(static_cast<size_t>(std::max(largo, 1)), 0);
            glGetShaderInfoLog(shader, largo, nullptr, log.data());
            std::cerr << "[PATO] ERROR AL COMPILAR SHADER: " << log.data() << '\n';
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    inline GLuint CrearProgramaGL(const std::string& vertexPath,
        const std::string& fragmentPath) {
        const std::string vertex = LeerTextoShader(vertexPath);
        const std::string fragment = LeerTextoShader(fragmentPath);
        if (vertex.empty() || fragment.empty()) return 0;

        GLuint vs = CompilarShader(GL_VERTEX_SHADER, vertex);
        if (!vs) return 0;

        GLuint fs = CompilarShader(GL_FRAGMENT_SHADER, fragment);
        if (!fs) {
            glDeleteShader(vs);
            return 0;
        }

        GLuint programa = glCreateProgram();
        glAttachShader(programa, vs);
        glAttachShader(programa, fs);
        glLinkProgram(programa);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint correcto = GL_FALSE;
        glGetProgramiv(programa, GL_LINK_STATUS, &correcto);
        if (correcto != GL_TRUE) {
            GLint largo = 0;
            glGetProgramiv(programa, GL_INFO_LOG_LENGTH, &largo);
            std::vector<char> log(static_cast<size_t>(std::max(largo, 1)), 0);
            glGetProgramInfoLog(programa, largo, nullptr, log.data());
            std::cerr << "[PATO] ERROR AL ENLAZAR SHADERS: " << log.data() << '\n';
            glDeleteProgram(programa);
            return 0;
        }

        return programa;
    }

    class PatoAnimado {
    public:
        PatoAnimado()
            : vao(0), vbo(0), ebo(0), indiceTotal(0),
            huesos(0), frames(0), fps(60.0f), ok(false) {
        }

        PatoAnimado(const PatoAnimado&) = delete;
        PatoAnimado& operator=(const PatoAnimado&) = delete;

        ~PatoAnimado() {
            // Main.cpp llama liberar() antes de glfwTerminate().
            // Esta segunda llamada es segura porque los identificadores quedan en cero.
            liberar();
        }

        void liberar() {
            if (ebo) glDeleteBuffers(1, &ebo);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (vao) glDeleteVertexArrays(1, &vao);

            vao = 0;
            vbo = 0;
            ebo = 0;
            indiceTotal = 0;
            huesos = 0;
            frames = 0;
            fps = 60.0f;
            ok = false;
            matrices.clear();
        }

        bool cargar(const std::string& archivo) {
            liberar();

            std::ifstream f(archivo.c_str(), std::ios::binary);
            if (!f) {
                std::cerr << "[PATO] No se pudo abrir: " << archivo << '\n';
                return false;
            }

            char magic[8] = {};
            f.read(magic, 8);
            if (!f || std::memcmp(magic, "PATOAN1\0", 8) != 0) {
                std::cerr << "[PATO] Formato incorrecto: " << archivo << '\n';
                return false;
            }

            std::uint32_t nVert = 0, nInd = 0, nBones = 0, nFrames = 0;
            f.read(reinterpret_cast<char*>(&nVert), 4);
            f.read(reinterpret_cast<char*>(&nInd), 4);
            f.read(reinterpret_cast<char*>(&nBones), 4);
            f.read(reinterpret_cast<char*>(&nFrames), 4);
            f.read(reinterpret_cast<char*>(&fps), 4);

            if (!f || nVert < 3 || nVert > 1000000 ||
                nInd < 3 || nInd > 5000000 ||
                nBones == 0 || nBones > MAX_HUESOS ||
                nFrames < 2 || nFrames > 10000 ||
                !std::isfinite(fps) || fps <= 0.0f) {
                std::cerr << "[PATO] Encabezado invalido: " << archivo << '\n';
                return false;
            }

            std::vector<VerticePato> vertices(nVert);
            std::vector<std::uint32_t> indices(nInd);
            matrices.resize(static_cast<size_t>(nBones) * nFrames);

            f.read(reinterpret_cast<char*>(vertices.data()),
                static_cast<std::streamsize>(nVert * sizeof(VerticePato)));
            f.read(reinterpret_cast<char*>(indices.data()),
                static_cast<std::streamsize>(nInd * sizeof(std::uint32_t)));
            f.read(reinterpret_cast<char*>(matrices.data()),
                static_cast<std::streamsize>(matrices.size() * sizeof(glm::mat4)));

            if (!f) {
                std::cerr << "[PATO] Archivo incompleto: " << archivo << '\n';
                matrices.clear();
                return false;
            }

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(nVert * sizeof(VerticePato)),
                vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(nInd * sizeof(std::uint32_t)),
                indices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                sizeof(VerticePato), (void*)offsetof(VerticePato, posicion));

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                sizeof(VerticePato), (void*)offsetof(VerticePato, normal));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                sizeof(VerticePato), (void*)offsetof(VerticePato, uv));

            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 4, GL_INT, sizeof(VerticePato),
                (void*)offsetof(VerticePato, idHuesos));

            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE,
                sizeof(VerticePato), (void*)offsetof(VerticePato, pesos));

            glBindVertexArray(0);

            indiceTotal = static_cast<GLsizei>(nInd);
            huesos = nBones;
            frames = nFrames;
            ok = true;


            return true;
        }

        bool listo() const { return ok; }

        float duracion() const {
            return (frames > 1 && fps > 0.0f)
                ? float(frames - 1) / fps
                : 0.0f;
        }

        void dibujar(GLuint programa, float segundos, GLuint textura,
            const glm::mat4& modelo, const glm::mat4& vista,
            const glm::mat4& proyeccion, bool repetir) const {
            if (!ok || !programa) return;

            const float duracionClip = static_cast<float>(frames - 1) / fps;
            const float t = repetir
                ? std::fmod(std::max(segundos, 0.0f), duracionClip)
                : std::max(0.0f, std::min(segundos, duracionClip));

            const unsigned int frame = std::min(
                static_cast<unsigned int>(t * fps + 0.0001f), frames - 1);

            glUseProgram(programa);
            glUniformMatrix4fv(glGetUniformLocation(programa, "model"),
                1, GL_FALSE, glm::value_ptr(modelo));
            glUniformMatrix4fv(glGetUniformLocation(programa, "view"),
                1, GL_FALSE, glm::value_ptr(vista));
            glUniformMatrix4fv(glGetUniformLocation(programa, "projection"),
                1, GL_FALSE, glm::value_ptr(proyeccion));
            glUniformMatrix4fv(glGetUniformLocation(programa, "bones[0]"),
                static_cast<GLsizei>(huesos), GL_FALSE,
                glm::value_ptr(matrices[static_cast<size_t>(frame) * huesos]));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textura);
            glUniform1i(glGetUniformLocation(programa, "colorTexture"), 0);

            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, indiceTotal, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

    private:
        GLuint vao, vbo, ebo;
        GLsizei indiceTotal;
        unsigned int huesos, frames;
        float fps;
        bool ok;
        std::vector<glm::mat4> matrices;
    };

    inline GLuint CargarTextura(const std::string& archivo) {
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = SOIL_load_image(
            archivo.c_str(), &w, &h, &channels, SOIL_LOAD_RGBA);

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        if (pixels && w > 0 && h > 0) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            SOIL_free_image_data(pixels);
        }
        else {
            const unsigned char naranja[] = { 245, 175, 75, 255 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, naranja);
            std::cerr << "[PATO] Aviso: textura provisional; no se pudo cargar PNG\n";
        }

        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        return id;
    }

    // -----------------------------------------------------------
    // Secuencia automatica: nado -> preparacion -> despegue -> vuelo.
    // -----------------------------------------------------------
    inline constexpr float DURACION_DESPLAZAMIENTO = 4.0f;
    inline constexpr float CORTE_DESPEGUE = 1.30f;
    inline constexpr float RADIO_TRAYECTORIA = 1.20f;
    inline constexpr float VELOCIDAD_ANGULAR_VUELO = 0.45f;

    enum EstadoPato {
        DESPLAZAMIENTO = 0,
        PREPARACION = 1,
        DESPEGUE = 2,
        VUELO_CONTINUO = 3,
        TRANSICION_EXTRA = 4
    };

    struct PosePato {
        EstadoPato estado = DESPLAZAMIENTO;
        const PatoAnimado* clip = nullptr;
        float tiempoClip = 0.0f;
        glm::vec3 posicion = glm::vec3(0.0f);
        bool repetirClip = false;
        const char* descripcion = "";
    };

    inline float Suavizar(float t) {
        t = std::max(0.0f, std::min(1.0f, t));
        return t * t * (3.0f - 2.0f * t);
    }

    inline PosePato ActualizarDesplazamiento(float t, const PatoAnimado& nado) {
        const float progreso = Suavizar(t / DURACION_DESPLAZAMIENTO);
        return {
            DESPLAZAMIENTO, &nado, t,
            glm::vec3(-1.30f + 2.20f * progreso, 0.05f, 0.0f),
            true, "01 - Avance (nado de Blender)"
        };
    }

    inline PosePato ActualizarPreparacion(float t, const PatoAnimado& preparado) {
        const float limite = std::min(CORTE_DESPEGUE, preparado.duracion());
        return {
            PREPARACION, &preparado, std::min(t, limite),
            glm::vec3(0.90f, 0.05f, 0.0f),
            false, "02 - Preparacion para despegar (Blender)"
        };
    }

    inline PosePato ActualizarDespegue(float t, const PatoAnimado& preparado) {
        const float tiempoAbsoluto = std::min(
            CORTE_DESPEGUE + std::max(t, 0.0f), preparado.duracion());
        return {
            DESPEGUE, &preparado, tiempoAbsoluto,
            glm::vec3(0.90f, 0.05f, 0.0f),
            false, "03 - Despegue original (Blender)"
        };
    }

    inline PosePato ActualizarVueloContinuo(float t, const PatoAnimado& vuelo) {
        const float angulo = VELOCIDAD_ANGULAR_VUELO * t;
        const float radio = RADIO_TRAYECTORIA;

        const glm::vec3 posicion(
            0.90f + radio * (1.0f - std::cos(angulo)),
            0.05f + 0.08f * std::sin(t * 2.0f),
            radio * std::sin(angulo));

        return {
            VUELO_CONTINUO, &vuelo, std::max(t, 0.0f),
            posicion, true,
            "04 - Vuelo continuo (aleteo de Blender + recorrido C++)"
        };
    }

    inline PosePato ActualizarCicloAutomatico(double segundos,
        const PatoAnimado& nado,
        const PatoAnimado& preparado,
        const PatoAnimado& vuelo) {

        const float tInicial = static_cast<float>(std::max(segundos, 0.0));

        if (tInicial < DURACION_DESPLAZAMIENTO)
            return ActualizarDesplazamiento(tInicial, nado);

        float t = tInicial - DURACION_DESPLAZAMIENTO;
        const float preparacion = std::min(CORTE_DESPEGUE, preparado.duracion());

        if (t < preparacion)
            return ActualizarPreparacion(t, preparado);

        t -= preparacion;
        const float despegue = std::max(0.0f, preparado.duracion() - preparacion);

        if (t < despegue)
            return ActualizarDespegue(t, preparado);

        t -= despegue;
        return ActualizarVueloContinuo(t, vuelo);
    }

} // namespace PatoSecuencia
