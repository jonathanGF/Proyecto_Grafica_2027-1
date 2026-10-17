#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

// Librerías para carga de imágenes y matemáticas (GLM y SOIL2)
#include "SOIL2/SOIL2.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Variables de la Cámara en primera persona
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f; // Rotación horizontal
float pitch = 0.0f; // Rotación vertical
float lastX = 800.0f / 2.0f;
float lastY = 600.0f / 2.0f;

// Control de velocidad con el tiempo (DeltaTime)
float deltaTime = 0.0f;
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

// Variables y función para el cálculo de FPS
double lastFpsTime = 0.0;
int nbFrames = 0;

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

// Vértices del cubo iluminado (Posiciones + Normales)
float cubeVertices[] = {
    // Posiciones          // Normales
    -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
    -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,

    -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,

    -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,

     0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,

    -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,

    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f
};

// Vértices del Skybox
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

// Posiciones de las 4 luces puntuales (velas/ofrendas)
glm::vec3 pointLightPositions[] = {
    glm::vec3(1.2f,  0.2f,  1.0f),
    glm::vec3(-1.2f, -0.2f,  0.8f),
    glm::vec3(-1.0f,  0.3f, -1.0f),
    glm::vec3(0.0f, -0.4f, -1.2f)
};

int main() {
    // 1. Inicializar GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. Crear ventana
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "El Camino de las Animas", NULL, NULL);
    if (window == NULL) {
        std::cout << "Fallo al crear la ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. Inicializar GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Fallo al inicializar GLEW" << std::endl;
        return -1;
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    glEnable(GL_DEPTH_TEST);

    // 4. Compilar shaders
    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Shader skyboxShader("shader/skybox.vert", "shader/skybox.frag");

    // 5. Configurar VAO/VBO del Cubo Iluminado
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // 6. Configurar VAO/VBO del Skybox
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // 7. Cargar el Cubemap con SOIL2
    GLuint cubemapTexture = SOIL_load_OGL_cubemap(
        "skybox/xpos.png",
        "skybox/xneg.png",
        "skybox/ypos.png",
        "skybox/yneg.png",
        "skybox/zpos.png",
        "skybox/zneg.png",
        SOIL_LOAD_RGB,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS
    );

    if (cubemapTexture == 0) {
        std::cout << "ERROR::SOIL2::Fallo al cargar el cubemap: " << SOIL_last_result() << std::endl;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    skyboxShader.use();
    glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

    // 8. Ciclo principal (Game Loop)
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);


        // Limpieza de pantalla
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Matrices de proyección y vista
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // ==========================================
        // PARTE A: DIBUJAR CUBOS CON ILUMINACIÓN
        // ==========================================
        glDisable(GL_CULL_FACE);
        miShader.use();

        glUniform3fv(glGetUniformLocation(miShader.ID, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 0.7f, 0.7f, 0.75f);

        // Luz Direccional (Luna)
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.ambient"), 0.03f, 0.04f, 0.07f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.diffuse"), 0.20f, 0.25f, 0.35f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.specular"), 0.15f, 0.20f, 0.25f);

        // Luces Puntuales (Velas)
        for (unsigned int i = 0; i < 4; i++) {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(pointLightPositions[i]));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.05f, 0.02f, 0.00f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.00f, 0.55f, 0.15f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 1.00f, 0.70f, 0.30f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.09f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.032f);
        }

        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Cubo central
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // 4 cubos pequeños indicadores de las velas
        for (unsigned int i = 0; i < 4; i++) {
            glm::mat4 lightModel = glm::mat4(1.0f);
            lightModel = glm::translate(lightModel, pointLightPositions[i]);
            lightModel = glm::scale(lightModel, glm::vec3(0.1f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(lightModel));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);

        // ==========================================
        // PARTE B: DIBUJAR EL SKYBOX
        // ==========================================
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

        glfwSwapBuffers(window);
        glfwPollEvents();
        showFPS(window);
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

    glfwTerminate();
    return 0;
}