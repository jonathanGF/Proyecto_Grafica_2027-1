// El Camino de las Animas - Baile de los Viejitos (escena independiente)
// Coreografia por maquina de estados; piel/esqueleto animados en la GPU.
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.h"
#include "Model.h"
#include "ViejitoRig.h"

static constexpr float PI_BAILE = 3.14159265358979323846f;

enum EstadoBaile {
    REPOSO = 0,
    SALTO_IZQUIERDA = 1,
    SALTO_DERECHA = 2,
    SALTO_ADELANTE = 3,
    RETROCESO_1 = 4,
    RETROCESO_2 = 5,
    RETROCESO_3 = 6,
    RETROCESO_4 = 7,
    PASO_DERECHO_BASTON = 8,
    PASO_IZQUIERDO = 9,
    ZAP_DERECHO_1 = 10,
    ZAP_IZQUIERDO = 11,
    ZAP_DERECHO_2 = 12,
    REMATE = 13
};

// Duraciones base en segundos; el factor ritmo las acorta poco a poco.
static const float DURACION_ESTADO[] = {
    0.0f, 0.74f, 0.74f, 0.86f, 0.40f, 0.40f, 0.40f, 0.40f,
    0.66f, 0.63f, 0.26f, 0.26f, 0.26f, 0.52f
};

static float limitar01(float x) {
    return glm::clamp(x, 0.0f, 1.0f);
}
static float suavizar(float x) {
    x = limitar01(x);
    return x * x * (3.0f - 2.0f * x);
}
// Subida marcada, bajada rapida y breve apoyo: el pie no queda flotando.
static float alzarPie(float t) {
    if (t <= 0.53f) return suavizar(t / 0.53f);
    if (t < 0.77f) return 1.0f - suavizar((t - 0.53f) / 0.24f);
    return 0.0f;
}
// Amortiguacion breve justo cuando el pie vuelve a tocar el suelo.
static float impacto(float t) {
    if (t <= 0.77f || t >= 0.97f) return 0.0f;
    return std::sin(PI_BAILE * (t - 0.77f) / 0.20f);
}

// Base de la postura en TODAS las fases: de pie y con rodillas dobladas.
// Al bailar, la espalda pasa suavemente de erguida a jorobada y viceversa;
// la mano izquierda permanece cerca de la cintura y la derecha junto al baston.
static PoseViejito posturaBase() {
    PoseViejito p;
    // Comienza casi erguido; la joroba aparece gradualmente durante el baile.
    p.espalda = 8.0f;
    p.espaldaMedia = 6.0f;
    p.espaldaAlta = 4.0f;
    p.cuello = -4.0f;

    p.musloIzq = -5.0f; p.musloDer = -5.0f;
    p.rodillaIzq = 15.0f; p.rodillaDer = 15.0f;
    p.pieIzq = -6.0f; p.pieDer = -6.0f;

    // Conservamos los angulos de brazos que ya NO atraviesan el torso.
    p.brazoIzqX = -7.0f;
    p.brazoIzqY = 7.0f;
    p.brazoIzqZ = -4.0f;
    p.codoIzqX = -4.0f;
    p.codoIzqY = 2.0f;
    p.codoIzqZ = -19.0f;
    p.brazoDerX = 32.0f;
    p.brazoDerZ = -18.0f;
    p.codoDerX = 10.0f;
    p.codoDerZ = -8.0f;
    // Muneca ligeramente flexionada y dedos cerrados alrededor del palo.
    p.munecaDerX = -5.0f;
    p.agarreDerecho = 0.72f;
    return p;
}

// Calcula articulaciones + posicion sobre el suelo de cada estado.
// Cada estado empieza y termina con la misma postura base, y su posicion
// final coincide con la inicial del siguiente: sin saltos artificiales.
static PoseViejito calcularBaile(int estado, float t, float reloj,
                                float& posX, float& posY, float& posZ,
                                float& bastonZ, float& bastonX) {
    PoseViejito p = posturaBase();
    posX = posY = posZ = bastonZ = bastonX = 0.0f;
    t = limitar01(t);

    // Postura viva: se endereza y luego se encorva poco a poco.
    // El tiempo es CONTINUO entre estados: no hay saltos al cambiar de movimiento.
    // Una vuelta de este ciclo dura ~4.3 segundos a ritmo 100%.
    const float flexion = suavizar(0.5f - 0.5f * std::cos(reloj * 1.45f));
    p.espalda      += 19.0f * flexion;
    p.espaldaMedia += 14.0f * flexion;
    p.espaldaAlta  += 11.0f * flexion;
    p.cuello       -= 7.0f * flexion;
    p.caderaX      += 2.0f * flexion;
    p.rodillaIzq   += 3.5f * flexion;
    p.rodillaDer   += 3.5f * flexion;

    // Mirada suave a ambos lados, sin girar ni tambalear todo el cuerpo.
    // Se usa el hueso Head (cabezaY), no la rotacion global del modelo.
    p.cabezaY = 15.0f * std::sin(reloj * 0.92f)
              + 3.0f * std::sin(reloj * 1.70f + 0.25f);
    p.cuello += 2.0f * std::sin(reloj * 0.85f);
    p.espaldaLateral += 0.55f * std::sin(reloj * 1.40f);
    p.espaldaAlta += 1.0f * std::sin(reloj * 1.10f);

    // Achaques constantes: temblor pequeno de rodillas, no del cuerpo entero.
    p.rodillaIzq += 1.25f * std::sin(reloj * 22.0f);
    p.rodillaDer += 1.25f * std::sin(reloj * 24.0f + 1.3f);

    // El brazo con baston RESPIRA y responde al ritmo, sin quedarse inmovil.
    // Amplitudes pequenas: nunca atraviesa el pecho.
    p.brazoDerX += 3.5f * std::sin(reloj * 1.8f);
    p.brazoDerZ -= 2.0f * std::sin(reloj * 1.35f + 0.5f);
    p.codoDerX += 7.0f * std::sin(reloj * 2.6f + 0.3f);
    // La muneca acompana el baston; los dedos no quedan congelados.
    p.munecaDerX += 4.0f * std::sin(reloj * 2.2f);
    p.munecaDerZ += 3.0f * std::sin(reloj * 1.9f + 0.4f);
    p.agarreDerecho = 0.90f + 0.06f * std::sin(reloj * 2.7f);

    // Cada estado agrega una mirada con entrada/salida suaves (sin tirones).
    float haciaLado = 0.0f;
    if (estado == SALTO_IZQUIERDA) haciaLado = 14.0f;
    else if (estado == SALTO_DERECHA) haciaLado = -14.0f;
    else if (estado == SALTO_ADELANTE) haciaLado = 6.0f;
    else if (estado >= RETROCESO_1 && estado <= RETROCESO_4)
        haciaLado = ((estado - RETROCESO_1) % 2 == 0) ? 7.0f : -7.0f;
    else if (estado == PASO_DERECHO_BASTON) haciaLado = -12.0f;
    else if (estado == PASO_IZQUIERDO) haciaLado = 10.0f;
    else if (estado == REMATE) haciaLado = -5.0f;
    p.cabezaY += haciaLado * std::sin(PI_BAILE * t);
    p.cabezaY = glm::clamp(p.cabezaY, -25.0f, 25.0f);

    if (estado == REPOSO) return p;

    // Los saltos parten y terminan exactamente en un punto de apoyo.
    if (estado == SALTO_IZQUIERDA || estado == SALTO_DERECHA ||
        estado == SALTO_ADELANTE) {
        // La traslacion ocurre cuando ya despego y termina antes de caer.
        float avance = suavizar((t - 0.16f) / 0.68f);
        float vuelo = std::pow(std::max(0.0f,std::sin(PI_BAILE*t)), 1.2f);
        float prepara = 1.0f - suavizar(t / 0.16f);
        float aterriza = suavizar((t-0.75f)/0.11f) *
                         (1.0f - suavizar((t-0.87f)/0.13f));
        posY = (estado == SALTO_ADELANTE ? 0.115f : 0.095f) * vuelo;
        p.rodillaIzq += 8.0f * prepara + 12.0f * aterriza + 7.0f * vuelo;
        p.rodillaDer += 8.0f * prepara + 12.0f * aterriza + 7.0f * vuelo;
        p.musloIzq -= 7.0f * vuelo;
        p.musloDer -= 7.0f * vuelo;
        p.pieIzq += 4.0f * vuelo;
        p.pieDer += 4.0f * vuelo;

        // Al despegar recoge ligeramente el brazo y dobla el codo.
        // Vuelve a la posicion inicial antes de pasar al siguiente estado.
        p.brazoDerX -= 8.0f * vuelo;
        p.brazoDerZ -= 3.0f * vuelo;
        p.codoDerX += 16.0f * vuelo + 5.0f * aterriza;
        p.munecaDerX -= 5.0f * vuelo;
        if (estado == SALTO_IZQUIERDA) {
            posX = -0.22f * avance;
            p.musloIzqZ += 4.0f * vuelo;
            bastonZ = 5.0f * vuelo;
        } else if (estado == SALTO_DERECHA) {
            posX = -0.22f + 0.22f * avance;
            p.musloDerZ -= 4.0f * vuelo;
            bastonZ = -5.0f * vuelo;
        } else { // SALTO_ADELANTE
            posZ = 0.18f * avance;
            // Pisoton de los dos pies, sincronizado con el baston.
            float golpeFinal = suavizar((t-0.72f)/0.10f) *
                               (1.0f-suavizar((t-0.85f)/0.15f));
            p.rodillaIzq += 7.0f * golpeFinal;
            p.rodillaDer += 7.0f * golpeFinal;
            bastonX = 12.0f * vuelo - 9.0f * golpeFinal;
        }
    }
    // Cuatro pasitos de reversa: cada uno desplaza exactamente 0.045 unidades.
    // Aqui cambia la posicion, no solo rota el tronco.
    else if (estado >= RETROCESO_1 && estado <= RETROCESO_4) {
        int numero = estado - RETROCESO_1;
        float avance = suavizar(t);
        float levanta = std::sin(PI_BAILE*t);
        posZ = 0.18f - 0.045f * (numero + avance);
        bool derecho = (numero % 2 == 0);
        if (derecho) {
            p.musloDer += 19.0f * levanta;
            p.rodillaDer += 25.0f * levanta;
            p.pieDer -= 10.0f * levanta;
        } else {
            p.musloIzq += 19.0f * levanta;
            p.rodillaIzq += 25.0f * levanta;
            p.pieIzq -= 10.0f * levanta;
        }
        // Hombros torpes y codo derecho acompana los pasitos de reversa.
        p.espaldaLateral += (derecho ? 2.2f : -2.2f) * levanta;
        p.caderaZ += (derecho ? 1.2f : -1.2f) * levanta;
        p.codoDerX += 7.0f * levanta;
        p.brazoDerZ -= 2.5f * levanta;
        bastonZ = (derecho ? -6.0f : 6.0f) * levanta;
        posY = 0.008f * levanta;
    }
    // Rodilla derecha MUY alta, baston arriba y golpe simultaneo al bajar.
    else if (estado == PASO_DERECHO_BASTON) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        p.musloDer -= 67.0f * levanta;
        p.rodillaDer += 73.0f * levanta + 9.0f * pisoton;
        p.pieDer -= 22.0f * levanta;
        p.rodillaIzq += 5.0f * levanta; // pierna de apoyo
        posY = 0.012f*levanta - 0.012f*pisoton;
        // No cruza el pecho: el brazo derecho sale hacia el costado.
        p.brazoDerX -= 9.0f * levanta;
        p.brazoDerZ -= 12.0f * levanta;  // no extenderlo frente a la cara
        p.codoDerX += 18.0f * levanta;  // flexiona codo para alzar baston
        p.munecaDerX -= 6.0f * levanta;
        p.munecaDerZ += 5.0f * levanta;
        bastonZ = 30.0f*levanta - 12.0f*pisoton;
        bastonX = -15.0f*levanta + 8.0f*pisoton;
    }
    else if (estado == PASO_IZQUIERDO) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        p.musloIzq -= 67.0f*levanta;
        p.rodillaIzq += 73.0f*levanta + 9.0f*pisoton;
        p.pieIzq -= 22.0f*levanta;
        p.rodillaDer += 5.0f*levanta;
        posY = 0.012f*levanta - 0.012f*pisoton;
        bastonZ = -8.0f*levanta + 4.0f*pisoton;
    }
    // Tres golpes cortos, uno por estado, para identificar bien cada pisada.
    else if (estado == ZAP_DERECHO_1 || estado == ZAP_IZQUIERDO ||
             estado == ZAP_DERECHO_2) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        bool derecho = (estado != ZAP_IZQUIERDO);
        if (derecho) {
            p.musloDer -= 38.0f*levanta;
            p.rodillaDer += 42.0f*levanta + 5.0f*pisoton;
            p.pieDer -= 12.0f*levanta;
        } else {
            p.musloIzq -= 38.0f*levanta;
            p.rodillaIzq += 42.0f*levanta + 5.0f*pisoton;
            p.pieIzq -= 12.0f*levanta;
        }
        posY = 0.005f*levanta - 0.009f*pisoton;
        bastonZ = (derecho ? 9.0f : -9.0f)*levanta;
    }
    // Remate: pequeño taloneo conjunto + golpe fuerte de ambas plantas.
    else if (estado == REMATE) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        p.musloIzq -= 14.0f*levanta;
        p.musloDer -= 14.0f*levanta;
        p.rodillaIzq += 15.0f*levanta + 12.0f*pisoton;
        p.rodillaDer += 15.0f*levanta + 12.0f*pisoton;
        p.pieIzq += 13.0f*levanta;
        p.pieDer += 13.0f*levanta;
        posY = 0.022f*levanta - 0.018f*pisoton;
        p.espalda += 3.0f*pisoton; // pequena reaccion, conserva la joroba
        p.codoDerX += 9.0f * levanta + 4.0f * pisoton;
        p.munecaDerX -= 5.0f * levanta;
        bastonZ = 17.0f*levanta - 14.0f*pisoton;
        bastonX = -9.0f*levanta + 7.0f*pisoton;
    }
    return p;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "No se pudo iniciar GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Baile de los Viejitos - Prueba", nullptr, nullptr);
    if (!window) {
        std::cerr << "No se pudo crear la ventana." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "No se pudo iniciar GLEW." << std::endl;
        glfwTerminate();
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // Los mismos shaders y cargador de modelos que usa Main.cpp.
    Shader miShader("shader/shader.vert", "shader/shader.frag");
    ViejitoRig viejito("Models/ViejitoRig.bin", "Models/Textura_Viejito.png");
    std::cout << "ESTADO DEL VIEJITO: " << (viejito.estaListo() ? "CARGADO" : "NO CARGADO") << std::endl;
    // La tecla B inicia el baile; R lo reinicia y vuelve a 1x.
    int estadoBaile = REPOSO;
    float tiempoEstado = 0.0f;
    float ritmo = 1.0f;            // Acelera una vez por vuelta, hasta 1.7x.
    int vueltas = 0;
    float ultimoFrameBaile = (float)glfwGetTime();
    float relojCuerpo = 0.0f;  // Tiempo compartido de postura y mirada (sin reiniciar entre estados).
    int cuadros = 0;
    double ultimoFps = glfwGetTime();
    Model baston("Models/Baston.obj");
    viejito.animarPose(posturaBase());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float ahora = (float)glfwGetTime();
        float deltaBaile = std::min(ahora - ultimoFrameBaile, 0.05f);
        ultimoFrameBaile = ahora;

        ++cuadros;
        if (ahora - ultimoFps >= 1.0) {
            int fps = static_cast<int>(cuadros / (ahora - ultimoFps));
            glfwSetWindowTitle(window, ("Baile de los Viejitos - FPS: " +
                std::to_string(fps) + " | Estado: " + std::to_string(estadoBaile) +
                " | Ritmo: " + std::to_string(static_cast<int>(ritmo*100)) + "%").c_str());
            cuadros = 0;
            ultimoFps = ahora;
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            estadoBaile = REPOSO;
            tiempoEstado = 0.0f;
            ritmo = 1.0f;
            vueltas = 0;
            relojCuerpo = 0.0f;
        }
        if (estadoBaile == REPOSO && glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
            estadoBaile = SALTO_IZQUIERDA;
            tiempoEstado = 0.0f;
        }

        // Maquina de estados: tiempo real, con aceleracion progresiva por vuelta.
        if (estadoBaile != REPOSO) {
            relojCuerpo += deltaBaile * ritmo;
            tiempoEstado += deltaBaile * ritmo;
            while (tiempoEstado >= DURACION_ESTADO[estadoBaile]) {
                tiempoEstado -= DURACION_ESTADO[estadoBaile];
                if (estadoBaile == REMATE) {
                    estadoBaile = SALTO_IZQUIERDA;
                    ++vueltas;
                    ritmo = std::min(1.70f, 1.0f + 0.10f * vueltas);
                } else {
                    ++estadoBaile;
                }
            }
        }
        float t = estadoBaile == REPOSO ? 0.0f :
            tiempoEstado / DURACION_ESTADO[estadoBaile];
        float posX=0.0f, posY=0.0f, posZ=0.0f;
        float bastonZ=0.0f, bastonX=0.0f;
        PoseViejito pose = calcularBaile(estadoBaile, t, relojCuerpo,
                                        posX, posY, posZ, bastonZ, bastonX);
        // Solo se actualizan las matrices de huesos, el modelo sigue en GPU.
        viejito.animarPose(pose);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glViewport(0, 0, 800, 600);
        glClearColor(0.12f, 0.13f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 camara(0.0f, 1.10f, 4.2f);
        glm::mat4 view = glm::lookAt(camara, glm::vec3(0.0f, 0.85f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            800.0f / 600.0f, 0.1f, 100.0f);

        miShader.use();
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "view"), 1,
            GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "projection"), 1,
            GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(miShader.ID, "viewPos"), 1,
            glm::value_ptr(camara));

        // Iluminacion equivalente a la que ya utiliza la escena principal.
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.ambient"), 0.8f, 0.8f, 0.8f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.diffuse"), 0.8f, 0.8f, 0.8f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.specular"), 0.3f, 0.3f, 0.3f);
        for (int i = 0; i < 5; ++i) {
            std::string nombre = "pointLights[" + std::to_string(i) + "].";
            glUniform3f(glGetUniformLocation(miShader.ID, (nombre + "position").c_str()), 0.0f, 3.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (nombre + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (nombre + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (nombre + "specular").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (nombre + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (nombre + "linear").c_str()), 0.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (nombre + "quadratic").c_str()), 0.0f);
        }
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.position"), 0.0f, 3.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.direction"), 0.0f, -1.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.ambient"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.diffuse"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.specular"), 0.0f, 0.0f, 0.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.constant"), 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.linear"), 0.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.quadratic"), 0.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.cutOff"), std::cos(glm::radians(12.0f)));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.outerCutOff"), std::cos(glm::radians(18.0f)));
        glUniform3f(glGetUniformLocation(miShader.ID, "fogColor"), 0.12f, 0.13f, 0.18f);
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), 0.0f);
        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), 1);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);

        // Desplazamiento verdadero por saltos/pasos. NO oscilar el modelo completo.
        glm::mat4 modeloViejito(1.0f);
        modeloViejito = glm::translate(modeloViejito,
            glm::vec3(posX, posY, posZ));
        viejito.dibujar(modeloViejito, view, projection);

        // ViejitoRig usa su propio shader para GPU skinning.
        // Reactivar el shader de textura normal para el baston OBJ.
        miShader.use();

        // AGARRE CORREGIDO: la vara sigue el punto de los dedos, no el centro
        // de la palma. Para que no atraviese el sombrero, NO copiamos toda
        // la orientacion de la mano: el palo permanece mayormente vertical.
        // Esta referencia corresponde a la pose original del GLB, cerca
        // de las primeras falanges del indice y el pulgar.
        const glm::vec3 agarreDedosReposo(-0.351f, 0.765f, 0.126f);
        glm::vec3 agarreAnimado = glm::vec3(
            viejito.movimientoManoDerecha() * glm::vec4(agarreDedosReposo, 1.0f));

        // Baston.obj mide ~1.90 unidades en Y. Agarrarlo por Y=+0.48
        // deja la bifurcacion por encima de la mano, pero bajo la cabeza.
        // Escala 0.52: la punta inferior queda cerca del suelo.
        // El eje del tronco esta desplazado hacia X negativo.
        const glm::vec3 puntoEnBaston(-0.045f, 0.48f, -0.015f);
        glm::mat4 modeloBaston = glm::translate(modeloViejito, agarreAnimado);
        // Inclina levemente HACIA EL EXTERIOR (X negativo: brazo derecho).
        // Una oscilacion pequena acompana el baile sin cruzar la cara.
        float inclinacionZ = 8.0f + 1.5f * std::sin(relojCuerpo * 2.1f)
            + glm::clamp(bastonZ * 0.16f, -4.0f, 4.0f);
        float inclinacionX = glm::clamp(bastonX * 0.14f, -3.5f, 3.5f);
        modeloBaston = glm::rotate(modeloBaston, glm::radians(inclinacionZ),
                                  glm::vec3(0,0,1));
        modeloBaston = glm::rotate(modeloBaston, glm::radians(inclinacionX),
                                  glm::vec3(1,0,0));
        modeloBaston = glm::scale(modeloBaston, glm::vec3(0.52f));
        modeloBaston = glm::translate(modeloBaston, -puntoEnBaston);
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID,"model"),1,
                           GL_FALSE,glm::value_ptr(modeloBaston));
        baston.Draw(miShader);

        glfwSwapBuffers(window);
    }

    viejito.liberar();
    glfwTerminate();
    return 0;
}
