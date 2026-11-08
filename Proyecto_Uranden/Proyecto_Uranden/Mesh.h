#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Shader.h"

// Estructura para almacenar atributos por vértice
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec4 Color;
};

// Estructura para almacenar datos de textura vinculados a la malla
struct Texture {
    unsigned int id;
    std::string type; // p. ej. "texture_diffuse" o "texture_specular"
    std::string path;
};

class Mesh {
public:
    // Datos de la malla
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;
    unsigned int VAO;
    glm::vec4 baseColorFactor;

    Mesh(std::vector<Vertex> vertices,
         std::vector<unsigned int> indices,
         std::vector<Texture> textures,
         glm::vec4 baseColorFactor = glm::vec4(1.0f)) {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        this->baseColorFactor = baseColorFactor;

        setupMesh();
    }

    void Draw(Shader& shader) {
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        bool hasDiffuseTexture = false;

        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);

            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse") {
                number = std::to_string(diffuseNr++);
                hasDiffuseTexture = true;
            }
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);

            glUniform1i(glGetUniformLocation(shader.ID, (name + number).c_str()), i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        // Cada malla decide si usa imagen o solamente el color de su material.
        // Esto evita reutilizar accidentalmente la textura de la malla anterior.
        glUniform1i(glGetUniformLocation(shader.ID, "useTexture"), hasDiffuseTexture ? 1 : 0);
        glUniform3f(
            glGetUniformLocation(shader.ID, "objectColor"),
            baseColorFactor.r,
            baseColorFactor.g,
            baseColorFactor.b
        );
        glUniform1f(glGetUniformLocation(shader.ID, "materialAlpha"), baseColorFactor.a);

        if (!hasDiffuseTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1i(glGetUniformLocation(shader.ID, "texture_diffuse1"), 0);
        }

        // Dibujar malla
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Restaurar estado activo
        glActiveTexture(GL_TEXTURE0);
    }

private:
    unsigned int VBO, EBO;

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // Vértices
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        // Índices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // Atributo 0: Posición
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        // Atributo 1: Normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        // Atributo 2: Coordenadas de textura
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        // Atributo 3: color pintado por vértice exportado desde Blender/glTF
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));

        glBindVertexArray(0);
    }
};
