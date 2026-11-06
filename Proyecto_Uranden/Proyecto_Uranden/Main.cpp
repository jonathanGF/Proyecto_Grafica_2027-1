#define GLEW_STATIC

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

Camera camera(glm::vec3(0.0f, 1.5f, 4.0f));

bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
double lastFpsTime = 0.0;
int nbFrames = 0;

const float VELOCIDAD_CIRCUITO = 0.035f;

// =========================================================================
// 2. DATOS DE ESCENA Y TRAYECTORIAS
// =========================================================================

glm::vec3 offsetMuelle = glm::vec3(-3.5f, 0.0f, 0.0f);

// 4 veladoras del muelle (desactivadas en iluminación)
const glm::vec3 pointLightOffsets[4] = {
    glm::vec3(1.05f, -0.21f,  0.5f),
    glm::vec3(-1.05f, -0.21f,  0.5f),
    glm::vec3(-1.05f, -0.21f, -0.5f),
    glm::vec3(1.05f, -0.21f, -0.5f)
};

// 5 postes fijos ya calibrados de la plaza
const std::vector<glm::vec3> postesPlaza = {
    glm::vec3(4.62443f, -0.108958f, 15.885f),
    glm::vec3(3.70724f, -0.111046f, 15.8275f),
    glm::vec3(2.78916f, -0.114986f, 15.7848f),
    glm::vec3(2.09366f, -0.116454f, 16.0913f),
    glm::vec3(2.08908f, -0.113338f, 16.7173f)
};

// 6 reflectores cálidos en la orilla original (posiciones calibradas fijas)
std::vector<glm::vec3> lucesOrilla = {
    glm::vec3(6.4632f,  -0.660309f, -0.730785f),
    glm::vec3(4.06971f, -0.651941f,  4.17367f),
    glm::vec3(3.24339f, -0.62345f,   7.06666f),
    glm::vec3(3.04058f, -0.623944f,  9.44348f),
    glm::vec3(2.94714f, -0.597321f, 12.0f),
    glm::vec3(5.20106f, -0.262329f, 15.4927f)
};

// 2 luces dedicadas para el letrero y el árbol (Fijas, sin geometría visible)
std::vector<glm::vec3> lucesLetrero = {
    glm::vec3(1.43314f, -0.634289f, -11.0765f),
    glm::vec3(0.0f,     3.68036f,  -14.6803f)
};

// 4 postes exactos del puente de madera (Calibrados)
std::vector<glm::vec3> postesPuente = {
    glm::vec3(-0.254487f, -0.363107f, 16.8868f),
    glm::vec3(0.719089f, -0.35674f,  16.8865f),
    glm::vec3(-0.253564f, -0.370363f, 16.6411f),
    glm::vec3(0.726335f, -0.359537f, 16.64f)
};

// 4 reflectores para el lado opuesto del manantial (Calibrados)
std::vector<glm::vec3> lucesManantialOpuesto = {
    glm::vec3(-3.08164f, -0.686701f,  3.60584f),
    glm::vec3(-3.11661f, -0.656584f,  7.2f),
    glm::vec3(-2.87594f, -0.676895f, 10.5f),
    glm::vec3(-2.59666f, -0.646597f, 13.5f)
};

int orillaSeleccionada = -1;
int letreroSeleccionado = -1;
int postePuenteSeleccionado = -1;
int reflectorOpuestoSeleccionado = -1; // 0=I, 1=O, 2=K, 3=L

// Trayectoria de la canoa
std::vector<glm::vec3> puntosCircuito = {
    glm::vec3(-1.2f, -0.65f,  0.2f),
    glm::vec3(-1.2f, -0.65f,  4.0f),
    glm::vec3(-1.3f, -0.65f,  8.5f),
    glm::vec3(-1.2f, -0.65f, 12.5f),
    glm::vec3(-0.5f, -0.65f, 14.5f),
    glm::vec3(0.6f, -0.65f, 14.5f),
    glm::vec3(1.3f, -0.65f, 12.0f),
    glm::vec3(1.5f, -0.65f,  8.0f),
    glm::vec3(1.5f, -0.65f,  4.0f),
    glm::vec3(1.3f, -0.65f,  0.2f),
    glm::vec3(0.5f, -0.65f, -0.6f),
    glm::vec3(-0.6f, -0.65f, -0.5f)
};

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

struct Estacion {
    glm::vec3 posicion;
    float radioDeteccion;
    bool interactuable;
    bool activa;
};

std::vector<Estacion> estaciones = {
    {glm::vec3(-3.5f, 0.0f, 0.0f), 3.0f, false, false},
    {glm::vec3(-5.0f, 0.0f, 4.0f), 3.0f, false, false},
    {glm::vec3(0.0f, 0.0f, 6.0f),  3.0f, false, false},
    {glm::vec3(2.0f, 0.0f, 10.0f), 3.0f, false, false}
};

int estacionActivaHUD = -1;
bool eKeyPressed = false;

// =========================================================================
// 3. FUNCIONES AUXILIARES
// =========================================================================

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
            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(z);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
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
// 4. ENTRADA Y CONTROL DE CALIBRACIÓN
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
            for (int i = 0; i < (int)estaciones.size(); i++) {
                if (estaciones[i].interactuable) {
                    if (estacionActivaHUD == i) {
                        estacionActivaHUD = -1;
                    }
                    else {
                        estacionActivaHUD = i;
                    }
                    break;
                }
            }
        }
    }
    else {
        eKeyPressed = false;
    }

    // --- SELECCIÓN DE LOS 4 POSTES DEL PUENTE (Teclas 7, 8, 9, 0) ---
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) { postePuenteSeleccionado = 0; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) { postePuenteSeleccionado = 1; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) { postePuenteSeleccionado = 2; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) { postePuenteSeleccionado = 3; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }

    // --- SELECCIÓN DE REFLECTORES OPUESTOS DEL MANANTIAL (Teclas I, O, K, L) ---
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 0; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 1; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 2; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 3; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }

    // Movimiento del poste seleccionado
    if (postePuenteSeleccionado >= 0 && postePuenteSeleccionado < (int)postesPuente.size()) {
        float velAjuste = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    postesPuente[postePuenteSeleccionado].z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  postesPuente[postePuenteSeleccionado].z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  postesPuente[postePuenteSeleccionado].x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) postesPuente[postePuenteSeleccionado].x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     postesPuente[postePuenteSeleccionado].y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     postesPuente[postePuenteSeleccionado].y -= velAjuste;
    }

    // Movimiento de los reflectores opuestos del manantial
    if (reflectorOpuestoSeleccionado >= 0 && reflectorOpuestoSeleccionado < (int)lucesManantialOpuesto.size()) {
        float velAjuste = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    lucesManantialOpuesto[reflectorOpuestoSeleccionado].z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  lucesManantialOpuesto[reflectorOpuestoSeleccionado].z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  lucesManantialOpuesto[reflectorOpuestoSeleccionado].x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) lucesManantialOpuesto[reflectorOpuestoSeleccionado].x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     lucesManantialOpuesto[reflectorOpuestoSeleccionado].y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     lucesManantialOpuesto[reflectorOpuestoSeleccionado].y -= velAjuste;
    }

    // Guardar coordenadas con P
    static bool pKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) {
            pKeyPressed = true;
            std::cout << "\n=== COORDENADAS EXACTAS DE LOS 4 POSTES DEL PUENTE ===" << std::endl;
            for (size_t i = 0; i < postesPuente.size(); i++) {
                std::cout << "    glm::vec3("
                    << postesPuente[i].x << "f, "
                    << postesPuente[i].y << "f, "
                    << postesPuente[i].z << "f)," << std::endl;
            }
            std::cout << "\n=== COORDENADAS EXACTAS REFLECTORES OPUESTOS MANANTIAL ===" << std::endl;
            for (size_t i = 0; i < lucesManantialOpuesto.size(); i++) {
                std::cout << "    glm::vec3("
                    << lucesManantialOpuesto[i].x << "f, "
                    << lucesManantialOpuesto[i].y << "f, "
                    << lucesManantialOpuesto[i].z << "f)," << std::endl;
            }
            std::cout << "=========================================================\n" << std::endl;
        }
    }
    else {
        pKeyPressed = false;
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
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "ERROR::GLEW::Fallo al inicializar GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Shader skyboxShader("shader/skybox.vert", "shader/skybox.frag");
    Shader waterShader("shader/water.vert", "shader/water.frag");

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

    Model modeloCanoa("Models/canoa.obj");
    std::cout << "Cargando mapa completo de Uranden..." << std::endl;
    Model modeloMapaCompleto("Models/MapaCompleto/Uranden_BAKING.gltf");

    AudioEngine audioEngine;
    if (!audioEngine.init()) {
        std::cerr << "ERROR::MINIAUDIO::No se pudo inicializar el motor de audio" << std::endl;
    }
    audioEngine.playMusic("audio/Ofrenda_en_el_Agua.mp3", 0.35f, true);
    audioEngine.playAmbient("audio/ambiente.mp3", 0.25f, true);

    Shader hudShader("shader/hud.vs", "shader/hud.fs");
    unsigned int hudVAO, hudVBO;
    float hudVertices[] = {
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

    unsigned int texCedula1 = SOIL_load_OGL_texture("Models/textures/1_Las-copaleras-y-el-rito-de-purificacion.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula2 = SOIL_load_OGL_texture("Models/textures/2_Danzas-y-cocina-tradicional.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula3 = SOIL_load_OGL_texture("Models/textures/3_Procesion-de-Canoas.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula4 = SOIL_load_OGL_texture("Models/textures/4_El-rescate-de-los-manantiales.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        for (int i = 0; i < (int)estaciones.size(); i++) {
            float distancia = glm::distance(glm::vec2(camera.Position.x, camera.Position.z),
                glm::vec2(estaciones[i].posicion.x, estaciones[i].posicion.z));
            estaciones[i].interactuable = (distancia <= estaciones[i].radioDeteccion);
        }

        float tiempoAnim = currentFrame;
        float cicloRemo = std::fmod(tiempoAnim, 2.2f) / 2.2f;
        float impulsoRemo = std::pow(std::sin(cicloRemo * 3.14159f), 0.7f) * 0.4f;
        float avanceGlobal = (tiempoAnim * VELOCIDAD_CIRCUITO) + (impulsoRemo * 0.015f);

        int numPuntos = (int)puntosCircuito.size();
        float progreso = std::fmod(avanceGlobal, 1.0f) * numPuntos;
        int i1 = (int)progreso;
        float t = progreso - i1;

        int i0 = (i1 - 1 + numPuntos) % numPuntos;
        int i2 = (i1 + 1) % numPuntos;
        int i3 = (i1 + 2) % numPuntos;

        glm::vec3 posActual = calcularCatmullRom(puntosCircuito[i0], puntosCircuito[i1],
            puntosCircuito[i2], puntosCircuito[i3], t);

        float deltaT = 0.01f;
        glm::vec3 posFutura = calcularCatmullRom(puntosCircuito[i0], puntosCircuito[i1],
            puntosCircuito[i2], puntosCircuito[i3], t + deltaT);
        glm::vec3 direccionAvance = glm::normalize(posFutura - posActual);
        float anguloRumbo = glm::degrees(std::atan2(direccionAvance.x, direccionAvance.z));

        float bamboleoRoll = std::sin(cicloRemo * 6.28318f) * 1.5f;
        float cabeceoPitch = std::cos(cicloRemo * 6.28318f) * 0.8f;
        float flotacionOlas = std::sin(tiempoAnim * 1.0f) * 0.01f;
        posActual.y += flotacionOlas;

        glm::vec3 posLuzCanoa = posActual + glm::vec3(0.0f, 0.18f, 0.0f);
        float parpadeoCanoa = 0.85f + 0.15f * std::sin(tiempoAnim * 14.0f + 2.5f);
        glm::vec3 colorLuzCanoa = glm::vec3(0.85f, 0.45f, 0.12f) * parpadeoCanoa;

        glm::vec3 fogColor = glm::vec3(0.012f, 0.018f, 0.035f);
        float fogDensity = 0.012f;

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // --- D. Shader Principal: Luces y Cámara ---
        miShader.use();
        glUniform3fv(glGetUniformLocation(miShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 1. Luz Direccional (Luna)
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.ambient"), 0.04f, 0.05f, 0.08f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.diffuse"), 0.18f, 0.20f, 0.25f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.specular"), 0.15f, 0.15f, 0.20f);

        // 2. Luces Puntuales 0 a 3 (Veladoras del muelle - APAGADAS)
        for (unsigned int i = 0; i < 4; i++) {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            glm::vec3 posLuzReal = pointLightOffsets[i] + offsetMuelle;
            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzReal));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.09f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.032f);
        }

        // 3. Luz Puntual 4 (Canoa)
        glUniform3fv(glGetUniformLocation(miShader.ID, "pointLights[4].position"), 1, glm::value_ptr(posLuzCanoa));
        glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[4].ambient"), colorLuzCanoa.r * 0.05f, colorLuzCanoa.g * 0.05f, colorLuzCanoa.b * 0.05f);
        glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[4].diffuse"), colorLuzCanoa.r, colorLuzCanoa.g, colorLuzCanoa.b);
        glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[4].specular"), colorLuzCanoa.r * 0.6f, colorLuzCanoa.g * 0.6f, colorLuzCanoa.b * 0.6f);
        glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[4].constant"), 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[4].linear"), 0.18f);
        glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[4].quadratic"), 0.10f);

        // 4. Luces Puntuales 5 a 9 (5 Postes de la Plaza - Blanco neutro cálido)
        for (unsigned int i = 0; i < 5; i++) {
            int lightIndex = 5 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(postesPlaza[i]));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.005f, 0.005f, 0.005f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.68f, 0.62f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.25f, 0.25f, 0.22f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.60f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 1.40f);
        }

        // 5. Luces Puntuales 10 a 15 (6 Reflectores de la Orilla Original - Radio más contenido)
        for (unsigned int i = 0; i < 6; i++) {
            int lightIndex = 10 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < lucesOrilla.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesOrilla[i]));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.010f, 0.003f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.75f, 0.28f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.32f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.38f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.1f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.01f);
            }
        }

        // 6. Luces Puntuales 16 y 17 (Letrero Uranden y Árbol Central)
        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 16 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < lucesLetrero.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesLetrero[i]));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.015f, 0.005f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.10f, 0.70f, 0.25f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.70f, 0.45f, 0.18f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.22f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.20f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.1f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.01f);
            }
        }

        // 7. Luces Maestras del Puente (18 y 19 - Rasantes al puente)
        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 18 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            float t = (i == 0) ? 0.35f : 0.65f;
            glm::vec3 pIzq = glm::mix(postesPuente[0], postesPuente[1], t);
            glm::vec3 pDer = glm::mix(postesPuente[2], postesPuente[3], t);

            glm::vec3 posLuzPuente = (pIzq + pDer) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);

            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzPuente));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.008f, 0.001f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.32f, 0.05f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.30f, 0.15f, 0.03f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.70f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 1.80f);
        }

        // 8. Luces Puntuales 20 a 23 (4 Reflectores Orilla Opuesta - Radio más contenido)
        for (unsigned int i = 0; i < 4; i++) {
            int lightIndex = 20 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < lucesManantialOpuesto.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesManantialOpuesto[i]));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.010f, 0.003f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.25f, 0.78f, 0.28f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.32f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.38f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.0f, 0.0f, 0.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.1f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.01f);
            }
        }

        // 9. Reflector Cenital (Spotlight suavizado)
        glm::vec3 spotLightPos = glm::vec3(0.0f, 3.5f, 1.0f);
        glm::vec3 spotLightDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        glUniform3fv(glGetUniformLocation(miShader.ID, "spotLight.position"), 1, glm::value_ptr(spotLightPos));
        glUniform3fv(glGetUniformLocation(miShader.ID, "spotLight.direction"), 1, glm::value_ptr(spotLightDir));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.cutOff"), glm::cos(glm::radians(18.0f)));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.outerCutOff"), glm::cos(glm::radians(24.0f)));
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.ambient"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.diffuse"), 0.75f, 0.75f, 0.80f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.specular"), 0.40f, 0.40f, 0.45f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.constant"), 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.linear"), 0.14f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.quadratic"), 0.07f);

        // Niebla
        glUniform3fv(glGetUniformLocation(miShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), fogDensity);

        // --- E. Renderizado de Objetos 3D ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), true);
        glUniform1i(glGetUniformLocation(miShader.ID, "isLightSource"), false);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "materialAlpha"), 1.0f);

        // Mapa Completo
        glm::mat4 mapaModel = glm::mat4(1.0f);
        mapaModel = glm::translate(mapaModel, glm::vec3(0.0f, -0.2f, 0.0f));
        mapaModel = glm::scale(mapaModel, glm::vec3(0.2f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(mapaModel));
        modeloMapaCompleto.Draw(miShader);

        // Canoa
        glm::mat4 canoaModel = glm::mat4(1.0f);
        canoaModel = glm::translate(canoaModel, posActual);
        canoaModel = glm::rotate(canoaModel, glm::radians(anguloRumbo), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(bamboleoRoll), glm::vec3(0.0f, 0.0f, 1.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(cabeceoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        canoaModel = glm::scale(canoaModel, glm::vec3(2.0f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaModel));
        modeloCanoa.Draw(miShader);

        // --- Render Focos Físicos (Emisión pura sin sombras) ---
        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), false);
        glUniform1i(glGetUniformLocation(miShader.ID, "isLightSource"), true);
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), 0.0f);

        // 1. Postes de la Plaza
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 0.98f, 0.90f);
        for (unsigned int i = 0; i < postesPlaza.size(); i++) {
            glm::mat4 focoModel = glm::mat4(1.0f);
            focoModel = glm::translate(focoModel, postesPlaza[i]);
            focoModel = glm::scale(focoModel, glm::vec3(0.009f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // 2. Reflectores de la Orilla Original
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
        for (unsigned int i = 0; i < lucesOrilla.size(); i++) {
            glm::mat4 focoOrillaModel = glm::mat4(1.0f);
            focoOrillaModel = glm::translate(focoOrillaModel, lucesOrilla[i]);
            focoOrillaModel = glm::scale(focoOrillaModel, glm::vec3(0.019f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoOrillaModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // 2b. 4 Reflectores de la Orilla Opuesta
        for (unsigned int i = 0; i < lucesManantialOpuesto.size(); i++) {
            if ((int)i == reflectorOpuestoSeleccionado) {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 0.0f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            }

            glm::mat4 focoOpuestoModel = glm::mat4(1.0f);
            focoOpuestoModel = glm::translate(focoOpuestoModel, lucesManantialOpuesto[i]);
            focoOpuestoModel = glm::scale(focoOpuestoModel, glm::vec3(0.019f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoOpuestoModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // 3. Hilera de Foquitos y Cable Negro entre los 4 postes calibrables
        int numFocosHilera = 9;

        for (int cuerda = 0; cuerda < 2; cuerda++) {
            glm::vec3 pA = (cuerda == 0) ? postesPuente[0] : postesPuente[2];
            glm::vec3 pB = (cuerda == 0) ? postesPuente[1] : postesPuente[3];

            std::vector<glm::vec3> puntosCuerda(numFocosHilera);
            for (int i = 0; i < numFocosHilera; i++) {
                float t = (float)i / (float)(numFocosHilera - 1);
                glm::vec3 p = glm::mix(pA, pB, t);

                p.y -= std::sin(t * 3.14159f) * 0.075f;

                puntosCuerda[i] = p;
            }

            // A. Cable Negro continuo
            glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 0.03f, 0.03f, 0.03f);
            for (int i = 0; i < numFocosHilera - 1; i++) {
                glm::vec3 pInicio = puntosCuerda[i];
                glm::vec3 pFin = puntosCuerda[i + 1];

                glm::vec3 dirSegmento = pFin - pInicio;
                float distSegmento = glm::length(dirSegmento);
                glm::vec3 centroSegmento = (pInicio + pFin) * 0.5f;

                glm::vec3 dirNorm = glm::normalize(dirSegmento);
                float yawSegmento = glm::degrees(std::atan2(dirNorm.x, dirNorm.z));
                float pitchSegmento = glm::degrees(-std::asin(dirNorm.y));

                glm::mat4 cableModel = glm::mat4(1.0f);
                cableModel = glm::translate(cableModel, centroSegmento);
                cableModel = glm::rotate(cableModel, glm::radians(yawSegmento), glm::vec3(0.0f, 1.0f, 0.0f));
                cableModel = glm::rotate(cableModel, glm::radians(pitchSegmento), glm::vec3(1.0f, 0.0f, 0.0f));
                cableModel = glm::scale(cableModel, glm::vec3(0.0018f, 0.0018f, distSegmento * 0.5f));

                glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(cableModel));
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }

            // B. Bombillas Cálidas
            for (int i = 0; i < numFocosHilera; i++) {
                int idPosteActual = -1;
                if (cuerda == 0 && i == 0) idPosteActual = 0;
                else if (cuerda == 0 && i == numFocosHilera - 1) idPosteActual = 1;
                else if (cuerda == 1 && i == 0) idPosteActual = 2;
                else if (cuerda == 1 && i == numFocosHilera - 1) idPosteActual = 3;

                if (idPosteActual != -1 && idPosteActual == postePuenteSeleccionado) {
                    glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 5.0f, 5.0f, 0.0f);
                }
                else {
                    glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 4.5f, 2.2f, 0.4f);
                }

                glm::mat4 foquitoModel = glm::mat4(1.0f);
                foquitoModel = glm::translate(foquitoModel, puntosCuerda[i]);
                foquitoModel = glm::scale(foquitoModel, glm::vec3(0.007f));
                glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(foquitoModel));

                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }

        // Restaurar estado
        glUniform1i(glGetUniformLocation(miShader.ID, "isLightSource"), false);
        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), true);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), fogDensity);

        // --- F. Renderizado de la Superficie del Agua ---
        waterShader.use();
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3f(glGetUniformLocation(waterShader.ID, "dirLightDir"), -0.2f, -1.0f, -0.3f);

        glUniform1f(glGetUniformLocation(waterShader.ID, "time"), currentFrame * 0.08f);
        glUniform3fv(glGetUniformLocation(waterShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(waterShader.ID, "fogDensity"), fogDensity);

        glm::mat4 waterModel = glm::mat4(1.0f);
        waterModel = glm::translate(waterModel, glm::vec3(0.0f, -0.88f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(waterModel));

        glDisable(GL_CULL_FACE);
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // --- G. Skybox ---
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

        // --- HUD ---
        if (estacionActivaHUD != -1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);

            hudShader.use();
            glUniform1i(glGetUniformLocation(hudShader.ID, "hudTexture"), 0);
            glm::mat4 orthoProj = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
            hudShader.setMat4("projection", orthoProj);

            glm::mat4 orthoModel = glm::mat4(1.0f);
            orthoModel = glm::translate(orthoModel, glm::vec3(80.0f, 120.0f, 0.0f));
            orthoModel = glm::scale(orthoModel, glm::vec3(640.0f, 360.0f, 1.0f));
            hudShader.setMat4("model", orthoModel);

            glActiveTexture(GL_TEXTURE0);
            if (estacionActivaHUD == 0) glBindTexture(GL_TEXTURE_2D, texCedula1);
            else if (estacionActivaHUD == 1) glBindTexture(GL_TEXTURE_2D, texCedula2);
            else if (estacionActivaHUD == 2) glBindTexture(GL_TEXTURE_2D, texCedula3);
            else if (estacionActivaHUD == 3) glBindTexture(GL_TEXTURE_2D, texCedula4);

            glBindVertexArray(hudVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        showFPS(window);
    }

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteBuffers(1, &waterEBO);
    glDeleteVertexArrays(1, &hudVAO);
    glDeleteBuffers(1, &hudVBO);

    audioEngine.cleanup();
    glfwTerminate();
    return 0;
}