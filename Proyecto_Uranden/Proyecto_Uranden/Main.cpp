
#define GLEW_STATIC

#include<iostream>

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

// =========================================================================
// 1. CONSTANTES Y CONFIGURACIÓN GLOBAL
// =========================================================================
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Configuración de la Cámara
glm::vec3 cameraPos = glm::vec3(0.0f, 1.5f, 4.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = -10.0f;
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

#include <glm/glm.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <glm/gtc/type_ptr.hpp>

#include "CameraController.h"

#include "InteractionManager.h"

GLuint shaderProgram;

const GLint WIDTH = 800, HEIGHT = 600;

// Shaders
const GLchar* vertexShaderSource =
{
	"#version 330 core\n"
	"layout (location = 0) in vec3 position;\n"
	"uniform mat4 model;\n"
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"void main()\n"
	"{\n"
	"gl_Position = projection * view * model * vec4(position, 1.0f);\n"
	"}\0"
};

const GLchar* fragmentShaderSource = 
{
	"#version 330 core\n"
	"out vec4 color;\n"
	"void main()\n"
	"{\n"
	"color = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
	"}\n\0"
};

// Funciones de configuracion del shader

void CrearShader(void);




int main() {
	glfwInit();
	//Verificación de compatibilidad 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Practica 0", nullptr, nullptr);
	
	int screenWidth, screenHeight;

	glfwGetFramebufferSize(window, &screenWidth, &screenHeight);

	//Verificación de errores de creacion  ventana
	if (nullptr == window) 
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();

		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);
	// Capturar el raton para controlar la camara
	glfwSetInputMode(
		window,
		GLFW_CURSOR,
		GLFW_CURSOR_DISABLED
	);
	glewExperimental = GL_TRUE;

	//Verificación de errores de inicialización de glew

	if (GLEW_OK != glewInit()) {
		std::cout << "Failed to initialise GLEW" << std::endl;
		return EXIT_FAILURE;
	}

	// Imprimimos informacin de OpenGL del sistema
	std::cout << "> Version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "> Vendor: " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "> Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "> SL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;


	// Define las dimensiones del viewport
	glViewport(0, 0, screenWidth, screenHeight);

	CrearShader();

	// Set up vertex data (and buffer(s)) and attribute pointers
	GLfloat vertices[] = {
		// CANAL Y
		// Tronco principal
		-0.3f, -0.8f, 0.0f,
		 0.3f, -0.8f, 0.0f,
		 0.3f,  0.2f, 0.0f,
		 0.3f,  0.2f, 0.0f,
		-0.3f,  0.2f, 0.0f,
		-0.3f, -0.8f, 0.0f,

		// Brazo Izquierdo
		-0.3f,  0.2f, 0.0f,
		 0.0f,  0.2f, 0.0f,
		-0.6f,  0.8f, 0.0f,
		-0.6f,  0.8f, 0.0f,
		-0.9f,  0.8f, 0.0f,
		-0.3f,  0.2f, 0.0f,

		// Brazo Derecho
		 0.0f,  0.2f, 0.0f,
		 0.3f,  0.2f, 0.0f,
		 0.9f,  0.8f, 0.0f,
		 0.9f,  0.8f, 0.0f,
		 0.6f,  0.8f, 0.0f,
		 0.0f,  0.2f, 0.0f,

		 // === PARADOR EMPEDRADO ===
		  0.3f, -0.4f, 0.0f,
		  0.8f, -0.4f, 0.0f,
		  0.8f,  0.2f, 0.0f,
		  0.8f,  0.2f, 0.0f,
		  0.3f,  0.2f, 0.0f,
		  0.3f, -0.4f, 0.0f
	};


	GLuint VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	// Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0); // Note that this is allowed, the call to glVertexAttribPointer registered VBO as the currently bound vertex buffer object so afterwards we can safely unbind

	glBindVertexArray(0); // Unbind VAO (it's always a good thing to unbind any buffer/array to prevent strange bugs)


	
	
	// Controlador del visitante
	CameraController camara;

	// Administrador de estaciones interactivas
	InteractionManager interacciones;

	// Tiempo entre fotogramas
	float ultimoFrame = (float)glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		// Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
		glfwPollEvents();

		// Actualizar movimiento de camara
		float tiempoActual = (float)glfwGetTime();
		float deltaTime = tiempoActual - ultimoFrame;
		ultimoFrame = tiempoActual;

		camara.Actualizar(window, deltaTime);

		// Detectar proximidad y activar estaciones
		interacciones.Actualizar(
			window,
			camara.ObtenerPosicion()
		);

		// Cerrar con ESC
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		// Render
		// Clear the colorbuffer
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		// Draw our first triangle
		// 1. Proyección (Lente de la cámara)
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (GLfloat)screenWidth / (GLfloat)screenHeight, 0.1f, 100.0f);

		// 2. Vista
		glm::mat4 view = camara.ObtenerVista();

		// 3. Modelo (Posición de la geometría)
		glm::mat4 model = glm::mat4(1.0f);
		// Rotamos -90 grados en X para que la "Y" se acueste en el piso (el canal)
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		// Obtener las locaciones en el shader (Ojo: Para esto ocupas que shaderProgram sea global o retornarlo de CrearShader)
		// Para prueba rápida, si tu shaderProgram lo hiciste global o lo tienes a la mano:
		glUseProgram(shaderProgram);

		GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
		GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
		GLint projLoc = glGetUniformLocation(shaderProgram, "projection");

		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 24);
		glBindVertexArray(0);

		// Swap the screen buffers
		glfwSwapBuffers(window);
	}


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

// =========================================================================
// 3. FUNCIONES MATEMÁTICAS Y DE GENERACIÓN PROCEDURAL
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
    // Creamos el vertex shader y guardamos su identificador
	// Vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);


	// Verificamos los errores en tiempo de ejecución
	GLint success;
	GLchar infoLog[512];


	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

	
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}


	// Creamos el fragment shader y guardamos su identificador
	// Fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	// Verificamos los errores en tiempo de ejecución

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	// Creamos un nuevo programa y se asignamos
	// sus correspondientes vertex y fragment shaders
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);

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

    float sensitivity = 0.04f;
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
    Model modeloMuelle("Models/Meshy_AI_Weathered_Wooden_Dock_1018053025_texture.obj");
    Model modeloVela("Models/Meshy_AI_Marigold_Candle_Glow_1018054806_texture.obj");
    Model modeloCanoa("Models/canoa.obj");
    Model modeloTerreno("Models/Terreno.obj");

    // --- Motor de Audio ---
    AudioEngine audioEngine;
    if (!audioEngine.init()) {
        std::cerr << "ERROR::MINIAUDIO::No se pudo inicializar el motor de audio" << std::endl;
    }
    audioEngine.playMusic("audio/Ofrenda_en_el_Agua.mp3", 0.35f, true);
    audioEngine.playAmbient("audio/ambiente.mp3", 0.25f, true);

    // =====================================================================
    // 6. BUCLE PRINCIPAL DE RENDERIZADO
    // =====================================================================
    while (!glfwWindowShouldClose(window)) {
        // --- A. Tiempo y Procesamiento de Entradas ---
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

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
        float bamboleoRoll = std::sin(cicloRemo * 6.28318f) * 1.5f;  // Antes 2.8f
        float cabeceoPitch = std::cos(cicloRemo * 6.28318f) * 0.8f;  // Antes 1.5f
        float flotacionOlas = std::sin(tiempoAnim * 1.0f) * 0.01f;   // Antes * 2.5f y * 0.02f
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
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // --- D. Shader Principal: Paso de Iluminación, Niebla y Cámara ---
        miShader.use();
        glUniform3fv(glGetUniformLocation(miShader.ID, "viewPos"), 1, glm::value_ptr(cameraPos));
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
        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), true);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);

        // 1. Muelle (Desplazado con offsetMuelle)
        glm::mat4 dockModel = glm::mat4(1.0f);
        dockModel = glm::translate(dockModel, glm::vec3(0.0f, -0.75f, 0.0f) + offsetMuelle);
        dockModel = glm::scale(dockModel, glm::vec3(2.5f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(dockModel));
        modeloMuelle.Draw(miShader);

        // 2. Veladoras fijas en los postes (Sincronizadas con offsetMuelle)
        for (unsigned int i = 0; i < 4; i++) {
            glm::mat4 candleModel = glm::mat4(1.0f);
            candleModel = glm::translate(candleModel, pointLightOffsets[i] + offsetMuelle);
            candleModel = glm::scale(candleModel, glm::vec3(0.20f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(candleModel));
            modeloVela.Draw(miShader);
        }

        // 3. Canoa Animada (Recorre la nueva trayectoria adaptada al canal)
        glm::mat4 canoaModel = glm::mat4(1.0f);
        canoaModel = glm::translate(canoaModel, posActual);
        canoaModel = glm::rotate(canoaModel, glm::radians(anguloRumbo), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(bamboleoRoll), glm::vec3(0.0f, 0.0f, 1.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(cabeceoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        canoaModel = glm::scale(canoaModel, glm::vec3(2.0f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaModel));
        modeloCanoa.Draw(miShader);

        // 4. Terreno
        glm::mat4 terrainModel = glm::mat4(1.0f);
        terrainModel = glm::translate(terrainModel, glm::vec3(0.0f, -0.2f, 0.0f));
        terrainModel = glm::scale(terrainModel, glm::vec3(0.2f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(terrainModel));
        modeloTerreno.Draw(miShader);

        // --- F. Renderizado de la Superficie del Agua ---
        waterShader.use();
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShader.ID, "viewPos"), 1, glm::value_ptr(cameraPos));
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