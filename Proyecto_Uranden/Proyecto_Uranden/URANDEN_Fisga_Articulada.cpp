// URANDEN, Michoacan - LANZAMIENTO NATURAL DE FISGA
// Basado en la practica de KeyFrames: K, L, M, R.

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.h"
#include "Model.h"
#include "PescadorFisgaRigGPU_NaturalV3.h"

namespace UrandenFisga {
const int WIDTH = 1024, HEIGHT = 720;
const int MAX_FRAMES = 20;
const char* ARCHIVO_TXT = "URANDEN_GestoNatural_V3_Keyframes.txt";
const char* VERSION_ANIMACION = "URANDEN_FISGA_V7_SUELTA_CON_BRAZO_Y_MIRA_AL_AGUA";

// Cada tramo dura aprox. i_max_steps / 60 segundos (independiente de los FPS).
int i_max_steps = 24; // mas agil pero todavia natural
float tiempoTramo = 0.0f;
int FrameIndex = 0, playIndex = 0;
bool play = false, fisgaLiberada = false;
bool observandoCaida = false;
float tiempoObservacion = 0.0f;
// Vuelo independiente: desde la suelta la fisga NUNCA vuelve a seguir la mano.
float tiempoDesdeSuelta = 0.0f;
glm::vec3 origenSuelta(0.0f);
// V9: vuelo balistico a donde apunta la fisga al salir de la mano.
// Solo cambiamos el desplazamiento del proyectil, no los keyframes del pescador.
bool fisgaClavada = false;
const float ALTURA_AGUA = -0.15f;      // Plano virtual de agua (sin malla de agua)
const float PROFUNDIDAD_CLAVADO = 0.10f;
const float RAPIDEZ_HORIZONTAL = 4.10f;
const float GRAVEDAD_FISGA = 9.0f;
const float LONGITUD_A_PUNTA = 0.42f * (0.55f + 0.949265f);
glm::vec3 velocidadSalida(0.0f);
float tiempoImpacto = 0.0f;

// Posicion del punto de agarre y velocidad instantanea de la fisga.
// Antes del impacto, la punta sigue la tangente del tiro parabolico.
glm::vec3 velocidadFisgaEn(float t) {
    return velocidadSalida + glm::vec3(0.0f, -GRAVEDAD_FISGA * t, 0.0f);
}
glm::vec3 centroFisgaEn(float t) {
    return origenSuelta + velocidadSalida * t +
           glm::vec3(0.0f, -0.5f * GRAVEDAD_FISGA * t * t, 0.0f);
}
float alturaPuntaFisgaEn(float t) {
    const glm::vec3 vel = velocidadFisgaEn(t);
    const glm::vec3 punta = glm::normalize(vel);
    return centroFisgaEn(t).y + LONGITUD_A_PUNTA * punta.y;
}
// Encuentra por biseccion el instante en que la punta entra al agua.
float calcularTiempoImpacto() {
    const float alturaObjetivo = ALTURA_AGUA - PROFUNDIDAD_CLAVADO;
    float a = 0.0f, b = 3.0f;
    while (alturaPuntaFisgaEn(b) > alturaObjetivo && b < 8.0f) b *= 1.5f;
    for (int i = 0; i < 42; ++i) {
        const float mitad = 0.5f * (a + b);
        if (alturaPuntaFisgaEn(mitad) > alturaObjetivo) a = mitad;
        else b = mitad;
    }
    return b;
}

struct FRAME {
    float boteBalanceo;
    float hombroX;       // hombro: atras (-) / adelante (+)
    float hombroZ;       // apertura lateral
    float codoX;         // flexion
    float munecaX;       // giro de la mano
    float pechoX;        // leve inclinacion del torso (negativo = adelante)
    float rodillas;      // flexion de ambas rodillas, en grados
    float fisgaAngulo;   // orientacion de la punta
    float suelta;        // 0: tomada, 1: liberada
    float vueloZ;        // avance desde la mano en el momento de soltar
    float vueloY;        // elevacion/caida desde la mano
};
FRAME KeyFrame[MAX_FRAMES] = {};
FRAME actual = {};

// Duraciones naturales: preparar con calma, impulsar rapido y recuperar despacio.
// No depende de los FPS: cada tramo dura segundos reales.
float duracionTramo(int tramo) {
    static const float segundos[18] = {
        0.38f, 0.42f, 0.40f, 0.38f, 0.36f,
        0.30f, 0.28f, 0.18f, 0.16f, 0.13f,
        0.18f, 0.24f, 0.34f, 0.38f, 0.42f,
        0.55f, 0.78f, 0.90f  // se agacha, observa el agua y se acomoda
    };
    // Si se graba una animacion distinta con K, usar duracion uniforme.
    const float base = (FrameIndex == 19 && tramo >= 0 && tramo < 18)
                       ? segundos[tramo] : 0.35f;
    return base * (static_cast<float>(i_max_steps) / 28.0f);
}

// Pendiente monotona de un canal en un keyframe. A diferencia de smoothstep,
// NO frena el movimiento en cada posicion intermedia. Evita sobrepasos.
float pendiente(int k, float FRAME::*campo) {
    if (FrameIndex < 2) return 0.0f;
    if (k <= 0)
        return (KeyFrame[1].*campo - KeyFrame[0].*campo) / duracionTramo(0);
    if (k >= FrameIndex - 1)
        return (KeyFrame[k].*campo - KeyFrame[k-1].*campo) / duracionTramo(k-1);

    const float h0 = duracionTramo(k - 1);
    const float h1 = duracionTramo(k);
    const float d0 = (KeyFrame[k].*campo - KeyFrame[k-1].*campo) / h0;
    const float d1 = (KeyFrame[k+1].*campo - KeyFrame[k].*campo) / h1;
    if (d0 * d1 <= 0.0f) return 0.0f;
    const float w0 = 2.0f * h1 + h0;
    const float w1 = h1 + 2.0f * h0;
    return (w0 + w1) / (w0 / d0 + w1 / d1);
}

float interpolarCanal(int tramo, float t, float FRAME::*campo) {
    const float a = KeyFrame[tramo].*campo;
    const float b = KeyFrame[tramo+1].*campo;
    const float h = duracionTramo(tramo);
    const float m0 = pendiente(tramo, campo) * h;
    const float m1 = pendiente(tramo+1, campo) * h;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float v = (2.0f*t3 - 3.0f*t2 + 1.0f) * a
                  + (t3 - 2.0f*t2 + t) * m0
                  + (-2.0f*t3 + 3.0f*t2) * b
                  + (t3 - t2) * m1;
    // Limite anatomico implícito en las posturas: nunca excede sus extremos.
    return std::max(std::min(a, b), std::min(v, std::max(a, b)));
}

FRAME interpolar(int tramo, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    FRAME r = {};
#define SUAVIZAR(campo) r.campo = interpolarCanal(tramo, t, &FRAME::campo)
    SUAVIZAR(boteBalanceo);
    SUAVIZAR(hombroX);
    SUAVIZAR(hombroZ);
    SUAVIZAR(codoX);
    SUAVIZAR(munecaX);
    SUAVIZAR(pechoX);
    SUAVIZAR(rodillas);
    SUAVIZAR(fisgaAngulo);
    SUAVIZAR(vueloZ);
    SUAVIZAR(vueloY);
#undef SUAVIZAR
    // La suelta es un EVENTO, no un angulo. Mantener la fisga en la mano
    // durante el impulso, ANTES del descenso del brazo.
    // Evento discreto, no interpolar como si fuera una rotacion.
    // La suelta debe coincidir con la extension visible del codo.
    // La postura 8 aun sostiene la fisga; la postura 9 ya la solto.
    // En el tramo 8->9 el evento sucede al 58%, NO durante la preparacion.
    const float puntoSuelta = (FrameIndex == 19 && tramo == 8) ? 0.58f : 0.93f;
    r.suelta = (t >= puntoSuelta) ? KeyFrame[tramo+1].suelta : KeyFrame[tramo].suelta;
    return r;
}

// ANIMACION PRECARGADA: 19 posturas / 18 transiciones.
// V7: suelta al 58% de 8->9; tras el tiro se agacha DE FRENTE.
// El codo continua extendiendose en las posturas 9,10,11.
// Orden: bote, hombroX, hombroZ, codoX, munecaX, pechoX,
//        rodillas, fisgaAngulo, suelta, vueloZ, vueloY.
void precargarAnimacion() {
    const FRAME poses[] = {
        { 0.00f, -8.00f, 1.00f, -25.00f, 2.00f, 1.00f, 4.00f, 38.00f, 0.00f, 0.00f, 0.00f }, // 00 reposo relajado
        { 0.30f, -12.00f, 2.00f, -30.00f, 3.00f, 1.50f, 5.00f, 40.00f, 0.00f, 0.00f, 0.00f }, // 01 acomoda agarre
        { 0.80f, -24.00f, 6.00f, -48.00f, 4.00f, 3.00f, 8.00f, 48.00f, 0.00f, 0.00f, 0.00f }, // 02 inicia carga
        { 1.20f, -40.00f, 10.00f, -78.00f, 5.00f, 6.00f, 11.00f, 56.00f, 0.00f, 0.00f, 0.00f }, // 03 gira el torso
        { 1.00f, -58.00f, 14.00f, -100.00f, 6.00f, 10.00f, 14.00f, 64.00f, 0.00f, 0.00f, 0.00f }, // 04 prepara impulso
        { 0.80f, -74.00f, 17.00f, -118.00f, 7.00f, 13.00f, 18.00f, 72.00f, 0.00f, 0.00f, 0.00f }, // 05 carga maxima
        { 0.50f, -82.00f, 18.00f, -120.00f, 7.00f, 14.00f, 20.00f, 78.00f, 0.00f, 0.00f, 0.00f }, // 06 pausa breve
        { 0.20f, -78.00f, 16.00f, -109.00f, 6.00f, 10.00f, 18.00f, 73.00f, 0.00f, 0.00f, 0.00f }, // 07 preimpulso con codo flexionado
        { -0.20f, -80.00f, 13.00f, -83.00f, 5.00f, 5.00f, 16.00f, 68.00f, 0.00f, 0.00f, 0.00f }, // 08 extension inicial del codo: todavia sostiene la fisga
        { -0.80f, -84.00f, 10.00f, -35.00f, 3.00f, -1.00f, 15.00f, 62.00f, 1.00f, 0.00f, 0.00f }, // 09 salida cuando la mano avanza: suelta al 58% de 8->9
        { -1.20f, -86.00f, 8.00f, -10.00f, 1.00f, 2.00f, 15.00f, 60.00f, 1.00f, 0.00f, 0.00f }, // 10 impulso desde las piernas
        { -1.00f, -87.00f, 6.00f, -5.00f, 0.00f, 6.00f, 19.00f, 62.00f, 1.00f, 0.00f, 0.00f }, // 11 acompanamiento: el torso empieza a doblarse HACIA EL FRENTE
        { -0.60f, -77.00f, 5.00f, -10.00f, 0.00f, 11.00f, 23.00f, 70.00f, 1.00f, 0.00f, 0.00f }, // 12 rodillas absorben el movimiento
        { -0.20f, -63.00f, 5.00f, -17.00f, -1.00f, 15.00f, 25.00f, 87.00f, 1.00f, 0.00f, 0.00f }, // 13 cuerpo sigue agachado
        { 0.10f, -52.00f, 4.00f, -24.00f, -1.00f, 18.00f, 26.00f, 105.00f, 1.00f, 0.00f, 0.00f }, // 14 observa el vuelo inclinado
        { 0.20f, -45.00f, 3.00f, -28.00f, 0.00f, 20.00f, 27.00f, 125.00f, 1.00f, 0.00f, 0.00f }, // 15 termina el seguimiento del brazo
        { 0.15f, -41.00f, 3.00f, -25.00f, 0.00f, 22.00f, 30.00f, 130.00f, 1.00f, 0.00f, 0.00f }, // 16 se inclina para buscar donde cayo
        { 0.10f, -36.00f, 2.00f, -23.00f, 0.00f, 25.00f, 32.00f, 133.00f, 1.00f, 0.00f, 0.00f }, // 17 se agacha hacia delante, sigue con la vista donde cayo la fisga
        { 0.05f, -33.00f, 2.00f, -26.00f, 0.00f, 23.00f, 30.00f, 136.00f, 1.00f, 0.00f, 0.00f }, // 18 mira al agua; conserva rodillas flexionadas y postura natural
    };
    FrameIndex = static_cast<int>(sizeof(poses) / sizeof(poses[0]));
    for (int i = 0; i < FrameIndex; ++i) KeyFrame[i] = poses[i];
    actual = KeyFrame[0];
    playIndex = 0;
    tiempoTramo = 0;
    play = false;
    fisgaLiberada = false;
    fisgaClavada = false;
    tiempoDesdeSuelta = 0.0f;
    observandoCaida = false;
    tiempoObservacion = 0.0f;
}
// Compatible con el mecanismo de la practica: graba una postura con K.
void saveFrame() {
    if (play || FrameIndex >= MAX_FRAMES) return;
    KeyFrame[FrameIndex++] = actual;
    std::cout << "Keyframe grabado: " << FrameIndex << '/' << MAX_FRAMES << '\n';
}

void guardarAnimacion() {
    if (FrameIndex < 2) {
        std::cout << "Graba al menos 2 keyframes para guardar.\n";
        return;
    }
    std::ofstream archivo(ARCHIVO_TXT);
    if (!archivo) {
        std::cout << "ERROR: no se pudo guardar " << ARCHIVO_TXT << '\n';
        return;
    }
    archivo.precision(9);
    archivo << VERSION_ANIMACION << '\n';
    archivo << FrameIndex << '\n';
    for (int i = 0; i < FrameIndex; ++i) {
        const FRAME& f = KeyFrame[i];
        archivo << f.boteBalanceo << ' ' << f.hombroX << ' ' << f.hombroZ << ' '
                << f.codoX << ' ' << f.munecaX << ' ' << f.pechoX << ' '
                << f.rodillas << ' ' << f.fisgaAngulo << ' ' << f.suelta << ' ' << f.vueloZ << ' '
                << f.vueloY << '\n';
    }
    std::cout << "Animacion guardada: " << FrameIndex << " poses en " << ARCHIVO_TXT << '\n';
}

bool cargarAnimacion() {
    std::ifstream archivo(ARCHIVO_TXT);
    if (!archivo) return false;
    std::string version;
    if (!(archivo >> version) || version != VERSION_ANIMACION) {
        std::cout << "TXT antiguo detectado: se usara la nueva animacion "
                     "precargada para evitar la liberacion tardia.\n";
        return false;
    }
    int n = 0;
    if (!(archivo >> n) || n < 2 || n > MAX_FRAMES) return false;
    FRAME temp[MAX_FRAMES] = {};
    for (int i = 0; i < n; ++i) {
        FRAME& f = temp[i];
        if (!(archivo >> f.boteBalanceo >> f.hombroX >> f.hombroZ
                      >> f.codoX >> f.munecaX >> f.pechoX
                      >> f.rodillas >> f.fisgaAngulo >> f.suelta >> f.vueloZ >> f.vueloY)) {
            std::cout << "Archivo de keyframes incompleto; usando poses precargadas.\n";
            return false;
        }
    }
    FrameIndex = n;
    for (int i = 0; i < n; ++i) KeyFrame[i] = temp[i];
    actual = KeyFrame[0];
    playIndex = 0;
    tiempoTramo = 0;
    play = false;
    fisgaLiberada = false;
    fisgaClavada = false;
    tiempoDesdeSuelta = 0.0f;
    observandoCaida = false;
    tiempoObservacion = 0.0f;
    std::cout << "Animacion cargada desde TXT: " << n << " keyframes.\n";
    return true;
}

void Animation(float dt) {
    if (!play || FrameIndex < 2) {
        // Un movimiento discreto de observacion despues del lanzamiento:
        // sigue agachado y busca con la mirada sin regresar a reposo.
        if (!play && observandoCaida && FrameIndex == 19) {
            tiempoObservacion += std::max(0.0f, std::min(dt, 0.05f));
            actual = KeyFrame[FrameIndex - 1];
            actual.pechoX += 0.65f * std::sin(tiempoObservacion * 1.35f);
            actual.hombroZ += 0.65f * std::sin(tiempoObservacion * 0.75f);
            actual.rodillas += 0.25f * std::sin(tiempoObservacion * 1.1f);
        }
        return;
    }
    // Avanzar por tiempo REAL; no perder tiempo cuando los FPS bajen.
    tiempoTramo += std::max(0.0f, std::min(dt, 0.25f));
    while (play && tiempoTramo >= duracionTramo(playIndex)) {
        tiempoTramo -= duracionTramo(playIndex);
        actual = KeyFrame[playIndex + 1];
        ++playIndex;
        if (playIndex >= FrameIndex - 1) {
            play = false;
            playIndex = 0;
            observandoCaida = (FrameIndex == 19 && fisgaLiberada);
            tiempoObservacion = 0.0f;
            std::cout << "Termino lanzamiento: pescador observa la caida.\n";
            break;
        }
    }
    if (play) {
        const float t = tiempoTramo / duracionTramo(playIndex);
        actual = interpolar(playIndex, t);
    }
}

void KeyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GL_TRUE);
    if (key == GLFW_KEY_K) saveFrame();
    if (key == GLFW_KEY_M) guardarAnimacion();
    if (key == GLFW_KEY_L) {
        if (play) play = false;
        else if (FrameIndex > 1) {
            actual = KeyFrame[0];
            playIndex = 0;
            tiempoTramo = 0;
            fisgaLiberada = false; // se vuelve a enganchar a la mano
            fisgaClavada = false;
            tiempoDesdeSuelta = 0.0f;
            observandoCaida = false;
            tiempoObservacion = 0.0f;
            play = true;
            std::cout << "Reproduciendo lanzamiento NATURAL con codo, rodillas y fisga.\n";
        }
    }
    if (key == GLFW_KEY_R) {
        play = false;
        FrameIndex = 0;
        playIndex = 0;
        tiempoTramo = 0;
        fisgaLiberada = false;
        fisgaClavada = false;
        tiempoDesdeSuelta = 0.0f;
        observandoCaida = false;
        tiempoObservacion = 0.0f;
        for (int i = 0; i < MAX_FRAMES; ++i) KeyFrame[i] = FRAME{};
        actual = FRAME{};
        actual.hombroX = -12.0f;
        actual.codoX = -20.0f;
        actual.fisgaAngulo = 40.0f;
        std::cout << "Memoria reiniciada. El TXT sigue intacto hasta pulsar M.\n";
    }
    if (key == GLFW_KEY_F && !play) {
        actual.suelta = actual.suelta > 0.5f ? 0.0f : 1.0f;
        if (actual.suelta < 0.5f) {
            fisgaLiberada = false;
            fisgaClavada = false;
            tiempoDesdeSuelta = 0.0f;
        }
        std::cout << (actual.suelta > 0.5f ? "Fisga suelta.\n" : "Fisga en mano.\n");
    }
}

// Controles manuales para construir keyframes nuevos luego de pulsar R.
void DoMovement(GLFWwindow* w, float dt,
                float& camX, float& camY, float& camZ) {
    if (!play) {
        const float ang = 90.0f * dt;
        if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) actual.hombroX -= ang;
        if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) actual.hombroX += ang;
        if (glfwGetKey(w, GLFW_KEY_1) == GLFW_PRESS) actual.codoX -= ang;
        if (glfwGetKey(w, GLFW_KEY_2) == GLFW_PRESS) actual.codoX += ang;
        if (glfwGetKey(w, GLFW_KEY_3) == GLFW_PRESS) actual.hombroZ -= ang;
        if (glfwGetKey(w, GLFW_KEY_4) == GLFW_PRESS) actual.hombroZ += ang;
        if (glfwGetKey(w, GLFW_KEY_5) == GLFW_PRESS) actual.fisgaAngulo -= ang;
        if (glfwGetKey(w, GLFW_KEY_6) == GLFW_PRESS) actual.fisgaAngulo += ang;
        if (glfwGetKey(w, GLFW_KEY_7) == GLFW_PRESS) actual.pechoX -= ang * 0.5f;
        if (glfwGetKey(w, GLFW_KEY_8) == GLFW_PRESS) actual.pechoX += ang * 0.5f;
        if (glfwGetKey(w, GLFW_KEY_T) == GLFW_PRESS) actual.rodillas = std::min(48.0f, actual.rodillas + ang * 0.5f);
        if (glfwGetKey(w, GLFW_KEY_G) == GLFW_PRESS) actual.rodillas = std::max(0.0f, actual.rodillas - ang * 0.5f);
        if (glfwGetKey(w, GLFW_KEY_9) == GLFW_PRESS) actual.munecaX -= ang;
        if (glfwGetKey(w, GLFW_KEY_0) == GLFW_PRESS) actual.munecaX += ang;
        if (glfwGetKey(w, GLFW_KEY_B) == GLFW_PRESS) actual.boteBalanceo -= ang * 0.25f;
        if (glfwGetKey(w, GLFW_KEY_N) == GLFW_PRESS) actual.boteBalanceo += ang * 0.25f;
        if (glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) actual.vueloZ -= dt;
        if (glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) actual.vueloZ += dt;
        if (glfwGetKey(w, GLFW_KEY_C) == GLFW_PRESS) actual.vueloY -= dt;
        if (glfwGetKey(w, GLFW_KEY_V) == GLFW_PRESS) actual.vueloY += dt;

        // Limites para que el codo, hombro y muneca no se doblen al reves.
        actual.hombroX = std::max(-108.0f, std::min(10.0f, actual.hombroX));
        actual.hombroZ = std::max(-12.0f, std::min(18.0f, actual.hombroZ));
        actual.codoX = std::max(-125.0f, std::min(0.0f, actual.codoX));
        actual.munecaX = std::max(-10.0f, std::min(10.0f, actual.munecaX));
        actual.pechoX = std::max(-20.0f, std::min(28.0f, actual.pechoX));
        actual.rodillas = std::max(0.0f, std::min(40.0f, actual.rodillas));
        actual.fisgaAngulo = std::max(20.0f, std::min(150.0f, actual.fisgaAngulo));
    }
    const float paso = 1.5f * dt;
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS) camZ = std::max(1.6f, camZ - paso);
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS) camZ = std::min(10.0f, camZ + paso);
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS) camX -= paso;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) camX += paso;
    if (glfwGetKey(w, GLFW_KEY_I) == GLFW_PRESS) camY += paso;
    if (glfwGetKey(w, GLFW_KEY_O) == GLFW_PRESS) camY -= paso;
}

void configurarLuces(GLuint shaderID, const glm::vec3& camara) {
    glUniform3fv(glGetUniformLocation(shaderID, "viewPos"), 1, glm::value_ptr(camara));
    glUniform3f(glGetUniformLocation(shaderID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
    glUniform3f(glGetUniformLocation(shaderID, "dirLight.ambient"), 0.8f, 0.8f, 0.8f);
    glUniform3f(glGetUniformLocation(shaderID, "dirLight.diffuse"), 0.8f, 0.8f, 0.8f);
    glUniform3f(glGetUniformLocation(shaderID, "dirLight.specular"), 0.3f, 0.3f, 0.3f);
    for (int i = 0; i < 5; ++i) {
        std::string s = "pointLights[" + std::to_string(i) + "].";
        glUniform3f(glGetUniformLocation(shaderID, (s + "position").c_str()), 0, 3, 1);
        glUniform3f(glGetUniformLocation(shaderID, (s + "ambient").c_str()), 0, 0, 0);
        glUniform3f(glGetUniformLocation(shaderID, (s + "diffuse").c_str()), 0, 0, 0);
        glUniform3f(glGetUniformLocation(shaderID, (s + "specular").c_str()), 0, 0, 0);
        glUniform1f(glGetUniformLocation(shaderID, (s + "constant").c_str()), 1.0f);
        glUniform1f(glGetUniformLocation(shaderID, (s + "linear").c_str()), 0.0f);
        glUniform1f(glGetUniformLocation(shaderID, (s + "quadratic").c_str()), 0.0f);
    }
    glUniform3f(glGetUniformLocation(shaderID, "spotLight.position"), 0, 3.5f, 1);
    glUniform3f(glGetUniformLocation(shaderID, "spotLight.direction"), 0, -1, -0.2f);
    glUniform3f(glGetUniformLocation(shaderID, "spotLight.ambient"), 0, 0, 0);
    glUniform3f(glGetUniformLocation(shaderID, "spotLight.diffuse"), 1.5f, 1.5f, 1.5f);
    glUniform3f(glGetUniformLocation(shaderID, "spotLight.specular"), 0.5f, 0.5f, 0.5f);
    glUniform1f(glGetUniformLocation(shaderID, "spotLight.constant"), 1.0f);
    glUniform1f(glGetUniformLocation(shaderID, "spotLight.linear"), 0.07f);
    glUniform1f(glGetUniformLocation(shaderID, "spotLight.quadratic"), 0.017f);
    glUniform1f(glGetUniformLocation(shaderID, "spotLight.cutOff"), glm::cos(glm::radians(18.0f)));
    glUniform1f(glGetUniformLocation(shaderID, "spotLight.outerCutOff"), glm::cos(glm::radians(24.0f)));
    glUniform3f(glGetUniformLocation(shaderID, "fogColor"), 0.12f, 0.13f, 0.18f);
    glUniform1f(glGetUniformLocation(shaderID, "fogDensity"), 0.0f);
    glUniform1i(glGetUniformLocation(shaderID, "useTexture"), 1);
    glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 1, 1, 1);
}
} // namespace UrandenFisga

int main() {
    using namespace UrandenFisga;
    if (!glfwInit()) { std::cerr << "ERROR: GLFW no pudo iniciar.\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT,
        "URANDEN - Lanzamiento de fisga", nullptr, nullptr);
    if (!w) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { glfwTerminate(); return -1; }
    glEnable(GL_DEPTH_TEST);
    glfwSetKeyCallback(w, KeyCallback);

    // Igual que tu BaileViejitos.cpp: Shader.use(), Shader.ID.
    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Model Bote("Models/Bote.obj");
    Model Fisga("Models/Fisga.obj");
    // NO usamos Pescador.obj rigido: ahora su esqueleto real mueve el brazo.
    PescadorFisgaRigGPU Pescador("Models/PescadorFisgaRig.bin",
                              "Models/Pescador_Textura.png");
    if (!Pescador.estaListo()) {
        std::cerr << "No se puede mostrar el pescador articulado.\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "URANDEN V9: PARABOLA HACIA DONDE APUNTA EL PESCADOR, SIN DESVIO LATERAL.\n";
    precargarAnimacion();
    if (!cargarAnimacion()) std::cout << "V7: 19 keyframes precargados. Pulsa L.\n";
    std::cout << "URANDEN GPU: Pulsa L para reproducir (hombro-codo-mano + fisga).\n";
    float anterior = static_cast<float>(glfwGetTime());
    float camX = 1.80f, camY = 1.40f, camZ = 3.10f; // Vista inicial mas cercana al brazo

    // Las luces son constantes: configurarlas UNA sola vez evita mas de 80
    // busquedas de uniformes por fotograma y reduce tirones de CPU.
    miShader.use();
    configurarLuces(miShader.ID, glm::vec3(camX, camY, camZ));
    const GLint uView = glGetUniformLocation(miShader.ID, "view");
    const GLint uProjection = glGetUniformLocation(miShader.ID, "projection");
    const GLint uModel = glGetUniformLocation(miShader.ID, "model");
    const GLint uViewPos = glGetUniformLocation(miShader.ID, "viewPos");

    while (!glfwWindowShouldClose(w)) {
        float ahora = static_cast<float>(glfwGetTime());
        float dt = std::min(0.25f, ahora - anterior);
        anterior = ahora;
        glfwPollEvents();
        DoMovement(w, dt, camX, camY, camZ);
        Animation(dt);

        // Como en la practica KeyFrames: interpolamos solo variables de pose.
        // Ahora 30 matrices de huesos se actualizan en la CPU;
        // la GPU anima los 128370 vertices al dibujarlos.
        Pescador.animar(actual.hombroX, actual.hombroZ, actual.codoX,
                        actual.munecaX, actual.pechoX, actual.rodillas);
        glViewport(0, 0, WIDTH, HEIGHT);
        glClearColor(0.12f, 0.14f, 0.18f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::vec3 camara(camX, camY, camZ);
        glm::mat4 view = glm::lookAt(camara, glm::vec3(0.0f, 0.49f, 0.15f), glm::vec3(0,1,0));
        glm::mat4 proyeccion = glm::perspective(glm::radians(45.0f),
                                   static_cast<float>(WIDTH) / HEIGHT, 0.1f, 100.0f);
        miShader.use();
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proyeccion));
        glUniform3fv(uViewPos, 1, glm::value_ptr(camara));

        // El bote balancea levemente y el pescador se mantiene sobre el bote.
        glm::mat4 boteM(1.0f);
        boteM = glm::rotate(boteM, glm::radians(actual.boteBalanceo), glm::vec3(0,0,1));
        boteM = glm::scale(boteM, glm::vec3(1.85f, 1.85f, 1.85f));
        glUniformMatrix4fv(uModel, 1, GL_FALSE,
                            glm::value_ptr(boteM));
        Bote.Draw(miShader);

        glm::mat4 pescadorM = glm::translate(glm::mat4(1.0f), glm::vec3(0,0.060f,0));
        pescadorM = glm::rotate(pescadorM, glm::radians(actual.boteBalanceo), glm::vec3(0,0,1));
        pescadorM = glm::scale(pescadorM, glm::vec3(0.60f));
        glUniformMatrix4fv(uModel, 1, GL_FALSE,
                            glm::value_ptr(pescadorM));
        Pescador.dibujar(pescadorM, view, proyeccion);

        // V9: MISMA suelta aprobada V7: 58% del tramo 8->9.
        // El eje local +Y de la fisga, en su pose de salida, marca hacia donde
        // apunta el pescador; el proyectil vuela hacia ESE frente, no a un lado.
        const glm::vec3 manoMundo = glm::vec3(
            pescadorM * glm::vec4(Pescador.puntoMano(), 1.0f));
        glm::vec3 fisgaEn = manoMundo;

        const bool lanzamientoEnCurso = play && FrameIndex == 19 &&
            (playIndex > 8 ||
             (playIndex == 8 && tiempoTramo >= duracionTramo(8) * 0.58f));
        if (lanzamientoEnCurso || actual.suelta >= 0.5f || fisgaLiberada) {
            if (!fisgaLiberada) {
                origenSuelta = manoMundo;
                fisgaLiberada = true;
                fisgaClavada = false;
                tiempoDesdeSuelta = 0.0f;

                // La fisga en la mano esta orientada por fisgaAngulo en X:
                // +Y local gira hacia +Z, que es el frente del PESCADOR.
                // Usamos su eje proyectado sobre el agua para que no se vaya
                // al costado de la embarcacion ni salte en otra direccion.
                const glm::mat4 giroFisga = glm::rotate(glm::mat4(1.0f),
                    glm::radians(actual.fisgaAngulo), glm::vec3(1, 0, 0));
                const glm::vec3 ejeApuntado = glm::normalize(glm::vec3(
                    giroFisga * glm::vec4(0, 1, 0, 0)));
                glm::vec3 adelante = glm::vec3(ejeApuntado.x, 0.0f, ejeApuntado.z);
                if (glm::length(adelante) < 0.0001f)
                    adelante = glm::vec3(0, 0, 1);
                adelante = glm::normalize(adelante);
                // La inclinacion de la fisga define el angulo inicial del tiro.
                // El angulo es positivo y moderado, como una fisga real.
                const float componenteVertical = glm::clamp(
                    RAPIDEZ_HORIZONTAL * ejeApuntado.y /
                    std::max(glm::length(glm::vec2(ejeApuntado.x, ejeApuntado.z)), 0.01f),
                    0.70f, 2.60f);
                velocidadSalida = adelante * RAPIDEZ_HORIZONTAL +
                                  glm::vec3(0.0f, componenteVertical, 0.0f);
                tiempoImpacto = calcularTiempoImpacto();
                std::cout << "V9: FISGA SOLTADA EN LA DIRECCION QUE APUNTA EL PESCADOR.\n";
            } else if (!fisgaClavada) {
                tiempoDesdeSuelta += std::max(0.0f, std::min(dt, 0.10f));
            }

            // Movimiento parabolico: horizontal siempre hacia delante,
            // subida breve al inicio y caida acelerada por gravedad.
            const float t = std::min(tiempoDesdeSuelta, tiempoImpacto);
            fisgaEn = centroFisgaEn(t);
            if (!fisgaClavada && tiempoDesdeSuelta >= tiempoImpacto) {
                fisgaClavada = true;
                std::cout << "V9: LA PUNTA SE CLAVO EN EL AGUA, AL FRENTE.\n";
            }
        }

        glm::mat4 fisgaM = glm::translate(glm::mat4(1.0f), fisgaEn);
        if (fisgaLiberada) {
            // El extremo +Y del OBJ apunta segun la velocidad instantanea;
            // empieza como estaba en la mano y se inclina al bajar al agua.
            const glm::vec3 punta = glm::normalize(velocidadFisgaEn(
                std::min(tiempoDesdeSuelta, tiempoImpacto)));
            const glm::vec3 ejeVarilla(0.0f, 1.0f, 0.0f);
            glm::vec3 ejeGiro = glm::cross(ejeVarilla, punta);
            const float seno = glm::length(ejeGiro);
            const float coseno = glm::clamp(glm::dot(ejeVarilla, punta), -1.0f, 1.0f);
            if (seno > 0.00001f) {
                ejeGiro /= seno;
                fisgaM = glm::rotate(fisgaM, std::atan2(seno, coseno), ejeGiro);
            } else if (coseno < 0.0f) {
                fisgaM = glm::rotate(fisgaM, glm::radians(180.0f), glm::vec3(1, 0, 0));
            }
        } else {
            // Pose en la mano de la version que ya quedo aprobada.
            fisgaM = glm::rotate(fisgaM, glm::radians(actual.fisgaAngulo),
                                 glm::vec3(1, 0, 0));
        }
        fisgaM = glm::scale(fisgaM, glm::vec3(0.42f));
        // Misma posicion de agarre de las versiones anteriores.
        fisgaM = glm::translate(fisgaM, glm::vec3(0, 0.55f, 0));
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(fisgaM));
        Fisga.Draw(miShader);
        glfwSwapBuffers(w);
    }
    Pescador.liberar();
    glfwTerminate();
    return 0;
}
