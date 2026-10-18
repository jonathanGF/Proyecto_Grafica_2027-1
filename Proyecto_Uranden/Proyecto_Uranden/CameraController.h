#pragma once

#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Controlador basico del visitante.
// Version inicial: movimiento WASD con orientacion fija.

class CameraController
{
private:
    glm::vec3 posicion;
    float velocidad;

public:
    CameraController()
        : posicion(0.0f, 2.0f, 3.0f),
        velocidad(0.7f)
    {
    }

    void Actualizar(GLFWwindow* ventana, float deltaTime)
    {
        float paso = velocidad * deltaTime;

        // W: avanzar
        if (glfwGetKey(ventana, GLFW_KEY_W) == GLFW_PRESS)
        {
            posicion.z -= paso;
        }

        // S: retroceder
        if (glfwGetKey(ventana, GLFW_KEY_S) == GLFW_PRESS)
        {
            posicion.z += paso;
        }

        // A: desplazarse a la izquierda
        if (glfwGetKey(ventana, GLFW_KEY_A) == GLFW_PRESS)
        {
            posicion.x -= paso;
        }

        // D: desplazarse a la derecha
        if (glfwGetKey(ventana, GLFW_KEY_D) == GLFW_PRESS)
        {
            posicion.x += paso;
        }
    }

    glm::mat4 ObtenerVista() const
    {
        // Mantenemos la orientacion original.
        glm::vec3 direccion(0.0f, -2.0f, -3.0f);

        return glm::lookAt(
            posicion,
            posicion + direccion,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }
};
