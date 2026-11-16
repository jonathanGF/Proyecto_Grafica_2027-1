
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
#ifdef _WIN32
#include <direct.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"

// -----------------------------------------------------------
// 1. CONFIGURACION GENERAL (similar a la practica del profesor)
// -----------------------------------------------------------
const GLuint WIDTH = 1000, HEIGHT = 700;
const unsigned int MAX_HUESOS = 64;
GLfloat deltaTime = 0.0f;
bool keys[1024] = {};
glm::vec3 cameraPosition(0.0f, 2.1f, 12.0f);
glm::vec3 correccionUsuario(0.0f);

// La secuencia automatica inicia sin tocar ninguna tecla.
bool modoAutomatico = true;
bool pausado = false;
double relojAnimacion = 0.0;
int etapaManual = 0;

// Prototipos como los utilizados en la practica.
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void DoMovement();

// -----------------------------------------------------------
// 2. APOYO PARA UBICAR RECURSOS
// -----------------------------------------------------------
bool ExisteArchivo(const std::string& nombre) {
    std::ifstream f(nombre.c_str(), std::ios::binary);
    return f.good();
}
std::string RutaRecurso(const std::string& nombre) {
    const char* prefijos[] = { "", "Proyecto_Uranden/", "../Proyecto_Uranden/", "../", "../../Proyecto_Uranden/" };
    for (const char* p : prefijos) {
        const std::string candidato = std::string(p) + nombre;
        if (ExisteArchivo(candidato)) {
            std::cout << "Recurso encontrado: " << candidato << '\n';
            return candidato;
        }
    }
    std::cerr << "Falta archivo: " << nombre << '\n';
    return nombre;
}

struct VerticePato {
    float posicion[3];
    float normal[3];
    float uv[2];
    int idHuesos[4];
    float pesos[4];
};
static_assert(sizeof(VerticePato) == 64, "Formato de vertice incompatible; se requieren 64 bytes");
static_assert(sizeof(glm::mat4) == 64, "Formato de matrices incompatible; se requieren 64 bytes");

// Compilador de shaders independiente: evita diferencias entre versiones de Shader.h.
static std::string leerTexto(const std::string& archivo) {
    std::ifstream f(archivo.c_str(), std::ios::binary);
    if (!f) { std::cerr << "No se pudo abrir shader: " << archivo << "\n"; return {}; }
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
static GLuint compilarGLSL(GLenum tipo, const std::string& codigo) {
    GLuint s = glCreateShader(tipo);
    const char* ptr = codigo.c_str();
    glShaderSource(s, 1, &ptr, nullptr);
    glCompileShader(s);
    GLint correcto = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &correcto);
    if (correcto != GL_TRUE) {
        GLint largo = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &largo);
        std::vector<char> log(static_cast<size_t>(std::max(largo, 1)), 0);
        glGetShaderInfoLog(s, largo, nullptr, log.data());
        std::cerr << "ERROR AL COMPILAR SHADER: " << log.data() << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint crearProgramaGL(const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vertex = leerTexto(vertexPath);
    const std::string fragment = leerTexto(fragmentPath);
    if (vertex.empty() || fragment.empty()) return 0;
    GLuint vs = compilarGLSL(GL_VERTEX_SHADER, vertex);
    if (!vs) return 0;
    GLuint fs = compilarGLSL(GL_FRAGMENT_SHADER, fragment);
    if (!fs) { glDeleteShader(vs); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint correcto = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &correcto);
    if (correcto != GL_TRUE) {
        GLint largo = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &largo);
        std::vector<char> log(static_cast<size_t>(std::max(largo, 1)), 0);
        glGetProgramInfoLog(p, largo, nullptr, log.data());
        std::cerr << "ERROR AL ENLAZAR SHADERS: " << log.data() << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

class PatoAnimado {
public:
    PatoAnimado() : vao(0), vbo(0), ebo(0), indiceTotal(0), huesos(0), frames(0), fps(60.0f), ok(false) {}
    ~PatoAnimado() {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
    }
    bool cargar(const std::string& archivo) {
        std::ifstream f(archivo.c_str(), std::ios::binary);
        if (!f) { std::cerr << "No se pudo abrir: " << archivo << "\n"; return false; }
        char magic[8];
        f.read(magic, 8);
        if (!f || std::memcmp(magic, "PATOAN1\0", 8) != 0) {
            std::cerr << "Formato incorrecto: " << archivo << "\n"; return false;
        }
        std::uint32_t nVert = 0, nInd = 0, nBones = 0, nFrames = 0;
        f.read(reinterpret_cast<char*>(&nVert), 4);
        f.read(reinterpret_cast<char*>(&nInd), 4);
        f.read(reinterpret_cast<char*>(&nBones), 4);
        f.read(reinterpret_cast<char*>(&nFrames), 4);
        f.read(reinterpret_cast<char*>(&fps), 4);
        if (!f || nVert < 3 || nVert>1000000 || nInd < 3 || nInd>5000000 ||
            nBones == 0 || nBones > MAX_HUESOS || nFrames < 2 || nFrames>10000 || fps <= 0) {
            std::cerr << "Encabezado invalido: " << archivo << "\n"; return false;
        }
        std::vector<VerticePato> verts(nVert);
        std::vector<std::uint32_t> inds(nInd);
        matrices.resize(static_cast<size_t>(nBones) * nFrames);
        f.read(reinterpret_cast<char*>(verts.data()), static_cast<std::streamsize>(nVert * sizeof(VerticePato)));
        f.read(reinterpret_cast<char*>(inds.data()), static_cast<std::streamsize>(nInd * sizeof(std::uint32_t)));
        f.read(reinterpret_cast<char*>(matrices.data()), static_cast<std::streamsize>(matrices.size() * sizeof(glm::mat4)));
        if (!f) { std::cerr << "Archivo incompleto: " << archivo << "\n"; return false; }
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, nVert * sizeof(VerticePato), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, nInd * sizeof(std::uint32_t), inds.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VerticePato), (void*)offsetof(VerticePato, posicion));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VerticePato), (void*)offsetof(VerticePato, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VerticePato), (void*)offsetof(VerticePato, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_INT, sizeof(VerticePato), (void*)offsetof(VerticePato, idHuesos));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VerticePato), (void*)offsetof(VerticePato, pesos));
        glBindVertexArray(0);
        indiceTotal = static_cast<GLsizei>(nInd);
        huesos = nBones;
        frames = nFrames;
        ok = true;
        std::cout << "CARGADO: " << archivo << " | Vertices=" << nVert
            << " | Triangulos=" << nInd / 3 << " | Huesos=" << huesos
            << " | Cuadros=" << frames << "\n";
        return true;
    }
    bool listo() const { return ok; }
    // Duracion de la animacion importada del archivo .patoanim.
    float duracion() const { return (frames > 1 && fps > 0.0f) ? float(frames - 1) / fps : 0.0f; }
    void dibujar(GLuint programa, float segundos, GLuint textura,
        const glm::mat4& modelo, const glm::mat4& vista,
        const glm::mat4& proyeccion, bool repetir) const {
        if (!ok) return;
        const float duracion = static_cast<float>(frames - 1) / fps;
        // Nado: repetir. Preparacion y vuelo: AVANZAR SIN REINICIAR
        // el clip, y conservar el ultimo frame al terminar.
        const float t = repetir
            ? std::fmod(std::max(segundos, 0.0f), duracion)
            : std::max(0.0f, std::min(segundos, duracion));
        const unsigned int frame = std::min(
            static_cast<unsigned int>(t * fps + 0.0001f), frames - 1);
        glUseProgram(programa);
        glUniformMatrix4fv(glGetUniformLocation(programa, "model"), 1, GL_FALSE, glm::value_ptr(modelo));
        glUniformMatrix4fv(glGetUniformLocation(programa, "view"), 1, GL_FALSE, glm::value_ptr(vista));
        glUniformMatrix4fv(glGetUniformLocation(programa, "projection"), 1, GL_FALSE, glm::value_ptr(proyeccion));
        glUniformMatrix4fv(glGetUniformLocation(programa, "bones[0]"), static_cast<GLsizei>(huesos), GL_FALSE,
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

static GLuint cargarTextura(const std::string& archivo) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = SOIL_load_image(archivo.c_str(), &w, &h, &channels, SOIL_LOAD_RGBA);
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    if (pixels && w > 0 && h > 0) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        std::cout << "TEXTURA CARGADA: " << archivo << " (" << w << "x" << h << ")\n";
        SOIL_free_image_data(pixels);
    }
    else {
        const unsigned char naranja[] = { 245,175,75,255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, naranja);
        std::cerr << "Aviso: usando textura provisional (no se pudo cargar PNG)\n";
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
// 4. LOGICA DE ANIMACION: ETAPAS DOCUMENTADAS POR SEPARADO
// -----------------------------------------------------------
// Secuencia automatica:
//   1) Avance sobre el suelo (clip de NADO de Blender).
//   2) Preparacion para despegar (inicio de PatoPreparadoVuelo).
//   3) Despegue (final de PatoPreparadoVuelo, SIN reiniciarlo).
//   4) Vuelo continuo (bucle de aleteo derivado del MISMO Blender).
// El vuelo NO termina ni regresa al suelo salvo que pulses R.
const float DURACION_DESPLAZAMIENTO = 4.0f;
const float CORTE_DESPEGUE = 1.30f;
const float RADIO_TRAYECTORIA = 1.20f;
const float VELOCIDAD_ANGULAR_VUELO = 0.45f; // radianes/segundo

// Estados visibles en consola para que el equipo entienda cada etapa.
enum EstadoPato {
    DESPLAZAMIENTO = 0, PREPARACION = 1,
    DESPEGUE = 2, VUELO_CONTINUO = 3,
    TRANSICION_EXTRA = 4
};

struct PosePato {
    EstadoPato estado;
    const PatoAnimado* clip;
    float tiempoClip;       // Tiempo del clip, en segundos.
    glm::vec3 posicion;     // Movimiento general agregado con C++.
    bool repetirClip;       // El vuelo continuo se reproduce en bucle.
    const char* descripcion;
};

float Suavizar(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

// ETAPA 1: AVANCE. El clip original de esta etapa es NADO, no caminata.
PosePato ActualizarDesplazamiento(float t, const PatoAnimado& nado) {
    const float progreso = Suavizar(t / DURACION_DESPLAZAMIENTO);
    return { DESPLAZAMIENTO, &nado, t,
             glm::vec3(-1.30f + 2.20f * progreso, 0.05f, 0.0f), true,
             "01 - Avance (animacion de nado de Blender)" };
}

// ETAPA 2: PREPARACION. Reproduce los primeros 1.30 s del clip.
PosePato ActualizarPreparacion(float t, const PatoAnimado& preparado) {
    const float limite = std::min(CORTE_DESPEGUE, preparado.duracion());
    return { PREPARACION, &preparado, std::min(t, limite),
             glm::vec3(0.90f, 0.05f, 0.0f), false,
             "02 - Preparando el despegue (Blender)" };
}

// ETAPA 3: DESPEGUE. Reanuda el MISMO clip, sin saltar al inicio.
PosePato ActualizarDespegue(float t, const PatoAnimado& preparado) {
    const float tiempoAbsoluto = std::min(
        CORTE_DESPEGUE + std::max(t, 0.0f), preparado.duracion());
    return { DESPEGUE, &preparado, tiempoAbsoluto,
             glm::vec3(0.90f, 0.05f, 0.0f), false,
             "03 - Despegue original (Blender)" };
}

// ETAPA 4: VUELO SIN FIN. Repite el movimiento final de las alas,
// moviendo al pato en una curva circular suave para que no desaparezca
// de la camara. Este movimiento de recorrido es C++, NO otro clip Blender.
PosePato ActualizarVueloContinuo(float t, const PatoAnimado& vuelo) {
    const float angulo = VELOCIDAD_ANGULAR_VUELO * t;
    const float radio = RADIO_TRAYECTORIA;
    const glm::vec3 posicion(
        0.90f + radio * (1.0f - std::cos(angulo)),
        0.05f + 0.08f * std::sin(t * 2.0f),
        radio * std::sin(angulo));
    return { VUELO_CONTINUO, &vuelo, std::max(t, 0.0f), posicion, true,
             "04 - Vuelo continuo: aleteo de Blender + recorrido C++" };
}

// Animacion adicional disponible para comparar con tecla 5.
PosePato ActualizarTransicionExtra(float t, const PatoAnimado& transicion) {
    return { TRANSICION_EXTRA, &transicion, t,
             glm::vec3(0.90f, 0.05f, 0.0f), true,
             "05 - Transicion extra (Blender, solo manual)" };
}

// Control automatico: despues del despegue, el vuelo CONTINUA
// indefinidamente. Ya no se usa fmod para reiniciar toda la secuencia.
PosePato ActualizarCicloAutomatico(double segundos,
    const PatoAnimado& nado,
    const PatoAnimado& preparado,
    const PatoAnimado& vuelo) {
    float t = static_cast<float>(std::max(segundos, 0.0));
    if (t < DURACION_DESPLAZAMIENTO)
        return ActualizarDesplazamiento(t, nado);
    t -= DURACION_DESPLAZAMIENTO;
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

// Teclas: 1 avance, 2 preparacion, 3 despegue, 4 vuelo continuo,
// 5 transicion extra; R retorna al modo automatico desde el inicio.
PosePato ActualizarEtapaManual(int etapa, double segundos,
    const PatoAnimado& nado,
    const PatoAnimado& transicion,
    const PatoAnimado& preparado,
    const PatoAnimado& vuelo) {
    if (etapa == DESPLAZAMIENTO)
        return ActualizarDesplazamiento(
            static_cast<float>(std::fmod(segundos, DURACION_DESPLAZAMIENTO)), nado);
    if (etapa == PREPARACION) {
        const float d = std::max(0.01f, std::min(CORTE_DESPEGUE, preparado.duracion()));
        return ActualizarPreparacion(static_cast<float>(std::fmod(segundos, d)), preparado);
    }
    if (etapa == DESPEGUE) {
        const float d = std::max(0.01f, preparado.duracion() - CORTE_DESPEGUE);
        return ActualizarDespegue(static_cast<float>(std::fmod(segundos, d)), preparado);
    }
    if (etapa == VUELO_CONTINUO)
        return ActualizarVueloContinuo(static_cast<float>(segundos), vuelo);
    const float d = std::max(0.01f, transicion.duracion());
    return ActualizarTransicionExtra(static_cast<float>(std::fmod(segundos, d)), transicion);
}

// -----------------------------------------------------------
// 5. PROGRAMA PRINCIPAL (main, similar al del profesor)
// -----------------------------------------------------------
int main() {
#ifdef _WIN32
    char working[2048] = {};
    if (_getcwd(working, sizeof(working)))
        std::cout << "Directorio de trabajo: " << working << '\n';
#endif
    // 5.1 Inicializar ventana GLFW y contexto OpenGL 3.3
    if (!glfwInit()) { std::cerr << "Error inicializando GLFW\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "Urandén - Pato: secuencia automatica", nullptr, nullptr);
    if (!window) { std::cerr << "Error creando ventana\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, KeyCallback);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "No se pudo inicializar GLEW\n";
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    glEnable(GL_DEPTH_TEST);

    int resultado = 0;
    // El bloque interno libera los recursos de OpenGL antes de destruir la ventana.
    {
        // 5.2 Cargar shaders (misma idea que Shader de la practica)
        const GLuint shaderPato = crearProgramaGL(
            RutaRecurso("shader/pato_directo.vs"),
            RutaRecurso("shader/pato_directo.frag"));
        if (!shaderPato) resultado = 1;

        // 5.3 Carga de modelos / animaciones (equivalente a Model(...))
        PatoAnimado nado, transicion, preparacion, vueloContinuo;
        const bool a = nado.cargar(RutaRecurso("Models/PatoNadando.patoanim"));
        const bool b = transicion.cargar(RutaRecurso("Models/transicionpreparadoavuelo.patoanim"));
        const bool c = preparacion.cargar(RutaRecurso("Models/PatoPreparadoVuelo.patoanim"));
        const bool d = vueloContinuo.cargar(RutaRecurso("Models/PatoVueloContinuo.patoanim"));
        if (!(a && b && c && d)) resultado = 1;
        const GLuint texturaPato = cargarTextura(RutaRecurso("Models/Pato_Color.png"));

        std::cout << "\nMODO AUTOMATICO: Avance -> Preparacion -> Despegue -> VUELO CONTINUO\n"
            << "Teclas 1/2/3/4: etapas; 5: transicion EXTRA (solo manual)\n"
            << "R: reiniciar y regresar a automatico | P: pausa / continuar\n"
            << "Flechas: corregir posicion | WASD: mover camara | ESC: salir\n";

        double instanteAnterior = glfwGetTime();
        int estadoAnterior = -1;

        // 5.4 Game Loop de la practica
        while (resultado == 0 && !glfwWindowShouldClose(window)) {
            const double instanteActual = glfwGetTime();
            deltaTime = static_cast<float>(std::min(instanteActual - instanteAnterior, 0.10));
            instanteAnterior = instanteActual;
            glfwPollEvents();
            DoMovement();
            if (!pausado) relojAnimacion += deltaTime;

            const PosePato pose = modoAutomatico
                ? ActualizarCicloAutomatico(relojAnimacion, nado, preparacion, vueloContinuo)
                : ActualizarEtapaManual(etapaManual, relojAnimacion, nado, transicion, preparacion, vueloContinuo);

            if (static_cast<int>(pose.estado) != estadoAnterior) {
                std::cout << "Etapa activa: " << pose.descripcion << '\n';
                estadoAnterior = static_cast<int>(pose.estado);
            }

            // 5.5 Limpiar framebuffer, crear matrices y dibujar
            int ancho = WIDTH, alto = HEIGHT;
            glfwGetFramebufferSize(window, &ancho, &alto);
            glViewport(0, 0, ancho, alto);
            glClearColor(0.10f, 0.17f, 0.25f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 model(1.0f);
            model = glm::translate(model, pose.posicion + correccionUsuario);
            if (pose.estado == VUELO_CONTINUO) {
                // Orientar el pato segun el movimiento de la trayectoria.
                model = glm::rotate(model,
                    VELOCIDAD_ANGULAR_VUELO * pose.tiempoClip,
                    glm::vec3(0.0f, 1.0f, 0.0f));
            }
            model = glm::scale(model, glm::vec3(0.90f));
            const glm::mat4 view = glm::lookAt(cameraPosition,
                cameraPosition + glm::vec3(0.0f, -0.06f, -1.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(ancho) / static_cast<float>(std::max(1, alto)),
                0.1f, 150.0f);

            pose.clip->dibujar(shaderPato, pose.tiempoClip,
                texturaPato, model, view, projection, pose.repetirClip);
            glfwSwapBuffers(window);
        }

        // 5.6 Liberar recursos sin cerrar aun el contexto OpenGL
        glDeleteTextures(1, &texturaPato);
        if (shaderPato) glDeleteProgram(shaderPato);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return resultado;
}

// -----------------------------------------------------------
// 6. FUNCIONES DE TECLADO (mismo patron de la practica)
// -----------------------------------------------------------
void DoMovement() {
    const float paso = 2.0f * deltaTime;
    if (keys[GLFW_KEY_W]) cameraPosition.z -= paso;
    if (keys[GLFW_KEY_S]) cameraPosition.z += paso;
    if (keys[GLFW_KEY_A]) cameraPosition.x -= paso;
    if (keys[GLFW_KEY_D]) cameraPosition.x += paso;
    cameraPosition.z = std::max(cameraPosition.z, 2.5f);
    if (keys[GLFW_KEY_LEFT])  correccionUsuario.x -= paso;
    if (keys[GLFW_KEY_RIGHT]) correccionUsuario.x += paso;
    if (keys[GLFW_KEY_UP])    correccionUsuario.z -= paso;
    if (keys[GLFW_KEY_DOWN])  correccionUsuario.z += paso;
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        if (action == GLFW_RELEASE) keys[key] = false;
    }
    if (action != GLFW_PRESS) return;
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
        modoAutomatico = false;
        etapaManual = key - GLFW_KEY_1;
        relojAnimacion = 0.0;
        pausado = false;
        std::cout << "Modo manual: etapa " << (etapaManual + 1)
            << " | Pulsa R para volver al automatico\n";
    }
    if (key == GLFW_KEY_R) {
        modoAutomatico = true;
        relojAnimacion = 0.0;
        correccionUsuario = glm::vec3(0.0f);
        pausado = false;
        std::cout << "Secuencia automatica reiniciada\n";
    }
    if (key == GLFW_KEY_P) {
        pausado = !pausado;
        std::cout << (pausado ? "Pausa\n" : "Continuar\n");
    }
}
