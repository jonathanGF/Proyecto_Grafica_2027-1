#ifndef _WIN64
#define GLEW_STATIC
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"

#include "Shader.h"
#include "Model.h"
#include "AudioEngine.h"
#include "Camera.h"

// =========================================================================
// 1. CONSTANTES Y CONFIGURACIÓN GLOBAL
// =========================================================================
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Configuración de la Cámara
Camera camera(glm::vec3(0.0f, 1.5f, 4.0f));

bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

// Control de Cuadros y Tiempo
float deltaTime = 0.0f;
float lastFrame = 0.0f;
double lastFpsTime = 0.0;
int nbFrames = 0;

// Constantes de Simulación
const float VELOCIDAD_CIRCUITO = 0.035f;

// =========================================================================
// 2. DATOS DE ESCENA Y TRAYECTORIAS
// =========================================================================

// Desplazamiento global del muelle hacia la orilla derecha
// Modifica el primer valor (X) para acercarlo o alejarlo de la orilla
glm::vec3 offsetMuelle = glm::vec3(-3.5f, 0.0f, 0.0f);

// Posiciones base relativas de las 4 veladoras en los postes del muelle
const glm::vec3 pointLightOffsets[4] = {
    glm::vec3(1.05f, -0.21f,  0.5f),  // Esquina frontal derecha
    glm::vec3(-1.05f, -0.21f,  0.5f),  // Esquina frontal izquierda
    glm::vec3(-1.05f, -0.21f, -0.5f),  // Lateral trasero izquierdo
    glm::vec3(1.05f, -0.21f, -0.5f)   // Lateral trasero derecho
};

// Puntos de control adaptados al cauce del agua (Catmull-Rom)
// Trayectoria cerrada: Izquierda = X negativa | Derecha = X positiva
const std::vector<glm::vec3> puntosCircuito = {
    // 1. Bajando por el carril izquierdo (X negativa, frente al muelle)
    glm::vec3(-1.2f, -0.65f,  0.2f),   // Punto de salida (frente al muelle)
    glm::vec3(-1.2f, -0.65f,  4.0f),   // Bajando por la izquierda (tramo alto)
    glm::vec3(-1.3f, -0.65f,  8.5f),   // Bajando por la izquierda (tramo medio)
    glm::vec3(-1.2f, -0.65f, 12.5f),   // Llegando al final del canal abajo

    // 2. Curva en U en el fondo del lago (cruza de X negativa a X positiva)
    glm::vec3(-0.5f, -0.65f, 14.5f),   // Entrada a la curva inferior
    glm::vec3(0.6f, -0.65f, 14.5f),   // Salida de la curva hacia la derecha

    // 3. Subiendo por el carril derecho (X positiva moderada para no pisar tierra)
    glm::vec3(1.3f, -0.65f, 12.0f),   // Inicio del ascenso por la derecha
    glm::vec3(1.5f, -0.65f,  8.0f),   // Canal derecho medio
    glm::vec3(1.5f, -0.65f,  4.0f),   // Subiendo hacia la isla
    glm::vec3(1.3f, -0.65f,  0.2f),   // Lado derecho a la altura del muelle

    // 4. Retorno superior (cruza de derecha a izquierda frente a la isla)
    glm::vec3(0.5f, -0.65f, -0.6f),   // Curva superior hacia el centro
    glm::vec3(-0.6f, -0.65f, -0.5f)    // Reincorporación frente al muelle
};

// Geometría del Skybox
const float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
};

// Estructura para las zonas de interacción
struct Estacion {
    glm::vec3 posicion;
    float radioDeteccion;
    bool interactuable;
    bool activa;
};

// Definición de las 4 estaciones
// Nota: Las coordenadas son temporales. Debes ajustarlas a la posición real de cada zona en tu mapa.
std::vector<Estacion> estaciones = {
    {glm::vec3(-3.5f, 0.0f, 0.0f), 3.0f, false, false},  // 1. Embarcadero
    {glm::vec3(-5.0f, 0.0f, 4.0f), 3.0f, false, false},  // 2. Parador
    {glm::vec3(0.0f, 0.0f, 6.0f),  3.0f, false, false},  // 3. Puente
    {glm::vec3(2.0f, 0.0f, 10.0f), 3.0f, false, false}   // 4. Islote
};

// Variable para controlar qué estación se está mostrando en el HUD (-1 significa ninguna)
int estacionActivaHUD = -1;
bool eKeyPressed = false;

// =========================================================================
// 3. FUNCIONES MATEMÁTICAS Y DE GENERACIÓN PROCEDURAL
// =========================================================================

float obtenerAlturaTerreno(Model& modelo, float x, float z) {
    // Reproducimos la misma transformación que tiene el terreno en el bucle principal
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::translate(modelMat, glm::vec3(0.0f, -0.2f, 0.0f));
    modelMat = glm::scale(modelMat, glm::vec3(0.2f));

    glm::vec2 pos2D(x, z);

    for (unsigned int i = 0; i < modelo.meshes.size(); i++) {
        for (size_t j = 0; j < modelo.meshes[i].indices.size(); j += 3) {

            // Obtener vértices originales (asume que 'vertices' e 'indices' son públicos en tu clase Mesh)
            glm::vec3 v0 = modelo.meshes[i].vertices[modelo.meshes[i].indices[j]].Position;
            glm::vec3 v1 = modelo.meshes[i].vertices[modelo.meshes[i].indices[j + 1]].Position;
            glm::vec3 v2 = modelo.meshes[i].vertices[modelo.meshes[i].indices[j + 2]].Position;

            // Pasamos los vértices a coordenadas de mundo
            v0 = glm::vec3(modelMat * glm::vec4(v0, 1.0f));
            v1 = glm::vec3(modelMat * glm::vec4(v1, 1.0f));
            v2 = glm::vec3(modelMat * glm::vec4(v2, 1.0f));

            // Coordenadas baricéntricas para evaluar si (X, Z) está dentro del triángulo
            float det = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
            if (det == 0.0f) continue;

            float l1 = ((v1.z - v2.z) * (pos2D.x - v2.x) + (v2.x - v1.x) * (pos2D.y - v2.z)) / det;
            float l2 = ((v2.z - v0.z) * (pos2D.x - v2.x) + (v0.x - v2.x) * (pos2D.y - v2.z)) / det;
            float l3 = 1.0f - l1 - l2;

            // Si los 3 pesos son positivos, el punto está dentro del triángulo
            if (l1 >= 0.0f && l2 >= 0.0f && l3 >= 0.0f) {
                return l1 * v0.y + l2 * v1.y + l3 * v2.y;
            }
        }
    }
    // Retorno de seguridad si sales del mapa
    return 0.0f;
}

float barryCentric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 pos) {
    float det = (p2.z - p3.z) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.z - p3.z);

    // Evitar división por cero si el triángulo está degenerado
    if (det == 0.0f) return 0.0f;

    float l1 = ((p2.z - p3.z) * (pos.x - p3.x) + (p3.x - p2.x) * (pos.y - p3.z)) / det;
    float l2 = ((p3.z - p1.z) * (pos.x - p3.x) + (p1.x - p3.x) * (pos.y - p3.z)) / det;
    float l3 = 1.0f - l1 - l2;

    // Retorna la altura interpolada en el eje Y
    return l1 * p1.y + l2 * p2.y + l3 * p3.y;
}

glm::vec3 calcularCatmullRom(const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2, const glm::vec3& p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

void generarPlanoAgua(int resolucion, float tamano, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    float paso = tamano / (float)resolucion;
    float mitad = tamano / 2.0f;

    for (int i = 0; i <= resolucion; ++i) {
        float z = -mitad + i * paso;
        for (int j = 0; j <= resolucion; ++j) {
            float x = -mitad + j * paso;

            // Posición (X, Y, Z)
            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(z);

            // Normal hacia arriba (X, Y, Z)
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);

            // Coordenadas UV (S, T)
            vertices.push_back((float)j / (float)resolucion);
            vertices.push_back((float)i / (float)resolucion);
        }
    }

    for (int i = 0; i < resolucion; ++i) {
        for (int j = 0; j < resolucion; ++j) {
            int fila1 = i * (resolucion + 1);
            int fila2 = (i + 1) * (resolucion + 1);

            indices.push_back(fila1 + j);
            indices.push_back(fila2 + j);
            indices.push_back(fila1 + j + 1);

            indices.push_back(fila1 + j + 1);
            indices.push_back(fila2 + j);
            indices.push_back(fila2 + j + 1);
        }
    }
}

// =========================================================================
// 4. CALLBACKS Y CONTROL DE ENTRADA
// =========================================================================

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        if (!eKeyPressed) {
            eKeyPressed = true;
            for (int i = 0; i < estaciones.size(); i++) {
                if (estaciones[i].interactuable) {
                    // Alternar el estado del HUD
                    if (estacionActivaHUD == i) {
                        estacionActivaHUD = -1; // Cerrar la cédula
                        std::cout << "HUD cerrado." << std::endl;
                    }
                    else {
                        estacionActivaHUD = i; // Mostrar la cédula
                        std::cout << "Mostrando HUD para Estacion " << i + 1 << std::endl;

                        // Espacio reservado para detonar animaciones (A1, A2, A3, A5)
                        // if (i == 0) IniciarDesfileCanoasA2();
                    }
                    break;
                }
            }
        }
    }
    else {
        eKeyPressed = false;
    }
}

void showFPS(GLFWwindow* window) {
    double currentTime = glfwGetTime();
    nbFrames++;
    if (currentTime - lastFpsTime >= 1.0) {
        double fps = double(nbFrames) / (currentTime - lastFpsTime);
        std::string title = "El Camino de las Animas | FPS: " + std::to_string((int)fps);
        glfwSetWindowTitle(window, title.c_str());
        nbFrames = 0;
        lastFpsTime = currentTime;
    }
}

// =========================================================================
// 5. FUNCIÓN PRINCIPAL
// =========================================================================

int main() {
    // --- Inicialización GLFW & OpenGL Context ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "El Camino de las Animas", NULL, NULL);
    if (window == NULL) {
        std::cerr << "ERROR::GLFW::Fallo al crear la ventana" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync activo

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "ERROR::GLEW::Fallo al inicializar GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // --- Shaders ---
    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Shader skyboxShader("shader/skybox.vert", "shader/skybox.frag");
    Shader waterShader("shader/water.vert", "shader/water.frag");

    // --- Buffers: Skybox ---
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    GLuint cubemapTexture = SOIL_load_OGL_cubemap(
        "skybox/xpos.png", "skybox/xneg.png",
        "skybox/ypos.png", "skybox/yneg.png",
        "skybox/zpos.png", "skybox/zneg.png",
        SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS
    );

    skyboxShader.use();
    glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

    // --- Buffers: Superficie del Agua ---
    std::vector<float> waterVertices;
    std::vector<unsigned int> waterIndices;
    int waterResolution = 100;
    float waterSize = 100.0f;
    generarPlanoAgua(waterResolution, waterSize, waterVertices, waterIndices);

    unsigned int waterVAO, waterVBO, waterEBO;
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glGenBuffers(1, &waterEBO);

    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(float), waterVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, waterIndices.size() * sizeof(unsigned int), waterIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // --- Carga de Modelos 3D ---
    // La canoa permanece separada porque tiene movimiento propio.
    Model modeloCanoa("Models/canoa.obj");

    // El mapa glTF sustituye al terreno, muelle, veladoras y demas elementos
    // estaticos que antes se cargaban como modelos independientes.
    std::cout << "Cargando mapa completo de Uranden..." << std::endl;
    Model modeloMapaCompleto("Models/MapaCompleto/Uranden_BAKING.gltf");
    std::cout << "Mapa completo cargado correctamente. Mallas: "
              << modeloMapaCompleto.meshes.size() << std::endl;

    // --- Motor de Audio ---
    AudioEngine audioEngine;
    if (!audioEngine.init()) {
        std::cerr << "ERROR::MINIAUDIO::No se pudo inicializar el motor de audio" << std::endl;
    }
    audioEngine.playMusic("audio/Ofrenda_en_el_Agua.mp3", 0.35f, true);
    audioEngine.playAmbient("audio/ambiente.mp3", 0.25f, true);

    // Compilar el shader del HUD
    Shader hudShader("shader/hud.vs", "shader/hud.fs");

    // Configurar VAO/VBO para un Quad 2D unitario (x, y, u, v)
    unsigned int hudVAO, hudVBO;
    float hudVertices[] = {
        // Posición (X, Y)  // Textura (U, V)
         0.0f,  1.0f,       0.0f, 1.0f,
         1.0f,  0.0f,       1.0f, 0.0f,
         0.0f,  0.0f,       0.0f, 0.0f,
         0.0f,  1.0f,       0.0f, 1.0f,
         1.0f,  1.0f,       1.0f, 1.0f,
         1.0f,  0.0f,       1.0f, 0.0f
    };
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);
    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(hudVertices), hudVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Cargar las texturas forzando RGBA para leer la transparencia
    unsigned int texCedula1 = SOIL_load_OGL_texture("Models/textures/1_Las-copaleras-y-el-rito-de-purificacion.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula2 = SOIL_load_OGL_texture("Models/textures/2_Danzas-y-cocina-tradicional.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula3 = SOIL_load_OGL_texture("Models/textures/3_Procesion-de-Canoas.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula4 = SOIL_load_OGL_texture("Models/textures/4_El-rescate-de-los-manantiales.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);

    // Parámetros básicos para que la textura no se vea borrosa ni se repita
    glBindTexture(GL_TEXTURE_2D, texCedula1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Repite esto para texCedula2, texCedula3 y texCedula4...
    

    // =====================================================================
    // 6. BUCLE PRINCIPAL DE RENDERIZADO
    // =====================================================================
    while (!glfwWindowShouldClose(window)) {
        // --- A. Tiempo y Procesamiento de Entradas ---
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Detección de proximidad a las estaciones
        for (int i = 0; i < estaciones.size(); i++) {
            float distancia = glm::distance(glm::vec2(camera.Position.x, camera.Position.z),
                glm::vec2(estaciones[i].posicion.x, estaciones[i].posicion.z));

            if (distancia <= estaciones[i].radioDeteccion) {
                if (!estaciones[i].interactuable) {
                    estaciones[i].interactuable = true;
                    std::cout << "Zona alcanzada: Estacion " << i + 1 << ". Presiona E." << std::endl;
                }
            }
            else {
                estaciones[i].interactuable = false;
            }
        }

        // --- B. Actualización de Cinemática y Animación (Update) ---
        float tiempoAnim = currentFrame;

        // Dinámica de remadas
        float cicloRemo = std::fmod(tiempoAnim, 2.2f) / 2.2f;
        float impulsoRemo = std::pow(std::sin(cicloRemo * 3.14159f), 0.7f) * 0.4f;
        float avanceGlobal = (tiempoAnim * VELOCIDAD_CIRCUITO) + (impulsoRemo * 0.015f);

        // Muestreo de Catmull-Rom para la canoa
        int numPuntos = puntosCircuito.size();
        float progreso = std::fmod(avanceGlobal, 1.0f) * numPuntos;
        int i1 = (int)progreso;
        float t = progreso - i1;

        int i0 = (i1 - 1 + numPuntos) % numPuntos;
        int i2 = (i1 + 1) % numPuntos;
        int i3 = (i1 + 2) % numPuntos;

        glm::vec3 posActual = calcularCatmullRom(puntosCircuito[i0], puntosCircuito[i1],
            puntosCircuito[i2], puntosCircuito[i3], t);

        // Vector tangente y orientación hacia el avance
        float deltaT = 0.01f;
        glm::vec3 posFutura = calcularCatmullRom(puntosCircuito[i0], puntosCircuito[i1],
            puntosCircuito[i2], puntosCircuito[i3], t + deltaT);
        glm::vec3 direccionAvance = glm::normalize(posFutura - posActual);
        float anguloRumbo = glm::degrees(std::atan2(direccionAvance.x, direccionAvance.z));

        // Oscilaciones por física de agua (más suaves)
        float bamboleoRoll = std::sin(cicloRemo * 6.28318f) * 1.5f;
        float cabeceoPitch = std::cos(cicloRemo * 6.28318f) * 0.8f;
        float flotacionOlas = std::sin(tiempoAnim * 1.0f) * 0.01f;
        posActual.y += flotacionOlas;

        // Posición y parpadeo de la luz puntual dinámica (Canoa)
        glm::vec3 posLuzCanoa = posActual + glm::vec3(0.0f, 0.18f, 0.0f);
        float parpadeoCanoa = 0.85f + 0.15f * std::sin(tiempoAnim * 14.0f + 2.5f);
        glm::vec3 colorLuzCanoa = glm::vec3(1.0f, 0.55f, 0.15f) * parpadeoCanoa;

        // --- C. Preparación del Framebuffer y Parámetros Atmosféricos ---
        glm::vec3 fogColor = glm::vec3(0.02f, 0.02f, 0.05f); // Tono de la bruma nocturna
        float fogDensity = 0.0f; // Densidad de niebla (0.0f para inspección clara)

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // --- D. Shader Principal: Paso de Iluminación, Niebla y Cámara ---
        miShader.use();
        glUniform3fv(glGetUniformLocation(miShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 1. Luz Direccional
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.ambient"), 0.7f, 0.7f, 0.7f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.diffuse"), 0.8f, 0.8f, 0.8f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.specular"), 0.20f, 0.20f, 0.25f);

        // 2. Luces Puntuales 0 a 3 (Veladoras fijas del Muelle, sincronizadas con offsetMuelle)
        for (unsigned int i = 0; i < 4; i++) {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            glm::vec3 posLuzReal = pointLightOffsets[i] + offsetMuelle;
            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzReal));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.01f, 0.00f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.60f, 0.30f, 0.08f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.30f, 0.10f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.14f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.07f);
        }

        // 3. Luz Puntual 4 (Luz Dinámica dentro de la Canoa)
        glUniform3fv(glGetUniformLocation(miShader.ID, "pointLights[4].position"), 1, glm::value_ptr(posLuzCanoa));
        glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[4].ambient"), colorLuzCanoa.r * 0.1f, colorLuzCanoa.g * 0.1f, colorLuzCanoa.b * 0.1f);
        glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[4].diffuse"), colorLuzCanoa.r, colorLuzCanoa.g, colorLuzCanoa.b);
        glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[4].specular"), colorLuzCanoa.r, colorLuzCanoa.g, colorLuzCanoa.b);
        glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[4].constant"), 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[4].linear"), 0.14f);
        glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[4].quadratic"), 0.07f);

        // 4. Reflector Cenital (Spotlight)
        glm::vec3 spotLightPos = glm::vec3(0.0f, 3.5f, 1.0f);
        glm::vec3 spotLightDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        glUniform3fv(glGetUniformLocation(miShader.ID, "spotLight.position"), 1, glm::value_ptr(spotLightPos));
        glUniform3fv(glGetUniformLocation(miShader.ID, "spotLight.direction"), 1, glm::value_ptr(spotLightDir));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.cutOff"), glm::cos(glm::radians(18.0f)));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.outerCutOff"), glm::cos(glm::radians(24.0f)));
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.ambient"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.diffuse"), 1.8f, 1.8f, 1.9f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.specular"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.constant"), 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.linear"), 0.07f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.quadratic"), 0.017f);

        // 5. Niebla en Modelos Opacos
        glUniform3fv(glGetUniformLocation(miShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), fogDensity);

        // --- E. Renderizado de Mallas 3D Opacas ---
        // Necesario para las hojas, ramas y flores con transparencia del glTF.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), true);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);

        // 1. Mapa completo exportado desde Blender.
        // Se conserva la misma escala y altura que utilizaba el terreno OBJ.
        glm::mat4 mapaModel = glm::mat4(1.0f);
        mapaModel = glm::translate(mapaModel, glm::vec3(0.0f, -0.2f, 0.0f));
        mapaModel = glm::scale(mapaModel, glm::vec3(0.2f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(mapaModel));
        modeloMapaCompleto.Draw(miShader);

        // 2. Canoa Animada (Recorre la trayectoria adaptada al canal)
        glm::mat4 canoaModel = glm::mat4(1.0f);
        canoaModel = glm::translate(canoaModel, posActual);
        canoaModel = glm::rotate(canoaModel, glm::radians(anguloRumbo), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(bamboleoRoll), glm::vec3(0.0f, 0.0f, 1.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(cabeceoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        canoaModel = glm::scale(canoaModel, glm::vec3(2.0f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaModel));
        modeloCanoa.Draw(miShader);

        // --- F. Renderizado de la Superficie del Agua ---
        waterShader.use();
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3f(glGetUniformLocation(waterShader.ID, "dirLightDir"), -0.2f, -1.0f, -0.3f);

        // Multiplicamos currentFrame por 0.25f para que las olas se muevan a 1/4 de su velocidad
        glUniform1f(glGetUniformLocation(waterShader.ID, "time"), currentFrame * 0.25f);

        // Niebla sobre el agua
        glUniform3fv(glGetUniformLocation(waterShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(waterShader.ID, "fogDensity"), fogDensity);

        glm::mat4 waterModel = glm::mat4(1.0f);
        waterModel = glm::translate(waterModel, glm::vec3(0.0f, -0.75f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(waterModel));

        glDisable(GL_CULL_FACE);
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // --- G. Renderizado del Skybox ---
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);


        // Renderizado del HUD (Interfaz 2D)
        if (estacionActivaHUD != -1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST); // El HUD siempre dibuja al frente

            hudShader.use();
            glUniform1i(glGetUniformLocation(hudShader.ID, "hudTexture"), 0);
            // Proyección ortográfica (Ajusta los valores al ancho y alto real de tu ventana, ej. 800x600)
            glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
            hudShader.setMat4("projection", projection);

            // Posición y tamaño de la cédula en pantalla
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(80.0f, 120.0f, 0.0f)); // Posición centrada (X, Y)
            model = glm::scale(model, glm::vec3(640.0f, 360.0f, 1.0f));    // Tamaño ampliado (Ancho, Alto)
            hudShader.setMat4("model", model);

            // Vincular la textura correcta dependiendo de estacionActivaHUD (0 a 3)
            glActiveTexture(GL_TEXTURE0);
            // Justo antes de glBindVertexArray(hudVAO);
            glActiveTexture(GL_TEXTURE0);
            if (estacionActivaHUD == 0) {
                glBindTexture(GL_TEXTURE_2D, texCedula1);
                if (texCedula1 == 0) {
                    std::cout << "ERROR: SOIL no encuentra la imagen 1_Las-copaleras-y-el-rito-de-purificacion.png" << std::endl;
                }
            }
            else if (estacionActivaHUD == 1) {
                glBindTexture(GL_TEXTURE_2D, texCedula2);
                if (texCedula2 == 0) {
                    std::cout << "ERROR: SOIL no encuentra la imagen 2_Danzas-y-cocina-tradicional.png" << std::endl;
                }
            }
            else if (estacionActivaHUD == 2) {
                glBindTexture(GL_TEXTURE_2D, texCedula3);
                if (texCedula3 == 0) {
                    std::cout << "ERROR: SOIL no encuentra la imagen 3_Procesion-de-Canoas.png" << std::endl;
                }
            }
            else if (estacionActivaHUD == 3) {
                glBindTexture(GL_TEXTURE_2D, texCedula4);
                if (texCedula4 == 0) {
                    std::cout << "ERROR: SOIL no encuentra la imagen 4_El-rescate-de-los-manantiales.png" << std::endl;
                }   
            }

            glBindVertexArray(hudVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST); // Reactivar para el siguiente fotograma 3D
            glDisable(GL_BLEND);
        }
        // --- H. Intercambio de Buffers y Eventos ---
        glfwSwapBuffers(window);
        glfwPollEvents();
        showFPS(window);
    }

    // =====================================================================
    // 7. LIMPIEZA DE RECURSOS
    // =====================================================================
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteBuffers(1, &waterEBO);

    audioEngine.cleanup();
    glfwTerminate();
    return 0;
}
