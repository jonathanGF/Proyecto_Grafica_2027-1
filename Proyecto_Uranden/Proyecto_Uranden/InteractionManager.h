
#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <string>
#include <vector>

// Informacion basica de una estacion
struct Estacion
{
    std::string nombre;
    glm::vec3 posicion;
    std::string descripcion;
};

// Administrador de las interacciones del recorrido
class InteractionManager
{
private:
    std::vector<Estacion> estaciones;

    float distanciaInteraccion = 0.55f;
    bool teclaEAnterior = false;

public:

    InteractionManager()
    {
        // Coordenadas TEMPORALES para probar el sistema
        estaciones.push_back({
            "Embarcadero",
            glm::vec3(0.0f, 0.0f, 1.0f),
            "Inicio del recorrido y altar flotante."
            });

        estaciones.push_back({
            "Parador empedrado",
            glm::vec3(0.55f, 0.0f, 0.0f),
            "Espacio de las danzas tradicionales."
            });

        estaciones.push_back({
            "Puente de madera",
            glm::vec3(-0.7f, 0.0f, -0.8f),
            "Zona de observacion de los pescadores."
            });

        estaciones.push_back({
            "Islote Uranden",
            glm::vec3(0.7f, 0.0f, -1.5f),
            "Estacion sobre los manantiales."
            });
    }

    void Actualizar(
        GLFWwindow* ventana,
        const glm::vec3& posicionVisitante
    )
    {
        int estacionCercana = -1;
        float menorDistancia = distanciaInteraccion;

        // Buscar la estacion mas cercana
        for (int i = 0; i < (int)estaciones.size(); i++)
        {
            // Solo medimos la distancia horizontal XZ
            glm::vec2 visitante(
                posicionVisitante.x,
                posicionVisitante.z
            );

            glm::vec2 estacion(
                estaciones[i].posicion.x,
                estaciones[i].posicion.z
            );

            float distancia = glm::length(
                visitante - estacion
            );

            if (distancia <= menorDistancia)
            {
                menorDistancia = distancia;
                estacionCercana = i;
            }
        }

        // Mostrar aviso en el titulo de la ventana
        if (estacionCercana != -1)
        {
            std::string titulo =
                "El Camino de las Animas | "
                + estaciones[estacionCercana].nombre
                + " | Presiona E para interactuar";

            glfwSetWindowTitle(
                ventana,
                titulo.c_str()
            );
        }
        else
        {
            glfwSetWindowTitle(
                ventana,
                "El Camino de las Animas"
            );
        }

        // Detectar E solo una vez por pulsacion
        bool teclaEActual =
            glfwGetKey(ventana, GLFW_KEY_E) == GLFW_PRESS;

        if (teclaEActual && !teclaEAnterior)
        {
            if (estacionCercana != -1)
            {
                std::cout << "\n================================\n";
                std::cout << "ESTACION ACTIVADA: "
                    << estaciones[estacionCercana].nombre
                    << "\n";

                std::cout << estaciones[estacionCercana].descripcion
                    << "\n";

                std::cout << "================================\n";
            }
        }

        teclaEAnterior = teclaEActual;
    }
};
