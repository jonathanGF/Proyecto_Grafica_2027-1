#include<iostream>

#define GLEW_STATIC

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <glm/gtc/type_ptr.hpp>

#include "CameraController.h"

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



	glfwTerminate();
	return EXIT_SUCCESS;
}

void CrearShader()
{
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

	// Verificamos que no hay error en el programa
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}


	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);


}

