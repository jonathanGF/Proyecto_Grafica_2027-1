#ifndef _WIN64
#define GLEW_STATIC
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"

#include "Shader.h"
#include "Model.h"
#include "AudioEngine.h"
#include "Camera.h"
#include "ViejitoRig.h" 
#include "PescadorFisgaRigGPU_NaturalV3.h"

// Fuerza el uso de la GPU dedicada en laptops con graficos hibridos (NVIDIA Optimus / AMD).
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Cache de ubicaciones de uniforms
GLint ubicacionUniform(GLuint programa, const std::string& nombre) {
    static std::unordered_map<GLuint, std::unordered_map<std::string, GLint>> cache;
    auto& tabla = cache[programa];
    auto it = tabla.find(nombre);
    if (it != tabla.end()) return it->second;
    GLint loc = glGetUniformLocation(programa, nombre.c_str());
    tabla.emplace(nombre, loc);
    return loc;
}

// =========================================================================
// 1. CONSTANTES Y CONFIGURACIÓN GLOBAL
// =========================================================================
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.10f, 4.0f));

bool modoDron = false;
bool modoCaminataPOV = true;
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
double lastFpsTime = 0.0;
int nbFrames = 0;

float intensidadGlobal = 2.3284f;
bool baileViejitosActivo = true; // Control del baile con tecla J

// =========================================================================
// ESCALAS AJUSTADAS A 0.4f
// =========================================================================

// 1. Viejitos en la plaza
float escalaViejitos = 0.22f;
glm::vec3 offsetViejitos(0.352146f, -0.199837f, 0.954319f);
float velAnimViejitos = 0.233078f;
float rotacionViejitos = -109.633f;
float factorEscalaBaston = 0.759367f; // Hereda el 0.4f de modeloViejito

// 2. Pescador de la Fisga y su Bote
glm::vec3 posBoteFisga(1.01965f, -0.772005f, -1.54735f);
float yawBoteFisga = 3.45743f;
float escalaFisgaGlobal = 0.25f;
bool modoEdicionFisga = false;

// 3. Exhibición de Mariposeros (Canoa A1)
float escalaCanoaA1 = 1.17f; // Proporcional al mariposero en 0.4f (cámbialo a 0.4f si prefieres)
float escalaA1 = 0.42f;
const float SEPARACION_PESCADOR = 0.225f;
float pisoCanoaA1 = -0.07f;
const float PIES_A_ORIGEN = 0.43f;

// 4. Procesión de Canoas y Copaleras
float escalaCanoaProcesion = 0.35f;
float alturaCanoasProcesion = -0.210887f;
float escalaCopalera = 0.85f; // Hereda el 0.4f de canoaMat
glm::vec3 offsetCopalera = glm::vec3(0.0f, 0.331801f, 0.05f);
glm::vec3 offsetFuegoSahumador = glm::vec3(0.0f, 0.45f, 0.25f);

glm::vec3 offsetLuzProa = glm::vec3(0.0f, 0.30f, 2.5f);
glm::vec3 offsetLuzPopa = glm::vec3(0.0f, 0.30f, -2.5f);

// =========================================================================
// LOGICA DE LANZAMIENTO DE FISGA (LUIS - ADAPTADA PARA LA ESCENA)
// =========================================================================
namespace UrandenFisga {
    const int MAX_FRAMES = 20;
    int i_max_steps = 24;
    float tiempoTramo = 0.0f;
    int FrameIndex = 0, playIndex = 0;
    bool play = false;
    bool fisgaLiberada = false;
    bool observandoCaida = false;
    float tiempoObservacion = 0.0f;
    float tiempoDesdeSuelta = 0.0f;
    glm::vec3 origenSuelta(0.0f);
    bool fisgaClavada = false;

    const float ALTURA_AGUA = -0.88f;
    const float PROFUNDIDAD_CLAVADO = 0.06f;
    const float RAPIDEZ_HORIZONTAL = 2.20f;
    const float GRAVEDAD_FISGA = 6.5f;
    glm::vec3 velocidadSalida(0.0f);
    float tiempoImpacto = 0.0f;

    struct FRAME {
        float boteBalanceo, hombroX, hombroZ, codoX, munecaX, pechoX, rodillas;
        float fisgaAngulo, suelta, vueloZ, vueloY;
    };
    FRAME KeyFrame[MAX_FRAMES] = {};
    FRAME actual = {};

    float duracionTramo(int tramo) {
        static const float segundos[18] = {
            0.38f, 0.42f, 0.40f, 0.38f, 0.36f,
            0.30f, 0.28f, 0.18f, 0.16f, 0.13f,
            0.18f, 0.24f, 0.34f, 0.38f, 0.42f,
            0.55f, 0.78f, 0.90f
        };
        const float base = (FrameIndex == 19 && tramo >= 0 && tramo < 18) ? segundos[tramo] : 0.35f;
        return base * (static_cast<float>(i_max_steps) / 28.0f);
    }

    float pendiente(int k, float FRAME::* campo) {
        if (FrameIndex < 2) return 0.0f;
        if (k <= 0) return (KeyFrame[1].*campo - KeyFrame[0].*campo) / duracionTramo(0);
        if (k >= FrameIndex - 1) return (KeyFrame[k].*campo - KeyFrame[k - 1].*campo) / duracionTramo(k - 1);
        const float h0 = duracionTramo(k - 1), h1 = duracionTramo(k);
        const float d0 = (KeyFrame[k].*campo - KeyFrame[k - 1].*campo) / h0;
        const float d1 = (KeyFrame[k + 1].*campo - KeyFrame[k].*campo) / h1;
        if (d0 * d1 <= 0.0f) return 0.0f;
        const float w0 = 2.0f * h1 + h0, w1 = h1 + 2.0f * h0;
        return (w0 + w1) / (w0 / d0 + w1 / d1);
    }

    float interpolarCanal(int tramo, float t, float FRAME::* campo) {
        const float a = KeyFrame[tramo].*campo, b = KeyFrame[tramo + 1].*campo;
        const float h = duracionTramo(tramo);
        const float m0 = pendiente(tramo, campo) * h, m1 = pendiente(tramo + 1, campo) * h;
        const float t2 = t * t, t3 = t2 * t;
        const float v = (2.0f * t3 - 3.0f * t2 + 1.0f) * a + (t3 - 2.0f * t2 + t) * m0
            + (-2.0f * t3 + 3.0f * t2) * b + (t3 - t2) * m1;
        return std::max(std::min(a, b), std::min(v, std::max(a, b)));
    }

    FRAME interpolar(int tramo, float t) {
        t = std::max(0.0f, std::min(1.0f, t));
        FRAME r = {};
        r.boteBalanceo = interpolarCanal(tramo, t, &FRAME::boteBalanceo);
        r.hombroX = interpolarCanal(tramo, t, &FRAME::hombroX);
        r.hombroZ = interpolarCanal(tramo, t, &FRAME::hombroZ);
        r.codoX = interpolarCanal(tramo, t, &FRAME::codoX);
        r.munecaX = interpolarCanal(tramo, t, &FRAME::munecaX);
        r.pechoX = interpolarCanal(tramo, t, &FRAME::pechoX);
        r.rodillas = interpolarCanal(tramo, t, &FRAME::rodillas);
        r.fisgaAngulo = interpolarCanal(tramo, t, &FRAME::fisgaAngulo);
        r.vueloZ = interpolarCanal(tramo, t, &FRAME::vueloZ);
        r.vueloY = interpolarCanal(tramo, t, &FRAME::vueloY);

        const float puntoSuelta = (FrameIndex == 19 && tramo == 8) ? 0.58f : 0.93f;
        r.suelta = (t >= puntoSuelta) ? KeyFrame[tramo + 1].suelta : KeyFrame[tramo].suelta;
        return r;
    }

    glm::vec3 velocidadFisgaEn(float t) {
        return velocidadSalida + glm::vec3(0.0f, -GRAVEDAD_FISGA * t, 0.0f);
    }
    glm::vec3 centroFisgaEn(float t) {
        return origenSuelta + velocidadSalida * t + glm::vec3(0.0f, -0.5f * GRAVEDAD_FISGA * t * t, 0.0f);
    }
    float alturaPuntaFisgaEn(float t) {
        const glm::vec3 vel = velocidadFisgaEn(t);
        const glm::vec3 punta = glm::normalize(vel);
        const float longPunta = 0.70f * ::escalaFisgaGlobal * (0.55f + 0.949265f);
        return centroFisgaEn(t).y + longPunta * punta.y;
    }
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

    void precargarAnimacion() {
        const FRAME poses[] = {
            { 0.00f,  -8.00f,  1.00f,  -25.00f,  2.00f,  1.00f,  4.00f,  38.00f, 0.0f, 0.0f, 0.0f },
            { 0.30f, -12.00f,  2.00f,  -30.00f,  3.00f,  1.50f,  5.00f,  40.00f, 0.0f, 0.0f, 0.0f },
            { 0.80f, -24.00f,  6.00f,  -48.00f,  4.00f,  3.00f,  8.00f,  48.00f, 0.0f, 0.0f, 0.0f },
            { 1.20f, -40.00f, 10.00f,  -78.00f,  5.00f,  6.00f, 11.00f,  56.00f, 0.0f, 0.0f, 0.0f },
            { 1.00f, -58.00f, 14.00f, -100.00f,  6.00f, 10.00f, 14.00f,  64.00f, 0.0f, 0.0f, 0.0f },
            { 0.80f, -74.00f, 17.00f, -118.00f,  7.00f, 13.00f, 18.00f,  72.00f, 0.0f, 0.0f, 0.0f },
            { 0.50f, -82.00f, 18.00f, -120.00f,  7.00f, 14.00f, 20.00f,  78.00f, 0.0f, 0.0f, 0.0f },
            { 0.20f, -78.00f, 16.00f, -109.00f,  6.00f, 10.00f, 18.00f,  73.00f, 0.0f, 0.0f, 0.0f },
            { -0.20f,-80.00f, 13.00f,  -83.00f,  5.00f,  5.00f, 16.00f,  68.00f, 0.0f, 0.0f, 0.0f },
            { -0.80f,-84.00f, 10.00f,  -35.00f,  3.00f, -1.0f,  15.00f,  62.00f, 1.0f, 0.0f, 0.0f },
            { -1.20f,-86.00f,  8.00f,  -10.00f,  1.00f,  2.00f, 15.00f,  60.00f, 1.0f, 0.0f, 0.0f },
            { -1.00f,-87.00f,  6.00f,   -5.00f,  0.00f,  6.00f, 19.00f,  62.00f, 1.0f, 0.0f, 0.0f },
            { -0.60f,-77.00f,  5.00f,  -10.00f,  0.00f, 11.00f, 23.00f,  70.00f, 1.0f, 0.0f, 0.0f },
            { -0.20f,-63.00f,  5.00f,  -17.00f, -1.00f, 15.00f, 25.00f,  87.00f, 1.0f, 0.0f, 0.0f },
            { 0.10f, -52.00f,  4.00f,  -24.00f, -1.00f, 18.00f, 26.00f, 105.00f, 1.0f, 0.0f, 0.0f },
            { 0.20f, -45.00f,  3.00f,  -28.00f,  0.00f, 20.00f, 27.00f, 125.00f, 1.0f, 0.0f, 0.0f },
            { 0.15f, -41.00f,  3.00f,  -25.00f,  0.00f, 22.00f, 30.00f, 130.00f, 1.0f, 0.0f, 0.0f },
            { 0.10f, -36.00f,  2.00f,  -23.00f,  0.00f, 25.00f, 32.00f, 133.00f, 1.0f, 0.0f, 0.0f },
            { 0.05f, -33.00f,  2.00f,  -26.00f,  0.00f, 23.00f, 30.00f, 136.00f, 1.0f, 0.0f, 0.0f }
        };
        FrameIndex = static_cast<int>(sizeof(poses) / sizeof(poses[0]));
        for (int i = 0; i < FrameIndex; ++i) KeyFrame[i] = poses[i];
        actual = KeyFrame[0];
        playIndex = 0;
        tiempoTramo = 0.0f;
        play = false;
        fisgaLiberada = false;
        fisgaClavada = false;
        tiempoDesdeSuelta = 0.0f;
        observandoCaida = false;
        tiempoObservacion = 0.0f;
    }

    void Animation(float dt) {
        if (::modoEdicionFisga) return;

        if (!play) {
            if (observandoCaida && FrameIndex == 19) {
                tiempoObservacion += std::max(0.0f, std::min(dt, 0.05f));
                actual = KeyFrame[FrameIndex - 1];
                actual.pechoX += 0.65f * std::sin(tiempoObservacion * 1.35f);
                actual.hombroZ += 0.65f * std::sin(tiempoObservacion * 0.75f);
                actual.rodillas += 0.25f * std::sin(tiempoObservacion * 1.1f);
            }
            return;
        }

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
                std::cout << ">> Termino lanzamiento: pescador observa la caida." << std::endl;
                break;
            }
        }
        if (play) {
            const float t = tiempoTramo / duracionTramo(playIndex);
            actual = interpolar(playIndex, t);
        }
    }

    void IniciarLanzamiento() {
        if (!play && !::modoEdicionFisga) {
            actual = KeyFrame[0];
            playIndex = 0;
            tiempoTramo = 0;
            fisgaLiberada = false;
            fisgaClavada = false;
            tiempoDesdeSuelta = 0.0f;
            observandoCaida = false;
            tiempoObservacion = 0.0f;
            play = true;
            std::cout << ">> Animacion de Fisga Iniciada." << std::endl;
        }
    }
}

// =========================================================================
// 2. DATOS DE ESCENA Y TRAYECTORIAS FIJAS
// =========================================================================
glm::vec3 offsetMuelle = glm::vec3(-3.5f, 0.0f, 0.0f);

const glm::vec3 pointLightOffsets[4] = {
    glm::vec3(1.05f, -0.21f,  0.5f),
    glm::vec3(-1.05f, -0.21f,  0.5f),
    glm::vec3(-1.05f, -0.21f, -0.5f),
    glm::vec3(1.05f, -0.21f, -0.5f)
};

const std::vector<glm::vec3> postesPlaza = {
    glm::vec3(4.62443f, -0.108958f, 15.885f),
    glm::vec3(3.70724f, -0.111046f, 15.8275f),
    glm::vec3(2.78916f, -0.114986f, 15.7848f),
    glm::vec3(2.09366f, -0.116454f, 16.0913f),
    glm::vec3(2.08908f, -0.113338f, 16.7173f)
};

std::vector<glm::vec3> lucesOrilla = {
    glm::vec3(6.4632f,  -0.660309f, -0.730785f),
    glm::vec3(4.06971f, -0.651941f,  4.17367f),
    glm::vec3(3.24339f, -0.62345f,   7.06666f),
    glm::vec3(3.04058f, -0.623944f,  9.44348f),
    glm::vec3(2.94714f, -0.597321f, 12.0f),
    glm::vec3(5.20106f, -0.262329f, 15.4927f)
};

std::vector<glm::vec3> lucesLetrero = {
    glm::vec3(1.43314f, -0.634289f, -11.0765f),
    glm::vec3(0.0f,     3.68036f,  -14.6803f)
};

std::vector<glm::vec3> postesPuente = {
    glm::vec3(-0.254487f, -0.363107f, 16.8868f),
    glm::vec3(0.719089f, -0.35674f,  16.8865f),
    glm::vec3(-0.253564f, -0.370363f, 16.6411f),
    glm::vec3(0.726335f, -0.359537f, 16.64f)
};

std::vector<glm::vec3> postesPuente2 = {
    glm::vec3(-7.35653f, -0.430664f, -12.7874f),
    glm::vec3(-5.88413f, -0.426269f, -13.6162f),
    glm::vec3(-7.18257f, -0.440817f, -12.4f),
    glm::vec3(-5.69923f, -0.426434f, -13.2347f)
};

std::vector<glm::vec3> lucesManantialOpuesto = {
    glm::vec3(-3.08164f, -0.686701f,  3.60584f),
    glm::vec3(-3.11661f, -0.656584f,  7.2f),
    glm::vec3(-2.87594f, -0.676895f, 10.5f),
    glm::vec3(-2.59666f, -0.646597f, 13.5f)
};

struct FocoEstructura { glm::vec3 pos; float pitch; float yaw; };

std::vector<FocoEstructura> focosUranden = {
    { glm::vec3(0.472333f, -0.277844f, -11.3845f), 0.0f, 12.621f },
    { glm::vec3(1.043f, -0.351215f, -11.5507f), 0.0f, 19.0009f },
    { glm::vec3(1.5f, -0.354501f, -11.7209f), 0.0f, 14.1641f },
    { glm::vec3(2.07332f, -0.383588f, -11.8695f), 0.0f, 16.9766f }
};

std::vector<FocoEstructura> focosLetras = {
    { glm::vec3(0.604192f, -0.277844f, -10.8625f), 0.0f, 14.057f },
    { glm::vec3(1.19124f, -0.321721f, -11.0867f), 0.0f, 17.422f },
    { glm::vec3(1.64225f, -0.333041f, -11.2188f), 0.0f, 16.8869f },
    { glm::vec3(2.20184f, -0.383588f, -11.415f), 0.0f, 16.8951f }
};

int estacionActivaHUD = -1;
bool eKeyPressed = false;

std::vector<glm::vec3> puntosCircuito = {
    glm::vec3(-1.0f, -0.65f,  0.5f), glm::vec3(-1.1f, -0.65f,  3.5f),
    glm::vec3(-1.1f, -0.65f,  6.5f), glm::vec3(-0.9f, -0.65f,  9.5f),
    glm::vec3(-0.4f, -0.65f, 12.0f), glm::vec3(0.4f, -0.65f, 13.5f),
    glm::vec3(1.1f, -0.65f, 12.0f),  glm::vec3(1.5f, -0.65f,  9.5f),
    glm::vec3(1.6f, -0.65f,  6.5f),  glm::vec3(1.5f, -0.65f,  3.5f),
    glm::vec3(1.0f, -0.65f,  0.5f),  glm::vec3(0.0f, -0.65f, -0.6f)
};

struct Estacion { glm::vec3 posicion; float radioDeteccion; bool interactuable; bool activa; };
std::vector<Estacion> estaciones = {
    {glm::vec3(-1.40175f, 0.0f, 1.49381f), 3.0f, false, false},
    {glm::vec3(1.61685f, 0.0f, -9.52137f), 3.0f, false, false},
    {glm::vec3(2.15513f, 0.0f, 15.4634f), 3.0f, false, false},
    {glm::vec3(0.611198f, 0.0f, 6.4615f), 3.0f, false, false}
};

struct PosicionAparcamiento { glm::vec3 posicion; float yaw; };
const std::vector<PosicionAparcamiento> aparcamientosProcesion = {
    { glm::vec3(0.20f, -0.65f, 11.2f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  9.8f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  8.4f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  7.0f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  5.6f),  90.0f }
};

bool animacionProcesionActiva = false;
float tiempoProcesion = 0.0f;
const int NUM_CANOAS_PROC = 5;
const float VEL_PROCESION = 1.2f, ACEL_PROCESION = 1.5f, ESPACIO_CANOAS = 1.8f, FRENADO_PROCESION = 0.6f, LATERAL_ATRAQUE = 0.7f, GIRO_ATRAQUE = 0.5f;

const std::vector<glm::vec3> puntosProcesion = {
    glm::vec3(-1.60839f, -0.65f, -0.249108f), glm::vec3(-1.34177f, -0.65f,  0.640236f),
    glm::vec3(-1.27887f, -0.65f,  1.4768f),   glm::vec3(-1.19585f, -0.65f,  2.7161f),
    glm::vec3(-1.1114f,  -0.65f,  4.21009f),  glm::vec3(-0.81947f, -0.65f, 10.3526f),
    glm::vec3(-0.468589f,-0.65f, 11.7561f),  glm::vec3(0.332512f,-0.65f, 11.9176f),
    glm::vec3(0.976588f,-0.65f, 10.0949f),  glm::vec3(1.11899f, -0.65f,  8.63335f),
    glm::vec3(1.33907f, -0.65f,  7.06824f),  glm::vec3(1.63217f, -0.65f,  4.76648f),
    glm::vec3(1.68298f, -0.65f,  3.11731f),  glm::vec3(1.44546f, -0.65f,  1.66692f),
    glm::vec3(1.22888f, -0.65f,  1.02546f),  glm::vec3(-0.189947f,-0.65f, -0.172758f),
    glm::vec3(-0.42318f, -0.65f,  2.20993f),  glm::vec3(0.322296f,-0.65f,  5.25056f)
};

struct RutaCanoa { std::vector<glm::vec3> pts; std::vector<float> dist; };
std::vector<RutaCanoa> rutasProcesion;
glm::vec3 posProc[NUM_CANOAS_PROC];
float yawProc[NUM_CANOAS_PROC];

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

bool teclaPulsada(GLFWwindow* window, int tecla) {
    static bool estado[GLFW_KEY_LAST + 1] = { false };
    bool ahora = glfwGetKey(window, tecla) == GLFW_PRESS;
    bool pulsada = ahora && !estado[tecla];
    estado[tecla] = ahora;
    return pulsada;
}

glm::mat4 rotarEnPivote(const glm::mat4& M, const glm::vec3& pivote, float grados, const glm::vec3& eje) {
    glm::mat4 r = glm::translate(M, pivote);
    r = glm::rotate(r, glm::radians(grados), eje);
    return glm::translate(r, -pivote);
}

// =========================================================================
// A1: EXHIBICION DE LOS MARIPOSEROS 
// =========================================================================
glm::vec3 posCanoaA1 = glm::vec3(-1.87317f, -0.806756f, 10.7197f);
float yawCanoaA1 = 101.509f;
bool reproduciendoA1 = true;
float tiempoAnimA1 = 0.0f;
const float DURACION_TRAMO_A1 = 1.0f;
const char* ARCHIVO_KF_A1 = "animaciones/A1_mariposeros.txt";

const glm::vec3 P_PELVIS = glm::vec3(-0.0017f, -0.0732f, -0.0031f);
const glm::vec3 P_HOMBRO_DER = glm::vec3(-0.168f, 0.1732f, -0.04062f);
const glm::vec3 P_HOMBRO_IZQ = glm::vec3(0.1578f, 0.1793f, -0.04094f);
const glm::vec3 P_CODO_DER = glm::vec3(-0.2226f, 0.1318f, -0.03336f);
const glm::vec3 P_CODO_IZQ = glm::vec3(0.2101f, 0.1384f, -0.03174f);
const glm::vec3 P_MANO_IZQ = glm::vec3(0.3037f, 0.05045f, 0.02456f);
const glm::vec3 P_CADERA_DER = glm::vec3(-0.04884f, -0.07459f, -0.002458f);
const glm::vec3 P_CADERA_IZQ = glm::vec3(0.04547f, -0.07175f, -0.003723f);
const glm::vec3 P_RODILLA_DER = glm::vec3(-0.0635f, -0.213f, -0.01632f);
const glm::vec3 P_RODILLA_IZQ = glm::vec3(0.05895f, -0.2137f, -0.01617f);
const glm::vec3 P_AGARRE = glm::vec3(-0.9415f, 0.1457f, 0.06631f);
const glm::vec3 P_EJE_ALAS = glm::vec3(-0.9415f, 0.6304f, 0.06631f);

const float L_MUSLO = 0.14f, L_PIERNA = 0.22f;

struct PoseA1 {
    float torso, cadera, rodilla, hombroExtFrente, hombroExtLado, codoExt;
    float hombroIntFrente, hombroIntLado, codoInt, redInclinacion, redLado, redApertura;
};
PoseA1 poseA1 = {};

const int NUM_ARTIC_A1 = 12;
const char* NOMBRES_ARTIC_A1[NUM_ARTIC_A1] = {
    "torso", "cadera", "rodilla", "hombroExtFrente", "hombroExtLado", "codoExt",
    "hombroIntFrente", "hombroIntLado", "codoInt", "redInclinacion", "redLado", "redApertura"
};
float* articA1(PoseA1& p, int i) {
    float* campos[NUM_ARTIC_A1] = {
        &p.torso, &p.cadera, &p.rodilla, &p.hombroExtFrente, &p.hombroExtLado, &p.codoExt,
        &p.hombroIntFrente, &p.hombroIntLado, &p.codoInt, &p.redInclinacion, &p.redLado, &p.redApertura };
    return campos[i];
}
int articSeleccionadaA1 = -1;

std::vector<PoseA1> keyframesA1 = {
    {  0,  0,  0,   0,   0,  0,   0,   0,  0,    0,   0,   0 },
    { -5,  0,  3,  10,  -5, 10,   5, -15, 35,   20,   0,   0 },
    { 25,  9, 15,  30, -15, 10,  25, -30, 35,   55,   0,   0 },
    { 45, 15, 23,  55, -15,  0,  50, -30, 20,   95,   0,   0 },
    { 55, 20, 30,  60, -15,  0,  55, -30, 15,  115,   0,   0 },
    { 60, 22, 33,  55, -15,  0,  50, -30, 15,  120,   0, -18 },
    { 60, 22, 33,  55, -15,  0,  50, -30, 15,  120,  15, -40 },
    { 56, 20, 30,  58, -15,  5,  53, -30, 20,  127, -10, -55 },
    { 40, 15, 23,  65, -15, 10,  60, -30, 30,  115,   0, -55 },
    { 20,  8, 12,  80, -15, 20,  75, -30, 40,  115,   0, -55 },
    {  0,  0,  2,  95, -10, 25,  90, -25, 50,  125,   0, -55 },
    { -5,  0,  0, 100, -10, 20,  95, -25, 45,  120,   8, -55 },
    {  0,  0,  0,  40,  -5, 20,  35, -20, 45,   70,   0, -25 },
    {  0,  0,  0,   0,   0,  0,   0,   0,  0,    0,   0,   0 }
};

PoseA1 interpolarPose(const PoseA1& a, const PoseA1& b, float t) {
    PoseA1 r;
    PoseA1& ra = const_cast<PoseA1&>(a);
    PoseA1& rb = const_cast<PoseA1&>(b);
    for (int i = 0; i < NUM_ARTIC_A1; i++)
        *articA1(r, i) = *articA1(ra, i) + (*articA1(rb, i) - *articA1(ra, i)) * t;
    return r;
}

bool guardarKeyframesA1(const char* ruta) {
    std::ofstream f(ruta);
    if (!f.is_open()) return false;
    f << keyframesA1.size() << "\n";
    for (auto& kf : keyframesA1) {
        for (int i = 0; i < NUM_ARTIC_A1; i++) f << *articA1(kf, i) << (i + 1 < NUM_ARTIC_A1 ? " " : "\n");
    }
    return true;
}

bool cargarKeyframesA1(const char* ruta) {
    std::ifstream f(ruta);
    if (!f.is_open()) return false;
    size_t n = 0;
    f >> n;
    std::vector<PoseA1> leidos(n);
    for (size_t k = 0; k < n; k++)
        for (int i = 0; i < NUM_ARTIC_A1; i++)
            if (!(f >> *articA1(leidos[k], i))) return false;
    if (leidos.size() < 2) return false;
    keyframesA1 = leidos;
    return true;
}

// =========================================================================
// 3. FUNCIONES AUXILIARES
// =========================================================================
glm::vec3 calcularCatmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

void agregarPunto(RutaCanoa& r, const glm::vec3& p) {
    if (!r.pts.empty()) {
        float d = glm::distance(r.pts.back(), p);
        if (d < 1e-5f) return;
        r.dist.push_back(r.dist.back() + d);
    }
    else r.dist.push_back(0.0f);
    r.pts.push_back(p);
}

void muestrearCatmull(RutaCanoa& r, const std::vector<glm::vec3>& c, const glm::vec3& fantasmaIni, const glm::vec3& fantasmaFin, int muestras) {
    std::vector<glm::vec3> e; e.push_back(fantasmaIni); e.insert(e.end(), c.begin(), c.end()); e.push_back(fantasmaFin);
    for (size_t i = 1; i + 2 < e.size(); i++)
        for (int j = 0; j <= muestras; j++) agregarPunto(r, calcularCatmullRom(e[i - 1], e[i], e[i + 1], e[i + 2], (float)j / muestras));
}

void muestrearBezier(RutaCanoa& r, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, int muestras) {
    for (int j = 0; j <= muestras; j++) {
        float t = (float)j / muestras, u = 1.0f - t;
        agregarPunto(r, u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t * p3);
    }
}

void construirRutasProcesion() {
    rutasProcesion.clear();
    const glm::vec3 inicio = puntosProcesion[0];
    const glm::vec3 dirAtras = glm::normalize(puntosProcesion[0] - puntosProcesion[1]);
    const glm::vec3 colaFila = inicio + dirAtras * (ESPACIO_CANOAS * (NUM_CANOAS_PROC - 1));

    for (int k = 0; k < NUM_CANOAS_PROC; k++) {
        const PosicionAparcamiento& lugar = aparcamientosProcesion[k];
        const glm::vec3 dirFinal(1.0f, 0.0f, 0.0f);
        glm::vec3 entrada = lugar.posicion - dirFinal * LATERAL_ATRAQUE + glm::vec3(0.0f, 0.0f, -GIRO_ATRAQUE);
        const PosicionAparcamiento& primero = aparcamientosProcesion[NUM_CANOAS_PROC - 1];
        glm::vec3 inicioCarril(primero.posicion.x - LATERAL_ATRAQUE, primero.posicion.y, primero.posicion.z - GIRO_ATRAQUE - 1.0f);

        RutaCanoa r;
        for (int j = 0; j <= 20; j++) agregarPunto(r, glm::mix(colaFila, inicio, (float)j / 20.0f));
        std::vector<glm::vec3> ctrl(puntosProcesion.begin(), puntosProcesion.end() - 1);
        ctrl.push_back(inicioCarril);
        if (entrada.z > inicioCarril.z + 0.3f) ctrl.push_back(entrada); else ctrl.back() = entrada;
        muestrearCatmull(r, ctrl, inicio + dirAtras, entrada + glm::vec3(0.0f, 0.0f, 1.0f), 30);
        muestrearBezier(r, entrada, entrada + glm::vec3(0.0f, 0.0f, GIRO_ATRAQUE), lugar.posicion - dirFinal * GIRO_ATRAQUE, lugar.posicion, 30);
        rutasProcesion.push_back(r);
    }
}

void evaluarRuta(const RutaCanoa& r, float d, glm::vec3& pos, float& yaw) {
    if (d <= 0.0f) d = 0.0f; if (d >= r.dist.back()) d = r.dist.back();
    size_t j = std::upper_bound(r.dist.begin(), r.dist.end(), d) - r.dist.begin();
    if (j == 0) j = 1; if (j >= r.pts.size()) j = r.pts.size() - 1;
    float tramo = r.dist[j] - r.dist[j - 1];
    float t = tramo > 1e-6f ? (d - r.dist[j - 1]) / tramo : 0.0f;
    pos = glm::mix(r.pts[j - 1], r.pts[j], t);
    glm::vec3 dir = r.pts[j] - r.pts[j - 1];
    yaw = glm::degrees(std::atan2(dir.x, dir.z));
}

float avanceProcesion(float t) {
    if (t < ACEL_PROCESION) return VEL_PROCESION * t * t / (2.0f * ACEL_PROCESION);
    return VEL_PROCESION * (t - ACEL_PROCESION * 0.5f);
}

void generarPlanoAgua(int resolucion, float tamano, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    float paso = tamano / (float)resolucion, mitad = tamano / 2.0f;
    for (int i = 0; i <= resolucion; ++i) {
        float z = -mitad + i * paso;
        for (int j = 0; j <= resolucion; ++j) {
            vertices.push_back(-mitad + j * paso); vertices.push_back(0.0f); vertices.push_back(z);
            vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
            vertices.push_back((float)j / resolucion); vertices.push_back((float)i / resolucion);
        }
    }
    for (int i = 0; i < resolucion; ++i) {
        for (int j = 0; j < resolucion; ++j) {
            int f1 = i * (resolucion + 1), f2 = (i + 1) * (resolucion + 1);
            indices.push_back(f1 + j); indices.push_back(f2 + j); indices.push_back(f1 + j + 1);
            indices.push_back(f1 + j + 1); indices.push_back(f2 + j); indices.push_back(f2 + j + 1);
        }
    }
}

// =========================================================================
// LOGICA COMPLETA: BAILE DE LOS VIEJITOS (LUIS)
// =========================================================================
static constexpr float PI_BAILE = 3.14159265358979323846f;
enum EstadoBaile { REPOSO = 0, SALTO_IZQUIERDA = 1, SALTO_DERECHA = 2, SALTO_ADELANTE = 3, RETROCESO_1 = 4, RETROCESO_2 = 5, RETROCESO_3 = 6, RETROCESO_4 = 7, PASO_DERECHO_BASTON = 8, PASO_IZQUIERDO = 9, ZAP_DERECHO_1 = 10, ZAP_IZQUIERDO = 11, ZAP_DERECHO_2 = 12, REMATE = 13 };
static const float DURACION_ESTADO[] = { 0.0f, 0.74f, 0.74f, 0.86f, 0.40f, 0.40f, 0.40f, 0.40f, 0.66f, 0.63f, 0.26f, 0.26f, 0.26f, 0.52f };

static float limitar01(float x) { return glm::clamp(x, 0.0f, 1.0f); }
static float suavizar(float x) { x = limitar01(x); return x * x * (3.0f - 2.0f * x); }
static float alzarPie(float t) { if (t <= 0.53f) return suavizar(t / 0.53f); if (t < 0.77f) return 1.0f - suavizar((t - 0.53f) / 0.24f); return 0.0f; }
static float impacto(float t) { if (t <= 0.77f || t >= 0.97f) return 0.0f; return std::sin(PI_BAILE * (t - 0.77f) / 0.20f); }

static PoseViejito posturaBase() {
    PoseViejito p; p.espalda = 8.0f; p.espaldaMedia = 6.0f; p.espaldaAlta = 4.0f; p.cuello = -4.0f;
    p.musloIzq = -5.0f; p.musloDer = -5.0f; p.rodillaIzq = 15.0f; p.rodillaDer = 15.0f; p.pieIzq = -6.0f; p.pieDer = -6.0f;
    p.brazoIzqX = -7.0f; p.brazoIzqY = 7.0f; p.brazoIzqZ = -4.0f; p.codoIzqX = -4.0f; p.codoIzqY = 2.0f; p.codoIzqZ = -19.0f;
    p.brazoDerX = 32.0f; p.brazoDerZ = -18.0f; p.codoDerX = 10.0f; p.codoDerZ = -8.0f; p.munecaDerX = -5.0f; p.agarreDerecho = 0.72f;
    return p;
}

static PoseViejito calcularBaile(int estado, float t, float reloj, float& posX, float& posY, float& posZ, float& bastonZ, float& bastonX) {
    PoseViejito p = posturaBase();
    posX = posY = posZ = bastonZ = bastonX = 0.0f;
    t = limitar01(t);

    const float flexion = suavizar(0.5f - 0.5f * std::cos(reloj * 1.45f));
    p.espalda += 19.0f * flexion; p.espaldaMedia += 14.0f * flexion; p.espaldaAlta += 11.0f * flexion;
    p.cuello -= 7.0f * flexion; p.caderaX += 2.0f * flexion;
    p.rodillaIzq += 3.5f * flexion; p.rodillaDer += 3.5f * flexion;
    p.cabezaY = 15.0f * std::sin(reloj * 0.92f) + 3.0f * std::sin(reloj * 1.70f + 0.25f);
    p.cuello += 2.0f * std::sin(reloj * 0.85f);
    p.espaldaLateral += 0.55f * std::sin(reloj * 1.40f); p.espaldaAlta += 1.0f * std::sin(reloj * 1.10f);
    p.rodillaIzq += 1.25f * std::sin(reloj * 22.0f); p.rodillaDer += 1.25f * std::sin(reloj * 24.0f + 1.3f);
    p.brazoDerX += 3.5f * std::sin(reloj * 1.8f); p.brazoDerZ -= 2.0f * std::sin(reloj * 1.35f + 0.5f);
    p.codoDerX += 7.0f * std::sin(reloj * 2.6f + 0.3f); p.munecaDerX += 4.0f * std::sin(reloj * 2.2f);
    p.munecaDerZ += 3.0f * std::sin(reloj * 1.9f + 0.4f); p.agarreDerecho = 0.90f + 0.06f * std::sin(reloj * 2.7f);

    float haciaLado = 0.0f;
    if (estado == SALTO_IZQUIERDA) haciaLado = 14.0f;
    else if (estado == SALTO_DERECHA) haciaLado = -14.0f;
    else if (estado == SALTO_ADELANTE) haciaLado = 6.0f;
    else if (estado >= RETROCESO_1 && estado <= RETROCESO_4) haciaLado = ((estado - RETROCESO_1) % 2 == 0) ? 7.0f : -7.0f;
    else if (estado == PASO_DERECHO_BASTON) haciaLado = -12.0f;
    else if (estado == PASO_IZQUIERDO) haciaLado = 10.0f;
    else if (estado == REMATE) haciaLado = -5.0f;
    p.cabezaY += haciaLado * std::sin(PI_BAILE * t); p.cabezaY = glm::clamp(p.cabezaY, -25.0f, 25.0f);

    if (estado == REPOSO) return p;

    if (estado == SALTO_IZQUIERDA || estado == SALTO_DERECHA || estado == SALTO_ADELANTE) {
        float avance = suavizar((t - 0.16f) / 0.68f); float vuelo = std::pow(std::max(0.0f, std::sin(PI_BAILE * t)), 1.2f);
        float prepara = 1.0f - suavizar(t / 0.16f); float aterriza = suavizar((t - 0.75f) / 0.11f) * (1.0f - suavizar((t - 0.87f) / 0.13f));
        posY = (estado == SALTO_ADELANTE ? 0.115f : 0.095f) * vuelo;
        p.rodillaIzq += 8.0f * prepara + 12.0f * aterriza + 7.0f * vuelo; p.rodillaDer += 8.0f * prepara + 12.0f * aterriza + 7.0f * vuelo;
        p.musloIzq -= 7.0f * vuelo; p.musloDer -= 7.0f * vuelo;
        p.pieIzq += 4.0f * vuelo; p.pieDer += 4.0f * vuelo;
        p.brazoDerX -= 8.0f * vuelo; p.brazoDerZ -= 3.0f * vuelo;
        p.codoDerX += 16.0f * vuelo + 5.0f * aterriza; p.munecaDerX -= 5.0f * vuelo;
        if (estado == SALTO_IZQUIERDA) { posX = -0.22f * avance; p.musloIzqZ += 4.0f * vuelo; bastonZ = 5.0f * vuelo; }
        else if (estado == SALTO_DERECHA) { posX = -0.22f + 0.22f * avance; p.musloDerZ -= 4.0f * vuelo; bastonZ = -5.0f * vuelo; }
        else {
            posZ = 0.18f * avance; float golpeFinal = suavizar((t - 0.72f) / 0.10f) * (1.0f - suavizar((t - 0.85f) / 0.15f));
            p.rodillaIzq += 7.0f * golpeFinal; p.rodillaDer += 7.0f * golpeFinal; bastonX = 12.0f * vuelo - 9.0f * golpeFinal;
        }
    }
    else if (estado >= RETROCESO_1 && estado <= RETROCESO_4) {
        int numero = estado - RETROCESO_1; float avance = suavizar(t); float levanta = std::sin(PI_BAILE * t);
        posZ = 0.18f - 0.045f * (numero + avance); bool derecho = (numero % 2 == 0);
        if (derecho) { p.musloDer += 19.0f * levanta; p.rodillaDer += 25.0f * levanta; p.pieDer -= 10.0f * levanta; }
        else { p.musloIzq += 19.0f * levanta; p.rodillaIzq += 25.0f * levanta; p.pieIzq -= 10.0f * levanta; }
        p.espaldaLateral += (derecho ? 2.2f : -2.2f) * levanta; p.caderaZ += (derecho ? 1.2f : -1.2f) * levanta;
        p.codoDerX += 7.0f * levanta; p.brazoDerZ -= 2.5f * levanta;
        bastonZ = (derecho ? -6.0f : 6.0f) * levanta; posY = 0.008f * levanta;
    }
    else if (estado == PASO_DERECHO_BASTON) {
        float levanta = alzarPie(t); float pisoton = impacto(t);
        p.musloDer -= 67.0f * levanta; p.rodillaDer += 73.0f * levanta + 9.0f * pisoton; p.pieDer -= 22.0f * levanta; p.rodillaIzq += 5.0f * levanta;
        posY = 0.012f * levanta - 0.012f * pisoton;
        p.brazoDerX -= 9.0f * levanta; p.brazoDerZ -= 12.0f * levanta; p.codoDerX += 18.0f * levanta;
        p.munecaDerX -= 6.0f * levanta; p.munecaDerZ += 5.0f * levanta;
        bastonZ = 30.0f * levanta - 12.0f * pisoton; bastonX = -15.0f * levanta + 8.0f * pisoton;
    }
    else if (estado == PASO_IZQUIERDO) {
        float levanta = alzarPie(t); float pisoton = impacto(t);
        p.musloIzq -= 67.0f * levanta; p.rodillaIzq += 73.0f * levanta + 9.0f * pisoton; p.pieIzq -= 22.0f * levanta; p.rodillaDer += 5.0f * levanta;
        posY = 0.012f * levanta - 0.012f * pisoton; bastonZ = -8.0f * levanta + 4.0f * pisoton;
    }
    else if (estado == ZAP_DERECHO_1 || estado == ZAP_IZQUIERDO || estado == ZAP_DERECHO_2) {
        float levanta = alzarPie(t); float pisoton = impacto(t); bool derecho = (estado != ZAP_IZQUIERDO);
        if (derecho) { p.musloDer -= 38.0f * levanta; p.rodillaDer += 42.0f * levanta + 5.0f * pisoton; p.pieDer -= 12.0f * levanta; }
        else { p.musloIzq -= 38.0f * levanta; p.rodillaIzq += 42.0f * levanta + 5.0f * pisoton; p.pieIzq -= 12.0f * levanta; }
        posY = 0.005f * levanta - 0.009f * pisoton; bastonZ = (derecho ? 9.0f : -9.0f) * levanta;
    }
    else if (estado == REMATE) {
        float levanta = alzarPie(t); float pisoton = impacto(t);
        p.musloIzq -= 14.0f * levanta; p.musloDer -= 14.0f * levanta; p.rodillaIzq += 15.0f * levanta + 12.0f * pisoton; p.rodillaDer += 15.0f * levanta + 12.0f * pisoton;
        p.pieIzq += 13.0f * levanta; p.pieDer += 13.0f * levanta;
        posY = 0.022f * levanta - 0.018f * pisoton; p.espalda += 3.0f * pisoton; p.codoDerX += 9.0f * levanta + 4.0f * pisoton; p.munecaDerX -= 5.0f * levanta;
        bastonZ = 17.0f * levanta - 14.0f * pisoton; bastonX = -9.0f * levanta + 7.0f * pisoton;
    }
    return p;
}

// =========================================================================
// 4. ENTRADA, CONTROL Y TOPOGRAFIA
// =========================================================================
void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn), ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos; lastY = ypos;
}

void fijarCamara(const glm::vec3& pos, float pitch, float yaw) {
    camera.Position = pos; camera.Pitch = pitch; camera.Yaw = yaw; camera.ProcessMouseMovement(0.0f, 0.0f);
}

struct TrianguloTerreno { glm::vec3 a, b, c; float minX, maxX, minZ, maxZ; };
std::vector<TrianguloTerreno> mallaTerreno;

void construirColisionadorTerreno(Model& mapa) {
    mallaTerreno.clear();
    glm::mat4 M = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.2f, 0.0f)), glm::vec3(0.2f));
    for (const auto& mesh : mapa.meshes) {
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            glm::vec3 v0 = glm::vec3(M * glm::vec4(mesh.vertices[mesh.indices[i]].Position, 1.0f));
            glm::vec3 v1 = glm::vec3(M * glm::vec4(mesh.vertices[mesh.indices[i + 1]].Position, 1.0f));
            glm::vec3 v2 = glm::vec3(M * glm::vec4(mesh.vertices[mesh.indices[i + 2]].Position, 1.0f));
            if (std::min({ v0.y, v1.y, v2.y }) > 0.90f || std::max({ v0.y, v1.y, v2.y }) < -0.85f) continue;
            mallaTerreno.push_back({ v0, v1, v2, std::min({v0.x, v1.x, v2.x}), std::max({v0.x, v1.x, v2.x}), std::min({v0.z, v1.z, v2.z}), std::max({v0.z, v1.z, v2.z}) });
        }
    }
}

float obtenerAlturaTerreno(float x, float z, float alturaActual = -0.20f) {
    float mejorY = -100.0f; bool encontrado = false;
    for (const auto& t : mallaTerreno) {
        if (x < t.minX || x > t.maxX || z < t.minZ || z > t.maxZ) continue;
        float det = (t.b.z - t.c.z) * (t.a.x - t.c.x) + (t.c.x - t.b.x) * (t.a.z - t.c.z);
        if (std::abs(det) < 1e-6f) continue;
        float l1 = ((t.b.z - t.c.z) * (x - t.c.x) + (t.c.x - t.b.x) * (z - t.c.z)) / det;
        float l2 = ((t.c.z - t.a.z) * (x - t.c.x) + (t.a.x - t.c.x) * (z - t.c.z)) / det;
        if (l1 >= -0.05f && l2 >= -0.05f && (1.0f - l1 - l2) >= -0.05f) {
            float y = l1 * t.a.y + l2 * t.b.y + (1.0f - l1 - l2) * t.c.y;
            if (y > mejorY) { mejorY = y; encontrado = true; }
        }
    }
    if (encontrado) return mejorY;
    if (z >= 10.0f) return -0.10f; if (z <= -5.0f) return -0.15f;
    return std::max(alturaActual, -0.45f);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    // C alterna caminata y camara libre; no interfiere con la edicion de la fisga.
    if (!modoEdicionFisga && teclaPulsada(window, GLFW_KEY_C)) {
        modoDron = false;
        modoCaminataPOV = !modoCaminataPOV;
        std::cout << (modoCaminataPOV
            ? ">> CAMARA: CAMINATA POV (ajuste al terreno y efecto de pasos)\n"
            : ">> CAMARA: LIBRE (sin efecto de pasos) | Q bajar, R subir\n");
    }

    if (!modoEdicionFisga) {
        // WASD conserva el modo seleccionado: ya no reactiva la caminata.
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);

        // Movimiento vertical solo en camara libre (y fuera del dron).
        if (!modoCaminataPOV && !modoDron) {
            const float velocidadVertical = 2.5f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) camera.Position.y += velocidadVertical;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camera.Position.y -= velocidadVertical;
        }
    }
    else {
        float vel = 1.5f * deltaTime, velRot = 45.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) posBoteFisga.z -= vel;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) posBoteFisga.z += vel;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) posBoteFisga.x -= vel;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) posBoteFisga.x += vel;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) posBoteFisga.y += vel;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) posBoteFisga.y -= vel;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) yawBoteFisga += velRot;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yawBoteFisga -= velRot;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) escalaFisgaGlobal += vel * 0.5f;
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) escalaFisgaGlobal = std::max(0.05f, escalaFisgaGlobal - vel * 0.5f);
    }

    static bool enterPresionado = false;
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        if (!enterPresionado) {
            enterPresionado = true; modoEdicionFisga = !modoEdicionFisga;
            if (modoEdicionFisga) {
                std::cout << "\n>> MODO EDICION FISGA: ACTIVADO [W/A/S/D] Mover | [UP/DOWN] Altura | [LEFT/RIGHT] Rotar | [U/I] Escalar\n";
            }
            else {
                std::cout << "\n>> MODO EDICION FISGA: DESACTIVADO\n";
                std::cout << "posBoteFisga = glm::vec3(" << posBoteFisga.x << "f, " << posBoteFisga.y << "f, " << posBoteFisga.z << "f);\n";
                std::cout << "yawBoteFisga = " << yawBoteFisga << "f;\n";
                std::cout << "escalaFisgaGlobal = " << escalaFisgaGlobal << "f;\n\n";
            }
        }
    }
    else enterPresionado = false;

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        if (!eKeyPressed) {
            eKeyPressed = true;
            for (int i = 0; i < (int)estaciones.size(); i++) {
                if (estaciones[i].interactuable) { estacionActivaHUD = (estacionActivaHUD == i) ? -1 : i; break; }
            }
        }
    }
    else eKeyPressed = false;

    if (teclaPulsada(window, GLFW_KEY_6) || teclaPulsada(window, GLFW_KEY_SPACE)) { animacionProcesionActiva = true; tiempoProcesion = 0.0f; }
    if (teclaPulsada(window, GLFW_KEY_7)) { modoDron = false; modoCaminataPOV = false; fijarCamara(glm::vec3(0.0f, 6.5f, 5.0f), -35.0f, -90.0f); }
    if (teclaPulsada(window, GLFW_KEY_8)) { modoDron = false; modoCaminataPOV = true; fijarCamara(glm::vec3(4.5f, obtenerAlturaTerreno(4.5f, -12.5f) + 0.25f, -12.5f), -5.0f, -135.0f); }
    if (teclaPulsada(window, GLFW_KEY_9)) { modoDron = false; modoCaminataPOV = false; fijarCamara(glm::vec3(2.8f, 0.2f, 14.5f), -5.0f, 40.0f); }
    if (teclaPulsada(window, GLFW_KEY_0)) { modoDron = false; modoCaminataPOV = false; fijarCamara(glm::vec3(-0.6f, -0.2f, 10.7f), -8.0f, -170.0f); }
    if (teclaPulsada(window, GLFW_KEY_P)) { modoDron = false; modoCaminataPOV = false; fijarCamara(glm::vec3(1.02f, -0.35f, -0.2f), -10.0f, -90.0f); }
    if (teclaPulsada(window, GLFW_KEY_5)) modoDron = !modoDron;
    if (teclaPulsada(window, GLFW_KEY_K)) UrandenFisga::IniciarLanzamiento();
    if (teclaPulsada(window, GLFW_KEY_J)) baileViejitosActivo = !baileViejitosActivo;

    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) factorEscalaBaston = std::max(0.1f, factorEscalaBaston - 0.5f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) factorEscalaBaston += 0.5f * deltaTime;
}

void showFPS(GLFWwindow* window) {
    double currentTime = glfwGetTime(); nbFrames++;
    if (currentTime - lastFpsTime >= 1.0) {
        glfwSetWindowTitle(window, ("El Camino de las Animas | FPS: " + std::to_string((int)(nbFrames / (currentTime - lastFpsTime)))).c_str());
        nbFrames = 0; lastFpsTime = currentTime;
    }
}

// =========================================================================
// 5. FUNCIÓN PRINCIPAL
// =========================================================================
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "El Camino de las Animas", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
#ifdef _WIN32
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) SetWindowPos(consoleWindow, NULL, 0, 0, 600, 600, SWP_NOZORDER);
#endif
    glfwSetWindowPos(window, 620, 50);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;
    glEnable(GL_DEPTH_TEST);

    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Shader skyboxShader("shader/skybox.vert", "shader/skybox.frag");
    Shader waterShader("shader/water.vert", "shader/water.frag");

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO); glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO); glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); glBindVertexArray(0);

    GLuint cubemapTexture = SOIL_load_OGL_cubemap("skybox/xpos.png", "skybox/xneg.png", "skybox/ypos.png", "skybox/yneg.png", "skybox/zpos.png", "skybox/zneg.png", SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
    skyboxShader.use(); glUniform1i(ubicacionUniform(skyboxShader.ID, "skybox"), 0);

    std::vector<float> waterVertices; std::vector<unsigned int> waterIndices;
    generarPlanoAgua(100, 100.0f, waterVertices, waterIndices);

    unsigned int waterVAO, waterVBO, waterEBO;
    glGenVertexArrays(1, &waterVAO); glGenBuffers(1, &waterVBO); glGenBuffers(1, &waterEBO);
    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO); glBufferData(GL_ARRAY_BUFFER, waterVertices.size() * sizeof(float), waterVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, waterIndices.size() * sizeof(unsigned int), waterIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    Model modeloCanoa("Models/canoa.obj");
    Model modeloCanoaPro("Models/canoaPro.obj");
    Model modeloCopalera("Models/copalera.gltf");
    Model bastonViejito("Models/Baston.obj");

    Model modeloBote("Models/Bote.obj");
    Model modeloFisga("Models/Fisga.obj");
    PescadorFisgaRigGPU pescadorFisga("Models/PescadorFisgaRig.bin", "Models/Pescador_Textura.png");
    UrandenFisga::precargarAnimacion();

    construirRutasProcesion();
    Model modeloMapaCompleto("Models/MapaCompleto/Rendido.gltf");
    construirColisionadorTerreno(modeloMapaCompleto);

    Model mCuerpo("Models/mariposero/cuerpo.obj");
    Model mBrazoDer("Models/mariposero/brazoDer.obj"); Model mBrazoIzq("Models/mariposero/brazoIzq.obj");
    Model mAntebrazoDer("Models/mariposero/antebrazoDer.obj"); Model mAntebrazoIzq("Models/mariposero/antebrazoIzq.obj");
    Model mMusloDer("Models/mariposero/musloDer.obj"); Model mMusloIzq("Models/mariposero/musloIzq.obj");
    Model mPantorrillaDer("Models/mariposero/pantorrillaDer.obj"); Model mPantorrillaIzq("Models/mariposero/pantorrillaIzq.obj");
    Model mMango("Models/mariposero/mango.obj"); Model mAlaIzq("Models/mariposero/alaIzq.obj"); Model mAlaDer("Models/mariposero/alaDer.obj");

    ViejitoRig viejito1("Models/ViejitoRig.bin", "Models/Textura_Viejito.png");
    ViejitoRig viejito2("Models/ViejitoRig.bin", "Models/Textura_Viejito.png");
    viejito1.animarPose(posturaBase()); viejito2.animarPose(posturaBase());

    int estadoBaile1 = SALTO_IZQUIERDA, estadoBaile2 = PASO_IZQUIERDO;
    float tiempoEstado1 = 0.0f, tiempoEstado2 = 0.0f, ritmoViejitos = 1.0f, relojCuerpo1 = 0.0f, relojCuerpo2 = 1.5f;
    int vueltasBaile = 0;

    if (cargarKeyframesA1(ARCHIVO_KF_A1)) std::cout << "A1: keyframes cargados.\n";

    auto dibujar = [&](Model& m, const glm::mat4& M) {
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(M));
        m.Draw(miShader);
        };

    auto dibujarPurepecha = [&](const glm::mat4& matBaseCanoa, float xOffset, float yawRotacion, bool invertirLado) {
        float aC = glm::radians(poseA1.cadera), aR = glm::radians(poseA1.rodilla);
        float bajada = (L_MUSLO + L_PIERNA) - (L_MUSLO * std::cos(aC) + L_PIERNA * std::cos(aC - aR));
        const glm::vec3 EJE_X(1, 0, 0), EJE_Y(0, 1, 0), EJE_Z(0, 0, 1);

        float yLocal = pisoCanoaA1 + PIES_A_ORIGEN * escalaA1;
        glm::mat4 base = glm::translate(matBaseCanoa, glm::vec3(xOffset, yLocal, 0.0f));
        base = glm::rotate(base, glm::radians(yawRotacion), EJE_Y);
        base = glm::scale(base, glm::vec3(escalaA1));
        if (invertirLado) base = glm::scale(base, glm::vec3(-1.0f, 1.0f, 1.0f));

        glm::mat4 pelvis = glm::translate(base, glm::vec3(0.0f, -bajada, 0.0f));
        glm::mat4 torso = rotarEnPivote(pelvis, P_PELVIS, poseA1.torso, EJE_X);
        dibujar(mCuerpo, torso);

        glm::mat4 brazoExt = rotarEnPivote(rotarEnPivote(torso, P_HOMBRO_IZQ, -poseA1.hombroExtFrente, EJE_X), P_HOMBRO_IZQ, -poseA1.hombroExtLado, EJE_Z);
        glm::mat4 antebrazoExt = rotarEnPivote(brazoExt, P_CODO_IZQ, -poseA1.codoExt, EJE_X);
        dibujar(mBrazoIzq, brazoExt);
        dibujar(mAntebrazoIzq, antebrazoExt);

        glm::mat4 brazoInt = rotarEnPivote(rotarEnPivote(torso, P_HOMBRO_DER, -poseA1.hombroIntFrente, EJE_X), P_HOMBRO_DER, poseA1.hombroIntLado, EJE_Z);
        glm::mat4 antebrazoInt = rotarEnPivote(brazoInt, P_CODO_DER, -poseA1.codoInt, EJE_X);
        dibujar(mBrazoDer, brazoInt);
        dibujar(mAntebrazoDer, antebrazoInt);

        glm::mat4 musloD = rotarEnPivote(pelvis, P_CADERA_DER, -poseA1.cadera, EJE_X);
        glm::mat4 musloI = rotarEnPivote(pelvis, P_CADERA_IZQ, -poseA1.cadera, EJE_X);
        dibujar(mMusloDer, musloD);
        dibujar(mMusloIzq, musloI);
        dibujar(mPantorrillaDer, rotarEnPivote(musloD, P_RODILLA_DER, poseA1.rodilla, EJE_X));
        dibujar(mPantorrillaIzq, rotarEnPivote(musloI, P_RODILLA_IZQ, poseA1.rodilla, EJE_X));

        glm::mat4 mango = glm::translate(antebrazoExt, P_MANO_IZQ);
        mango = glm::rotate(mango, glm::radians(-poseA1.redLado), EJE_Z);
        mango = glm::rotate(mango, glm::radians(poseA1.redInclinacion), EJE_X);
        mango = glm::translate(mango, -P_AGARRE);
        dibujar(mMango, mango);

        dibujar(mAlaIzq, rotarEnPivote(mango, P_EJE_ALAS, poseA1.redApertura, EJE_Y));
        dibujar(mAlaDer, rotarEnPivote(mango, P_EJE_ALAS, -poseA1.redApertura, EJE_Y));
        };

    AudioEngine audioEngine;
    if (!audioEngine.init()) std::cerr << "Audio error\n";
    audioEngine.playMusic("audio/Ofrenda_en_el_Agua.mp3", 0.35f, true);
    audioEngine.playAmbient("audio/ambiente.mp3", 0.25f, true);

    Shader hudShader("shader/hud.vs", "shader/hud.fs");
    unsigned int hudVAO, hudVBO;
    float hudVertices[] = { 0,1,0,1, 1,0,1,0, 0,0,0,0, 0,1,0,1, 1,1,1,1, 1,0,1,0 };
    glGenVertexArrays(1, &hudVAO); glGenBuffers(1, &hudVBO);
    glBindVertexArray(hudVAO); glBindBuffer(GL_ARRAY_BUFFER, hudVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(hudVertices), hudVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); glEnableVertexAttribArray(0); glBindVertexArray(0);

    unsigned int texCedula1 = SOIL_load_OGL_texture("Models/textures/1_Las-copaleras-y-el-rito-de-purificacion.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula2 = SOIL_load_OGL_texture("Models/textures/2_Danzas-y-cocina-tradicional.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula3 = SOIL_load_OGL_texture("Models/textures/3_Procesion-de-Canoas.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula4 = SOIL_load_OGL_texture("Models/textures/4_El-rescate-de-los-manantiales.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glm::vec3 posCamaraAnterior = camera.Position;
        processInput(window);

        static float fasePaso = 0.0f, alturaPiesSuavizada = -0.20f;
        glm::vec3 offsetVisualOjos(0.0f);
        if (!modoDron && modoCaminataPOV) {
            bool seEstaMoviendo = (glm::length(glm::vec2(camera.Position.x - posCamaraAnterior.x, camera.Position.z - posCamaraAnterior.z)) > 0.0001f);
            float sueloReal = std::max(obtenerAlturaTerreno(camera.Position.x, camera.Position.z, alturaPiesSuavizada), -0.55f);
            alturaPiesSuavizada = glm::mix(alturaPiesSuavizada, sueloReal, glm::clamp(deltaTime * 14.0f, 0.0f, 1.0f));
            camera.Position.y = std::max(alturaPiesSuavizada + 0.28f, sueloReal + 0.18f);
            if (seEstaMoviendo) {
                fasePaso += deltaTime * 7.5f;
                offsetVisualOjos = glm::vec3(0.0f, std::sin(fasePaso * 2.0f) * 0.022f, 0.0f) + (camera.Right * (std::cos(fasePaso) * 0.015f));
            }
            else fasePaso = 0.0f;
        }

        bool cercaDeAlguna = false;
        for (int i = 0; i < (int)estaciones.size(); i++) {
            estaciones[i].interactuable = (glm::distance(glm::vec2(camera.Position.x, camera.Position.z), glm::vec2(estaciones[i].posicion.x, estaciones[i].posicion.z)) <= estaciones[i].radioDeteccion);
            if (estaciones[i].interactuable) cercaDeAlguna = true;
        }
        static bool mensajeMostrado = false;
        if (cercaDeAlguna && estacionActivaHUD == -1) {
            if (!mensajeMostrado) { std::cout << "\n>> [ESTACION CERCANA] Presiona 'E' para leer." << std::endl; mensajeMostrado = true; }
        }
        else mensajeMostrado = false;

        if (modoDron) {
            camera.Position = glm::vec3(0.0f, 0.0f, 6.0f) + glm::vec3(std::sin(currentFrame * 0.12f) * 16.0f, 7.5f + 1.2f * std::sin(currentFrame * 0.4f), std::cos(currentFrame * 0.12f) * 16.0f);
            glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 0.0f, 6.0f) - camera.Position);
            camera.Pitch = glm::degrees(std::asin(dir.y)); camera.Yaw = glm::degrees(std::atan2(dir.z, dir.x));
            camera.ProcessMouseMovement(0.0f, 0.0f);
        }

        if (animacionProcesionActiva) tiempoProcesion += deltaTime;
        float avance = animacionProcesionActiva ? avanceProcesion(tiempoProcesion) : 0.0f;
        for (int k = 0; k < NUM_CANOAS_PROC; k++) {
            float d = ESPACIO_CANOAS * (NUM_CANOAS_PROC - 1 - k) + avance;
            float fA = rutasProcesion[k].dist.back() - FRENADO_PROCESION;
            if (d > fA) d = (d - fA >= 2.0f * FRENADO_PROCESION) ? rutasProcesion[k].dist.back() : fA + (d - fA) - std::pow(d - fA, 2) / (4.0f * FRENADO_PROCESION);
            evaluarRuta(rutasProcesion[k], d, posProc[k], yawProc[k]);
        }

        float deltaModificado = baileViejitosActivo ? (deltaTime * velAnimViejitos) : 0.0f;
        relojCuerpo1 += deltaModificado * ritmoViejitos; relojCuerpo2 += deltaModificado * ritmoViejitos;
        tiempoEstado1 += deltaModificado * ritmoViejitos; tiempoEstado2 += deltaModificado * ritmoViejitos;
        if (tiempoEstado1 >= DURACION_ESTADO[estadoBaile1]) {
            tiempoEstado1 -= DURACION_ESTADO[estadoBaile1];
            if (estadoBaile1 == REMATE) { estadoBaile1 = SALTO_IZQUIERDA; ritmoViejitos = std::min(1.40f, 1.0f + 0.05f * (++vueltasBaile)); }
            else ++estadoBaile1;
        }
        if (tiempoEstado2 >= DURACION_ESTADO[estadoBaile2]) {
            tiempoEstado2 -= DURACION_ESTADO[estadoBaile2];
            estadoBaile2 = (estadoBaile2 == REMATE) ? SALTO_IZQUIERDA : estadoBaile2 + 1;
        }

        float posX1 = 0, posY1 = 0, posZ1 = 0, bastonZ1 = 0, bastonX1 = 0;
        viejito1.animarPose(calcularBaile(estadoBaile1, tiempoEstado1 / DURACION_ESTADO[estadoBaile1], relojCuerpo1, posX1, posY1, posZ1, bastonZ1, bastonX1));
        float posX2 = 0, posY2 = 0, posZ2 = 0, bastonZ2 = 0, bastonX2 = 0;
        viejito2.animarPose(calcularBaile(estadoBaile2, tiempoEstado2 / DURACION_ESTADO[estadoBaile2], relojCuerpo2, posX2, posY2, posZ2, bastonZ2, bastonX2));

        glm::vec3 fogColor(0.012f, 0.018f, 0.035f); float fogDensity = 0.012f;
        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 posFisicaGuardada = camera.Position; camera.Position += offsetVisualOjos;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        miShader.use();
        glUniform3fv(ubicacionUniform(miShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3f(ubicacionUniform(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3fv(ubicacionUniform(miShader.ID, "dirLight.ambient"), 1, glm::value_ptr(glm::vec3(0.04f, 0.05f, 0.08f) * intensidadGlobal));
        glUniform3fv(ubicacionUniform(miShader.ID, "dirLight.diffuse"), 1, glm::value_ptr(glm::vec3(0.18f, 0.20f, 0.25f) * intensidadGlobal));
        glUniform3f(ubicacionUniform(miShader.ID, "dirLight.specular"), 0.15f, 0.15f, 0.20f);

        auto setLight = [&](int idx, glm::vec3 pos, glm::vec3 diff, float l, float q) {
            std::string b = "pointLights[" + std::to_string(idx) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (b + "position").c_str()), 1, glm::value_ptr(pos));
            glUniform3f(ubicacionUniform(miShader.ID, (b + "ambient").c_str()), diff.x * 0.02f, diff.y * 0.02f, diff.z * 0.02f);
            glUniform3fv(ubicacionUniform(miShader.ID, (b + "diffuse").c_str()), 1, glm::value_ptr(diff));
            glUniform3f(ubicacionUniform(miShader.ID, (b + "specular").c_str()), diff.x * 0.4f, diff.y * 0.4f, diff.z * 0.4f);
            glUniform1f(ubicacionUniform(miShader.ID, (b + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (b + "linear").c_str()), l);
            glUniform1f(ubicacionUniform(miShader.ID, (b + "quadratic").c_str()), q);
            };

        for (int i = 0; i < 4; i++) setLight(i, pointLightOffsets[i] + offsetMuelle, glm::vec3(0), 0.09f, 0.032f);
        setLight(0, posCanoaA1 + glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(yawCanoaA1), glm::vec3(0, 1, 0)) * glm::vec4(0, 0.9f, 0.8f, 1)), glm::vec3(0.95f, 0.90f, 0.80f), 0.35f, 0.45f);
        for (int i = 0; i < 5; i++) setLight(5 + i, postesPlaza[i], glm::vec3(0.70f, 0.68f, 0.62f), 0.60f, 1.40f);
        for (int i = 0; i < 6; i++) setLight(10 + i, lucesOrilla[i], glm::vec3(1.20f, 0.75f, 0.28f), 0.32f, 0.38f);
        for (int i = 0; i < 2; i++) setLight(16 + i, lucesLetrero[i], glm::vec3(1.10f, 0.70f, 0.25f), 0.22f, 0.20f);
        for (int i = 0; i < 2; i++) setLight(18 + i, (glm::mix(postesPuente[0], postesPuente[1], i ? 0.65f : 0.35f) + glm::mix(postesPuente[2], postesPuente[3], i ? 0.65f : 0.35f)) * 0.5f + glm::vec3(0, 0.08f, 0), glm::vec3(0.70f, 0.32f, 0.05f), 0.70f, 1.80f);
        for (int i = 0; i < 4; i++) setLight(20 + i, lucesManantialOpuesto[i], glm::vec3(1.25f, 0.78f, 0.28f), 0.32f, 0.38f);
        for (int i = 0; i < 4; i++) setLight(40 + i, focosUranden[i].pos, glm::vec3(1.20f, 0.65f, 0.20f), 0.22f, 0.20f);
        for (int i = 0; i < focosLetras.size(); i++) setLight(44 + i, focosLetras[i].pos, glm::vec3(1.20f, 0.65f, 0.20f), 0.22f, 0.20f);
        for (int i = 0; i < 2; i++) setLight(48 + i, (glm::mix(postesPuente2[0], postesPuente2[1], i ? 0.65f : 0.35f) + glm::mix(postesPuente2[2], postesPuente2[3], i ? 0.65f : 0.35f)) * 0.5f + glm::vec3(0, 0.08f, 0), glm::vec3(0.70f, 0.32f, 0.05f), 0.70f, 1.80f);

        glUniform3fv(ubicacionUniform(miShader.ID, "spotLight.position"), 1, glm::value_ptr(glm::vec3(0.0f, 3.5f, 1.0f)));
        glUniform3fv(ubicacionUniform(miShader.ID, "spotLight.direction"), 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f))));
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.cutOff"), glm::cos(glm::radians(18.0f)));
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.outerCutOff"), glm::cos(glm::radians(24.0f)));
        glUniform3f(ubicacionUniform(miShader.ID, "spotLight.diffuse"), 0.75f, 0.75f, 0.80f);
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.constant"), 1.0f);
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.linear"), 0.14f);
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.quadratic"), 0.07f);

        glUniform3fv(ubicacionUniform(miShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(ubicacionUniform(miShader.ID, "fogDensity"), fogDensity);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform1i(ubicacionUniform(miShader.ID, "useTexture"), true);
        glUniform1i(ubicacionUniform(miShader.ID, "isLightSource"), false);
        glUniform1f(ubicacionUniform(miShader.ID, "materialAlpha"), 1.0f);

        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.2f, 0.0f)), glm::vec3(0.2f))));
        modeloMapaCompleto.Draw(miShader);

        // --- 1. PROCESIÓN DE CANOAS Y COPALERAS ---
        for (int k = 0; k < NUM_CANOAS_PROC; k++) {
            glm::vec3 posK = posProc[k]; posK.y += std::sin(currentFrame * 1.2f + (k * 0.7f)) * 0.008f + alturaCanoasProcesion;
            glm::mat4 canoaMat = glm::scale(glm::rotate(glm::rotate(glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), posK), glm::radians(yawProc[k]), glm::vec3(0, 1, 0)), glm::radians(90.0f), glm::vec3(0, 1, 0)), glm::radians(std::sin(currentFrame + k * 0.5f) * 1.2f), glm::vec3(0, 0, 1)), glm::radians(std::cos(currentFrame + k * 0.5f) * 0.6f), glm::vec3(1, 0, 0)), glm::vec3(escalaCanoaProcesion));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaMat));
            modeloCanoaPro.Draw(miShader);

            glm::mat4 copaleraMat = glm::scale(glm::rotate(glm::translate(canoaMat, offsetCopalera), glm::radians(-90.0f), glm::vec3(0, 1, 0)), glm::vec3(escalaCopalera));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(copaleraMat));
            modeloCopalera.Draw(miShader);

            setLight(25 + k, glm::vec3(copaleraMat * glm::vec4(offsetFuegoSahumador, 1.0f)), glm::vec3(1.0f, 0.30f, 0.05f) * (0.6f + 0.4f * std::sin(currentFrame * 28.0f + k * 7.0f)), 2.5f, 4.5f);
            setLight(30 + (k * 2), glm::vec3(canoaMat * glm::vec4(offsetLuzProa, 1.0f)), glm::vec3(0.9f, 0.6f, 0.2f), 1.2f, 1.8f);
            setLight(31 + (k * 2), glm::vec3(canoaMat * glm::vec4(offsetLuzPopa, 1.0f)), glm::vec3(0.9f, 0.6f, 0.2f), 1.2f, 1.8f);
        }

        // --- 2. CANOA Y PESCADORES MARIPOSEROS (A1) ---
        glm::mat4 canoaA1 = glm::rotate(glm::translate(glm::mat4(1.0f), posCanoaA1), glm::radians(yawCanoaA1), glm::vec3(0.0f, 1.0f, 0.0f));
        dibujar(modeloCanoa, glm::scale(canoaA1, glm::vec3(escalaCanoaA1)));

        if (reproduciendoA1 && keyframesA1.size() >= 2) {
            tiempoAnimA1 += deltaTime;
            if (tiempoAnimA1 >= (keyframesA1.size() - 1) * DURACION_TRAMO_A1) tiempoAnimA1 = std::fmod(tiempoAnimA1, (keyframesA1.size() - 1) * DURACION_TRAMO_A1);
            int k = std::min((int)(tiempoAnimA1 / DURACION_TRAMO_A1), (int)keyframesA1.size() - 2);
            poseA1 = interpolarPose(keyframesA1[k], keyframesA1[k + 1], (tiempoAnimA1 - k * DURACION_TRAMO_A1) / DURACION_TRAMO_A1);
        }
        float sepA1 = SEPARACION_PESCADOR * escalaCanoaA1;
        dibujarPurepecha(canoaA1, sepA1, 0.0f, false);
        dibujarPurepecha(canoaA1, -sepA1, 0.0f, true);

        // --- 3. PESCADOR LANZANDO LA FISGA ---
        UrandenFisga::Animation(deltaTime);
        pescadorFisga.animar(UrandenFisga::actual.hombroX, UrandenFisga::actual.hombroZ, UrandenFisga::actual.codoX, UrandenFisga::actual.munecaX, UrandenFisga::actual.pechoX, UrandenFisga::actual.rodillas);

        glm::mat4 matBaseBote = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), posBoteFisga), glm::radians(yawBoteFisga), glm::vec3(0, 1, 0)), glm::vec3(escalaFisgaGlobal));

        miShader.use();
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::rotate(matBaseBote, glm::radians(UrandenFisga::actual.boteBalanceo), glm::vec3(0, 0, 1)), glm::vec3(1.85f))));
        modeloBote.Draw(miShader);

        // Escala interna del pescador en 1.0f para que mida 0.4f netos (escalaFisgaGlobal)
        glm::mat4 pescadorM = glm::scale(glm::rotate(glm::translate(matBaseBote, glm::vec3(0, 0.060f, 0)), glm::radians(UrandenFisga::actual.boteBalanceo), glm::vec3(0, 0, 1)), glm::vec3(1.0f));
        glUseProgram(0); glActiveTexture(GL_TEXTURE0);
        pescadorFisga.dibujar(pescadorM, view, projection);

        miShader.use();
        glm::vec3 manoMundo = glm::vec3(pescadorM * glm::vec4(pescadorFisga.puntoMano(), 1.0f));
        glm::vec3 fisgaEn = manoMundo;

        if ((UrandenFisga::play && UrandenFisga::FrameIndex == 19 && (UrandenFisga::playIndex > 8 || (UrandenFisga::playIndex == 8 && UrandenFisga::tiempoTramo >= UrandenFisga::duracionTramo(8) * 0.58f))) || UrandenFisga::actual.suelta >= 0.5f || UrandenFisga::fisgaLiberada) {
            if (!UrandenFisga::fisgaLiberada) {
                UrandenFisga::origenSuelta = manoMundo; UrandenFisga::fisgaLiberada = true; UrandenFisga::fisgaClavada = false; UrandenFisga::tiempoDesdeSuelta = 0.0f;
                glm::mat4 giroFisga = glm::rotate(glm::mat4(1.0f), glm::radians(yawBoteFisga), glm::vec3(0, 1, 0));
                giroFisga = glm::rotate(giroFisga, glm::radians(UrandenFisga::actual.fisgaAngulo), glm::vec3(1, 0, 0));
                glm::vec3 ejeA = glm::normalize(glm::vec3(giroFisga * glm::vec4(0, 1, 0, 0)));
                glm::vec3 fwd = glm::normalize(glm::vec3(ejeA.x, 0.0f, ejeA.z)); if (glm::length(fwd) < 0.0001f) fwd = glm::vec3(0, 0, 1);
                UrandenFisga::velocidadSalida = fwd * UrandenFisga::RAPIDEZ_HORIZONTAL + glm::vec3(0.0f, glm::clamp(UrandenFisga::RAPIDEZ_HORIZONTAL * ejeA.y / std::max(glm::length(glm::vec2(ejeA.x, ejeA.z)), 0.01f), 0.45f, 1.60f), 0.0f);
                UrandenFisga::tiempoImpacto = UrandenFisga::calcularTiempoImpacto();
            }
            else if (!UrandenFisga::fisgaClavada) UrandenFisga::tiempoDesdeSuelta += std::max(0.0f, std::min(deltaTime, 0.10f));
            fisgaEn = UrandenFisga::centroFisgaEn(std::min(UrandenFisga::tiempoDesdeSuelta, UrandenFisga::tiempoImpacto));
            if (!UrandenFisga::fisgaClavada && UrandenFisga::tiempoDesdeSuelta >= UrandenFisga::tiempoImpacto) UrandenFisga::fisgaClavada = true;
        }

        glm::mat4 fisgaM = glm::translate(glm::mat4(1.0f), fisgaEn);
        if (UrandenFisga::fisgaLiberada) {
            glm::vec3 punta = glm::normalize(UrandenFisga::velocidadFisgaEn(std::min(UrandenFisga::tiempoDesdeSuelta, UrandenFisga::tiempoImpacto)));
            glm::vec3 ejeG = glm::cross(glm::vec3(0, 1, 0), punta); float seno = glm::length(ejeG);
            if (seno > 0.00001f) fisgaM = glm::rotate(fisgaM, std::atan2(seno, glm::clamp(glm::dot(glm::vec3(0, 1, 0), punta), -1.0f, 1.0f)), ejeG / seno);
        }
        else {
            fisgaM = glm::rotate(fisgaM, glm::radians(yawBoteFisga), glm::vec3(0, 1, 0));
            fisgaM = glm::rotate(fisgaM, glm::radians(UrandenFisga::actual.fisgaAngulo), glm::vec3(1, 0, 0));
        }

        // Proporción de la fisga ajustada al pescador en 1.0f * escalaFisgaGlobal (0.70f = 0.42f / 0.60f)
        fisgaM = glm::scale(fisgaM, glm::vec3(0.70f * escalaFisgaGlobal));
        fisgaM = glm::translate(fisgaM, glm::vec3(0.0f, 0.55f, 0.0f));

        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(fisgaM));
        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        modeloFisga.Draw(miShader);

        // --- 4. VIEJITOS DANZANTES Y SUS BASTONES ---
        glUseProgram(0); glActiveTexture(GL_TEXTURE0);

        glm::mat4 modeloViejito1 = glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, -0.05f, 15.6f) + offsetViejitos + glm::vec3(posX1, posY1, posZ1));
        modeloViejito1 = glm::rotate(modeloViejito1, glm::radians(-45.0f + rotacionViejitos), glm::vec3(0.0f, 1.0f, 0.0f));
        modeloViejito1 = glm::scale(modeloViejito1, glm::vec3(escalaViejitos));
        viejito1.dibujar(modeloViejito1, view, projection);

        glm::mat4 modeloViejito2 = glm::translate(glm::mat4(1.0f), glm::vec3(3.2f, -0.05f, 16.0f) + offsetViejitos + glm::vec3(posX2, posY2, posZ2));
        modeloViejito2 = glm::rotate(modeloViejito2, glm::radians(-90.0f + rotacionViejitos), glm::vec3(0.0f, 1.0f, 0.0f));
        modeloViejito2 = glm::scale(modeloViejito2, glm::vec3(escalaViejitos));
        viejito2.dibujar(modeloViejito2, view, projection);

        miShader.use();
        const glm::vec3 agarreDedosReposo(-0.351f, 0.765f, 0.126f);
        const glm::vec3 puntoEnBaston(-0.045f, 0.48f, -0.015f);

        glm::vec3 agarreAnimado1 = glm::vec3(viejito1.movimientoManoDerecha() * glm::vec4(agarreDedosReposo, 1.0f));
        glm::mat4 modeloBaston1 = glm::translate(modeloViejito1, agarreAnimado1);
        float inclinacionZ1 = 8.0f + 1.5f * std::sin(relojCuerpo1 * 2.1f) + glm::clamp(bastonZ1 * 0.16f, -4.0f, 4.0f);
        float inclinacionX1 = glm::clamp(bastonX1 * 0.14f, -3.5f, 3.5f);
        modeloBaston1 = glm::rotate(modeloBaston1, glm::radians(inclinacionZ1), glm::vec3(0, 0, 1));
        modeloBaston1 = glm::rotate(modeloBaston1, glm::radians(inclinacionX1), glm::vec3(1, 0, 0));
        modeloBaston1 = glm::scale(modeloBaston1, glm::vec3(factorEscalaBaston));
        modeloBaston1 = glm::translate(modeloBaston1, -puntoEnBaston);
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(modeloBaston1));
        bastonViejito.Draw(miShader);

        glm::vec3 agarreAnimado2 = glm::vec3(viejito2.movimientoManoDerecha() * glm::vec4(agarreDedosReposo, 1.0f));
        glm::mat4 modeloBaston2 = glm::translate(modeloViejito2, agarreAnimado2);
        float inclinacionZ2 = 8.0f + 1.5f * std::sin(relojCuerpo2 * 2.1f) + glm::clamp(bastonZ2 * 0.16f, -4.0f, 4.0f);
        float inclinacionX2 = glm::clamp(bastonX2 * 0.14f, -3.5f, 3.5f);
        modeloBaston2 = glm::rotate(modeloBaston2, glm::radians(inclinacionZ2), glm::vec3(0, 0, 1));
        modeloBaston2 = glm::rotate(modeloBaston2, glm::radians(inclinacionX2), glm::vec3(1, 0, 0));
        modeloBaston2 = glm::scale(modeloBaston2, glm::vec3(factorEscalaBaston));
        modeloBaston2 = glm::translate(modeloBaston2, -puntoEnBaston);
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(modeloBaston2));
        bastonViejito.Draw(miShader);

        // --- LUCES GEOMÉTRICAS Y PUENTES ---
        glUniform1i(ubicacionUniform(miShader.ID, "useTexture"), false);
        glUniform1i(ubicacionUniform(miShader.ID, "isLightSource"), true);
        glUniform1f(ubicacionUniform(miShader.ID, "fogDensity"), 0.0f);
        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.0f, 0.98f, 0.90f);
        for (const auto& p : postesPlaza) { glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::translate(glm::mat4(1.0f), p), glm::vec3(0.009f)))); glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); }
        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
        for (const auto& p : lucesOrilla) { glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::translate(glm::mat4(1.0f), p), glm::vec3(0.019f)))); glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); }
        for (const auto& p : lucesManantialOpuesto) { glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::translate(glm::mat4(1.0f), p), glm::vec3(0.019f)))); glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); }
        for (const auto& f : focosUranden) { glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), f.pos), glm::radians(f.yaw), glm::vec3(0, 1, 0)), glm::radians(f.pitch), glm::vec3(1, 0, 0)), glm::vec3(0.035f)))); glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); }
        for (const auto& f : focosLetras) { glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), f.pos), glm::radians(f.yaw), glm::vec3(0, 1, 0)), glm::radians(f.pitch), glm::vec3(1, 0, 0)), glm::vec3(0.035f)))); glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); }

        auto dibujarFocosPuente = [&](const std::vector<glm::vec3>& pts, int numFocos) {
            for (int cuerda = 0; cuerda < 2; cuerda++) {
                glm::vec3 pA = cuerda ? pts[2] : pts[0], pB = cuerda ? pts[3] : pts[1];
                std::vector<glm::vec3> c(numFocos);
                for (int i = 0; i < numFocos; i++) { float tC = (float)i / (numFocos - 1); c[i] = glm::mix(pA, pB, tC) - glm::vec3(0, std::sin(tC * 3.14159f) * 0.075f, 0); }
                glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 0.03f, 0.03f, 0.03f);
                for (int i = 0; i < numFocos - 1; i++) {
                    glm::vec3 dir = c[i + 1] - c[i], dirN = glm::normalize(dir);
                    glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::rotate(glm::rotate(glm::translate(glm::mat4(1.0f), (c[i] + c[i + 1]) * 0.5f), glm::radians(glm::degrees(std::atan2(dirN.x, dirN.z))), glm::vec3(0, 1, 0)), glm::radians(glm::degrees(-std::asin(dirN.y))), glm::vec3(1, 0, 0)), glm::vec3(0.0018f, 0.0018f, glm::length(dir) * 0.5f))));
                    glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 4.5f, 2.2f, 0.4f);
                for (int i = 0; i < numFocos; i++) { glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::scale(glm::translate(glm::mat4(1.0f), c[i]), glm::vec3(0.007f)))); glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); }
            }
            };
        dibujarFocosPuente(postesPuente, 9); dibujarFocosPuente(postesPuente2, 12); glBindVertexArray(0);

        glUniform1i(ubicacionUniform(miShader.ID, "isLightSource"), false); glUniform1f(ubicacionUniform(miShader.ID, "fogDensity"), fogDensity);
        waterShader.use();
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(ubicacionUniform(waterShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3f(ubicacionUniform(waterShader.ID, "dirLightDir"), -0.2f, -1.0f, -0.3f);
        glUniform1f(ubicacionUniform(waterShader.ID, "time"), currentFrame * 0.08f);
        glUniform3fv(ubicacionUniform(waterShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(ubicacionUniform(waterShader.ID, "fogDensity"), fogDensity);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture); glUniform1i(ubicacionUniform(waterShader.ID, "skybox"), 1);
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.88f, 0.0f))));
        glDisable(GL_CULL_FACE); glBindVertexArray(waterVAO); glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0); glBindVertexArray(0);

        glDepthFunc(GL_LEQUAL); skyboxShader.use();
        glUniformMatrix4fv(ubicacionUniform(skyboxShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(view))));
        glUniformMatrix4fv(ubicacionUniform(skyboxShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(skyboxVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture); glDrawArrays(GL_TRIANGLES, 0, 36); glBindVertexArray(0); glDepthFunc(GL_LESS);

        if (estacionActivaHUD != -1) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_DEPTH_TEST);
            hudShader.use(); glUniform1i(ubicacionUniform(hudShader.ID, "hudTexture"), 0);
            hudShader.setMat4("projection", glm::ortho(0.0f, 800.0f, 0.0f, 600.0f));
            hudShader.setMat4("model", glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(80.0f, 120.0f, 0.0f)), glm::vec3(640.0f, 360.0f, 1.0f)));
            glActiveTexture(GL_TEXTURE0);
            if (estacionActivaHUD == 0) glBindTexture(GL_TEXTURE_2D, texCedula1); else if (estacionActivaHUD == 1) glBindTexture(GL_TEXTURE_2D, texCedula4); else if (estacionActivaHUD == 2) glBindTexture(GL_TEXTURE_2D, texCedula2); else if (estacionActivaHUD == 3) glBindTexture(GL_TEXTURE_2D, texCedula3);
            glBindVertexArray(hudVAO); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        }

        camera.Position = posFisicaGuardada;
        glfwSwapBuffers(window); glfwPollEvents(); showFPS(window);
    }

    glDeleteVertexArrays(1, &skyboxVAO); glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &waterVAO); glDeleteBuffers(1, &waterVBO); glDeleteBuffers(1, &waterEBO);
    glDeleteVertexArrays(1, &hudVAO); glDeleteBuffers(1, &hudVBO);

    pescadorFisga.liberar();
    audioEngine.cleanup();
    glfwTerminate();
    return 0;
}