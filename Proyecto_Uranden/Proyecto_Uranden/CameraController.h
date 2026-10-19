
#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

class CameraController
{
private:
    glm::vec3 posicion;
    glm::vec3 frente;
    glm::vec3 arriba;

    float yaw;
    float pitch;

    float velocidad;
    float sensibilidad;

    double ultimoMouseX;
    double ultimoMouseY;
    bool primerMovimiento;

    void ActualizarDireccion()
    {
        glm::vec3 direccion;

        direccion.x = cos(glm::radians(yaw)) *
            cos(glm::radians(pitch));

        direccion.y = sin(glm::radians(pitch));

        direccion.z = sin(glm::radians(yaw)) *
            cos(glm::radians(pitch));

        frente = glm::normalize(direccion);
    }

public:
    CameraController()
        : posicion(0.0f, 2.0f, 3.0f),
        frente(0.0f, 0.0f, -1.0f),
        arriba(0.0f, 1.0f, 0.0f),
        yaw(-90.0f),
        pitch(-30.0f),
        velocidad(0.7f),
        sensibilidad(0.12f),
        ultimoMouseX(0.0),
        ultimoMouseY(0.0),
        primerMovimiento(true)
    {
        ActualizarDireccion();
    }

    void Actualizar(GLFWwindow* ventana, float deltaTime)
    {
        float paso = velocidad * deltaTime;

        // Movimiento horizontal
        glm::vec3 frentePlano = glm::normalize(
            glm::vec3(frente.x, 0.0f, frente.z)
        );

        glm::vec3 derecha = glm::normalize(
            glm::cross(frentePlano, arriba)
        );

        // W - Avanzar
        if (glfwGetKey(ventana, GLFW_KEY_W) == GLFW_PRESS)
            posicion += frentePlano * paso;

        // S - Retroceder
        if (glfwGetKey(ventana, GLFW_KEY_S) == GLFW_PRESS)
            posicion -= frentePlano * paso;

        // A - Izquierda
        if (glfwGetKey(ventana, GLFW_KEY_A) == GLFW_PRESS)
            posicion -= derecha * paso;

        // D - Derecha
        if (glfwGetKey(ventana, GLFW_KEY_D) == GLFW_PRESS)
            posicion += derecha * paso;

        // ESPACIO - Subir
        if (glfwGetKey(ventana, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            posicion.y += paso;
        }

        // SHIFT IZQUIERDO - Bajar
        if (glfwGetKey(ventana, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
            posicion.y -= paso;
        }

        // Leer posicion del raton
        double mouseX, mouseY;
        glfwGetCursorPos(ventana, &mouseX, &mouseY);

        if (primerMovimiento)
        {
            ultimoMouseX = mouseX;
            ultimoMouseY = mouseY;
            primerMovimiento = false;
        }

        float movimientoX =
            (float)(mouseX - ultimoMouseX);

        float movimientoY =
            (float)(ultimoMouseY - mouseY);

        ultimoMouseX = mouseX;
        ultimoMouseY = mouseY;

        yaw += movimientoX * sensibilidad;
        pitch += movimientoY * sensibilidad;

        // Evitar que la camara de vueltas verticales
        if (pitch > 89.0f)
            pitch = 89.0f;

        if (pitch < -89.0f)
            pitch = -89.0f;

        ActualizarDireccion();
    }

    glm::mat4 ObtenerVista() const
    {
        return glm::lookAt(
            posicion,
            posicion + frente,
            arriba
        );
    }
    // Permite consultar la posicion del visitante
    glm::vec3 ObtenerPosicion() const
    {
        return posicion;
    }
};
