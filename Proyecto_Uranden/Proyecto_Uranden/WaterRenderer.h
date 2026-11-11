#ifndef URANDEN_WATER_RENDERER_H
#define URANDEN_WATER_RENDERER_H
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <vector>
#include <string>

inline float alturaOndaAgua(float x, float z, float time, float strength) {
    return strength * (0.018f * std::sin(x * 1.12f + z * 0.56f - time * 1.15f)
        + 0.011f * std::sin(x * -0.80f + z * 1.80f - time * 0.86f + 1.7f)
        + 0.006f * std::sin(x * 2.40f + z * 1.50f - time * 1.43f + 4.1f));
}
struct LuzAgua {
    glm::vec3 posicion;
    glm::vec3 color;
};

// Copia solo color y profundidad, SIN duplicar modelos ni renderizar el mapa otra vez.
// El contexto de este Main es OpenGL 3.3 sin MSAA; no activar GLFW_SAMPLES aqui.
// Las texturas se reutilizan, salvo cuando cambia el tamano de la ventana.
class WaterRenderer {
public:
    GLuint color = 0, depth = 0;
    int width = 0, height = 0;
    void capture(int w, int h) {
        glActiveTexture(GL_TEXTURE12);
        if (w != width || h != height || color == 0) {
            width = w; height = h;
            if (color == 0) glGenTextures(1, &color);
            if (depth == 0) glGenTextures(1, &depth);
            glBindTexture(GL_TEXTURE_2D, color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            parameters(GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, depth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            parameters(GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
        glBindTexture(GL_TEXTURE_2D, color);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
        glBindTexture(GL_TEXTURE_2D, depth);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    void bind(GLuint program, GLuint cubemap, const std::vector<LuzAgua>& luces) const {
        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, color);
        glUniform1i(glGetUniformLocation(program, "sceneColor"), 12);
        glActiveTexture(GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_2D, depth);
        glUniform1i(glGetUniformLocation(program, "sceneDepth"), 13);
        glActiveTexture(GL_TEXTURE14);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
        glUniform1i(glGetUniformLocation(program, "skybox"), 14);
        glUniform1i(glGetUniformLocation(program, "hasSkybox"), cubemap != 0);
        glUniform2f(glGetUniformLocation(program, "screenSize"), float(width), float(height));
        int count = luces.size() < 20 ? int(luces.size()) : 20;
        glUniform1i(glGetUniformLocation(program, "lightCount"), count);
        for (int i = 0; i < count; ++i) {
            std::string suffix = "[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(program, ("lightPositions" + suffix).c_str()), 1, glm::value_ptr(luces[i].posicion));
            glUniform3fv(glGetUniformLocation(program, ("lightColors" + suffix).c_str()), 1, glm::value_ptr(luces[i].color));
        }
        glActiveTexture(GL_TEXTURE0);
    }
    void release() {
        glDeleteTextures(1, &color);
        glDeleteTextures(1, &depth);
        color = depth = 0; width = height = 0;
    }
private:
    static void parameters(GLint filter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
};
#endif
