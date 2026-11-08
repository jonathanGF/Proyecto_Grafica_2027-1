#ifndef _WIN64
#define GLEW_STATIC
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SOIL2/SOIL2.h"

#include "Shader.h"
#include "Model.h"
#include "AudioEngine.h"
#include "Camera.h"

// =========================================================================
// 1. CONSTANTES Y CONFIGURACIÓN GLOBAL
// =========================================================================
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 1.5f, 4.0f));

bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
double lastFpsTime = 0.0;
int nbFrames = 0;

// =========================================================================
// 2. DATOS DE ESCENA Y TRAYECTORIAS
// =========================================================================

glm::vec3 offsetMuelle = glm::vec3(-3.5f, 0.0f, 0.0f);

// 4 veladoras del muelle
const glm::vec3 pointLightOffsets[4] = {
    glm::vec3(1.05f, -0.21f,  0.5f),
    glm::vec3(-1.05f, -0.21f,  0.5f),
    glm::vec3(-1.05f, -0.21f, -0.5f),
    glm::vec3(1.05f, -0.21f, -0.5f)
};

// 5 postes fijos
const std::vector<glm::vec3> postesPlaza = {
    glm::vec3(4.62443f, -0.108958f, 15.885f),
    glm::vec3(3.70724f, -0.111046f, 15.8275f),
    glm::vec3(2.78916f, -0.114986f, 15.7848f),
    glm::vec3(2.09366f, -0.116454f, 16.0913f),
    glm::vec3(2.08908f, -0.113338f, 16.7173f)
};

// 6 reflectores cálidos en la orilla original
std::vector<glm::vec3> lucesOrilla = {
    glm::vec3(6.4632f,  -0.660309f, -0.730785f),
    glm::vec3(4.06971f, -0.651941f,  4.17367f),
    glm::vec3(3.24339f, -0.62345f,   7.06666f),
    glm::vec3(3.04058f, -0.623944f,  9.44348f),
    glm::vec3(2.94714f, -0.597321f, 12.0f),
    glm::vec3(5.20106f, -0.262329f, 15.4927f)
};

// 2 luces dedicadas para el letrero y el árbol
std::vector<glm::vec3> lucesLetrero = {
    glm::vec3(1.43314f, -0.634289f, -11.0765f),
    glm::vec3(0.0f,     3.68036f,  -14.6803f)
};

// 4 postes exactos del puente de madera
std::vector<glm::vec3> postesPuente = {
    glm::vec3(-0.254487f, -0.363107f, 16.8868f),
    glm::vec3(0.719089f, -0.35674f,  16.8865f),
    glm::vec3(-0.253564f, -0.370363f, 16.6411f),
    glm::vec3(0.726335f, -0.359537f, 16.64f)
};

// 4 reflectores para el lado opuesto del manantial
std::vector<glm::vec3> lucesManantialOpuesto = {
    glm::vec3(-3.08164f, -0.686701f,  3.60584f),
    glm::vec3(-3.11661f, -0.656584f,  7.2f),
    glm::vec3(-2.87594f, -0.676895f, 10.5f),
    glm::vec3(-2.59666f, -0.646597f, 13.5f)
};

// --- ESTRUCTURA PARA LOS REFLECTORES ---
struct FocoEstructura {
    glm::vec3 pos;
    float pitch;
    float yaw;
};

// Valores fijos calibrados de las 4 LUCES de URANDÉN
std::vector<FocoEstructura> focosUranden = {
    { glm::vec3(0.472333f, -0.277844f, -11.3845f), 0.0f, 12.621f },
    { glm::vec3(1.043f, -0.351215f, -11.5507f), 0.0f, 19.0009f },
    { glm::vec3(1.5f, -0.354501f, -11.7209f), 0.0f, 14.1641f },
    { glm::vec3(2.07332f, -0.383588f, -11.8695f), 0.0f, 16.9766f }
};

// Valores calibrados de las 4 CAJITAS BRILLANTES (Geometría del foco frente a las letras)
std::vector<FocoEstructura> focosLetras = {
    { glm::vec3(0.472333f, -0.277844f, -10.0f), 0.0f, 0.0f },
    { glm::vec3(1.043f, -0.351215f, -10.0f), 0.0f, 0.0f },
    { glm::vec3(1.5f, -0.354501f, -10.0f), 0.0f, 0.0f },
    { glm::vec3(2.07332f, -0.383588f, -10.0f), 0.0f, 0.0f }
};

int orillaSeleccionada = -1;
int letreroSeleccionado = -1;
int postePuenteSeleccionado = -1;
int reflectorOpuestoSeleccionado = -1;
int focoUrandenSeleccionado = -1;
int focoLetrasSeleccionado = -1;

// Trayectoria con segmentos de longitud homogénea
std::vector<glm::vec3> puntosCircuito = {
    glm::vec3(-1.0f, -0.65f,  0.5f),
    glm::vec3(-1.1f, -0.65f,  3.5f),
    glm::vec3(-1.1f, -0.65f,  6.5f),
    glm::vec3(-0.9f, -0.65f,  9.5f),
    glm::vec3(-0.4f, -0.65f, 12.0f),
    glm::vec3(0.4f, -0.65f, 13.5f),
    glm::vec3(1.1f, -0.65f, 12.0f),
    glm::vec3(1.5f, -0.65f,  9.5f),
    glm::vec3(1.6f, -0.65f,  6.5f),
    glm::vec3(1.5f, -0.65f,  3.5f),
    glm::vec3(1.0f, -0.65f,  0.5f),
    glm::vec3(0.0f, -0.65f, -0.6f)
};

// Escalas definitivas calibradas
float escalaCanoaA1 = 1.78327f;
const float escalaCanoaProcesion = 0.462468f;

// --- VALORES CALIBRADOS DE LA COPALERA (FIJOS) ---
float escalaCopalera = 0.85f;
glm::vec3 offsetCopalera = glm::vec3(0.0f, 0.331801f, 0.05f);
glm::vec3 offsetFuegoSahumador = glm::vec3(0.0f, 0.45f, 0.25f);

// Luces del interior de la canoa (Proa y Popa)
glm::vec3 offsetLuzProa = glm::vec3(0.0f, 0.30f, 2.5f);
glm::vec3 offsetLuzPopa = glm::vec3(0.0f, 0.30f, -2.5f);

// =========================================================================
// CONTROL Y APARCAMIENTO DE LA PROCESIÓN
// =========================================================================
struct PosicionAparcamiento {
    glm::vec3 posicion;
    float yaw;
};

const std::vector<PosicionAparcamiento> aparcamientosProcesion = {
    { glm::vec3(0.20f, -0.65f, 11.2f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  9.8f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  8.4f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  7.0f),  90.0f },
    { glm::vec3(0.20f, -0.65f,  5.6f),  90.0f }
};

bool animacionProcesionActiva = false;
float tiempoProcesion = 0.0f;

const int   NUM_CANOAS_PROC = 5;
const float VEL_PROCESION = 1.2f;
const float ACEL_PROCESION = 1.5f;
const float ESPACIO_CANOAS = 1.8f;
const float FRENADO_PROCESION = 0.6f;
const float LATERAL_ATRAQUE = 0.7f;
const float GIRO_ATRAQUE = 0.5f;

const std::vector<glm::vec3> puntosProcesion = {
    glm::vec3(-1.60839f, -0.65f, -0.249108f),
    glm::vec3(-1.34177f, -0.65f,  0.640236f),
    glm::vec3(-1.27887f, -0.65f,  1.4768f),
    glm::vec3(-1.19585f, -0.65f,  2.7161f),
    glm::vec3(-1.1114f,  -0.65f,  4.21009f),
    glm::vec3(-0.81947f, -0.65f, 10.3526f),
    glm::vec3(-0.468589f,-0.65f, 11.7561f),
    glm::vec3(0.332512f,-0.65f, 11.9176f),
    glm::vec3(0.976588f,-0.65f, 10.0949f),
    glm::vec3(1.11899f, -0.65f,  8.63335f),
    glm::vec3(1.33907f, -0.65f,  7.06824f),
    glm::vec3(1.63217f, -0.65f,  4.76648f),
    glm::vec3(1.68298f, -0.65f,  3.11731f),
    glm::vec3(1.44546f, -0.65f,  1.66692f),
    glm::vec3(1.22888f, -0.65f,  1.02546f),
    glm::vec3(-0.189947f,-0.65f, -0.172758f),
    glm::vec3(-0.42318f, -0.65f,  2.20993f),
    glm::vec3(0.322296f,-0.65f,  5.25056f)
};

struct RutaCanoa {
    std::vector<glm::vec3> pts;
    std::vector<float> dist;
};
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

struct Estacion {
    glm::vec3 posicion;
    float radioDeteccion;
    bool interactuable;
    bool activa;
};

std::vector<Estacion> estaciones = {
    {glm::vec3(-3.5f, 0.0f, 0.0f), 3.0f, false, false},
    {glm::vec3(-5.0f, 0.0f, 4.0f), 3.0f, false, false},
    {glm::vec3(0.0f, 0.0f, 6.0f),  3.0f, false, false},
    {glm::vec3(2.0f, 0.0f, 10.0f), 3.0f, false, false}
};

int estacionActivaHUD = -1;
bool eKeyPressed = false;

// =========================================================================
// A1: EXHIBICION DE LOS MARIPOSEROS 
// =========================================================================
glm::vec3 posCanoaA1 = glm::vec3(-1.87317f, -0.806756f, 10.7197f);
float yawCanoaA1 = 101.509f;
bool calibrandoA1 = false;

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

const float L_MUSLO = 0.14f;
const float L_PIERNA = 0.22f;

struct PoseA1 {
    float torso;
    float cadera;
    float rodilla;
    float hombroExtFrente;
    float hombroExtLado;
    float codoExt;
    float hombroIntFrente;
    float hombroIntLado;
    float codoInt;
    float redInclinacion;
    float redLado;
    float redApertura;
};
PoseA1 poseA1 = {};

const int NUM_ARTIC_A1 = 12;
const char* NOMBRES_ARTIC_A1[NUM_ARTIC_A1] = {
    "torso", "cadera", "rodilla",
    "hombroExtFrente", "hombroExtLado", "codoExt",
    "hombroIntFrente", "hombroIntLado", "codoInt",
    "redInclinacion", "redLado", "redApertura"
};
float* articA1(PoseA1& p, int i) {
    float* campos[NUM_ARTIC_A1] = {
        &p.torso, &p.cadera, &p.rodilla,
        &p.hombroExtFrente, &p.hombroExtLado, &p.codoExt,
        &p.hombroIntFrente, &p.hombroIntLado, &p.codoInt,
        &p.redInclinacion, &p.redLado, &p.redApertura };
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
bool reproduciendoA1 = true;
float tiempoAnimA1 = 0.0f;
const float DURACION_TRAMO_A1 = 1.0f;
const char* ARCHIVO_KF_A1 = "animaciones/A1_mariposeros.txt";

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

float escalaA1 = 0.462976f;
const float SEPARACION_PESCADOR = 0.225f;
float pisoCanoaA1 = -0.07f;
const float PIES_A_ORIGEN = 0.43f;

// =========================================================================
// 3. FUNCIONES AUXILIARES
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

void agregarPunto(RutaCanoa& r, const glm::vec3& p) {
    if (!r.pts.empty()) {
        float d = glm::distance(r.pts.back(), p);
        if (d < 1e-5f) return;
        r.dist.push_back(r.dist.back() + d);
    }
    else {
        r.dist.push_back(0.0f);
    }
    r.pts.push_back(p);
}

void muestrearCatmull(RutaCanoa& r, const std::vector<glm::vec3>& c,
    const glm::vec3& fantasmaIni, const glm::vec3& fantasmaFin, int muestras) {
    std::vector<glm::vec3> e;
    e.push_back(fantasmaIni);
    e.insert(e.end(), c.begin(), c.end());
    e.push_back(fantasmaFin);
    for (size_t i = 1; i + 2 < e.size(); i++)
        for (int j = 0; j <= muestras; j++)
            agregarPunto(r, calcularCatmullRom(e[i - 1], e[i], e[i + 1], e[i + 2], (float)j / muestras));
}

void muestrearBezier(RutaCanoa& r, const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2, const glm::vec3& p3, int muestras) {
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
        glm::vec3 inicioCarril(primero.posicion.x - LATERAL_ATRAQUE, primero.posicion.y,
            primero.posicion.z - GIRO_ATRAQUE - 1.0f);

        RutaCanoa r;
        for (int j = 0; j <= 20; j++)
            agregarPunto(r, glm::mix(colaFila, inicio, (float)j / 20.0f));

        std::vector<glm::vec3> ctrl(puntosProcesion.begin(), puntosProcesion.end() - 1);
        ctrl.push_back(inicioCarril);
        if (entrada.z > inicioCarril.z + 0.3f) ctrl.push_back(entrada);
        else ctrl.back() = entrada;
        muestrearCatmull(r, ctrl, inicio + dirAtras, entrada + glm::vec3(0.0f, 0.0f, 1.0f), 30);

        muestrearBezier(r, entrada, entrada + glm::vec3(0.0f, 0.0f, GIRO_ATRAQUE),
            lugar.posicion - dirFinal * GIRO_ATRAQUE, lugar.posicion, 30);
        rutasProcesion.push_back(r);
    }
}

void evaluarRuta(const RutaCanoa& r, float d, glm::vec3& pos, float& yaw) {
    if (d <= 0.0f) d = 0.0f;
    if (d >= r.dist.back()) d = r.dist.back();
    size_t j = std::upper_bound(r.dist.begin(), r.dist.end(), d) - r.dist.begin();
    if (j == 0) j = 1;
    if (j >= r.pts.size()) j = r.pts.size() - 1;
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
    float paso = tamano / (float)resolucion;
    float mitad = tamano / 2.0f;

    for (int i = 0; i <= resolucion; ++i) {
        float z = -mitad + i * paso;
        for (int j = 0; j <= resolucion; ++j) {
            float x = -mitad + j * paso;
            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(z);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
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
// 4. ENTRADA Y CONTROL DE CALIBRACIÓN
// =========================================================================

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        if (!eKeyPressed) {
            eKeyPressed = true;
            for (int i = 0; i < (int)estaciones.size(); i++) {
                if (estaciones[i].interactuable) {
                    if (estacionActivaHUD == i) {
                        estacionActivaHUD = -1;
                    }
                    else {
                        estacionActivaHUD = i;
                    }
                    break;
                }
            }
        }
    }
    else {
        eKeyPressed = false;
    }

    if (teclaPulsada(window, GLFW_KEY_6) || teclaPulsada(window, GLFW_KEY_SPACE)) {
        animacionProcesionActiva = true;
        tiempoProcesion = 0.0f;
        std::cout << ">> Procesion de canoas iniciada en el manantial." << std::endl;
    }

    static bool kPresionada = false;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        if (!kPresionada) {
            kPresionada = true;
            std::cout << "glm::vec3(" << camera.Position.x << "f, -0.65f, " << camera.Position.z << "f)," << std::endl;
        }
    }
    else {
        kPresionada = false;
    }

    // --- CONTROLES DE CALIBRACIÓN: SELECCIÓN DE FOCOS ---
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) { postePuenteSeleccionado = 0; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) { postePuenteSeleccionado = 1; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) { postePuenteSeleccionado = 2; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) { postePuenteSeleccionado = 3; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }

    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 0; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 1; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 3; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; calibrandoA1 = false; }

    // --- F1 a F4 para los FOCOS DE URANDEN ORIGINALES ---
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) { focoUrandenSeleccionado = 0; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) { focoUrandenSeleccionado = 1; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) { focoUrandenSeleccionado = 2; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS) { focoUrandenSeleccionado = 3; focoLetrasSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }

    // --- F5 a F8 para los NUEVOS FOCOS DE LAS LETRAS ---
    if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) { focoLetrasSeleccionado = 0; focoUrandenSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS) { focoLetrasSeleccionado = 1; focoUrandenSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS) { focoLetrasSeleccionado = 2; focoUrandenSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }
    if (glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS) { focoLetrasSeleccionado = 3; focoUrandenSeleccionado = -1; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; calibrandoA1 = false; }

    // --- Movimiento y Rotación: FOCOS DE URANDÉN ORIGINALES ---
    if (focoUrandenSeleccionado >= 0 && focoUrandenSeleccionado < (int)focosUranden.size()) {
        float velAjuste = 0.6f * deltaTime;
        float velRot = 60.0f * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    focosUranden[focoUrandenSeleccionado].pos.z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  focosUranden[focoUrandenSeleccionado].pos.z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  focosUranden[focoUrandenSeleccionado].pos.x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) focosUranden[focoUrandenSeleccionado].pos.x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     focosUranden[focoUrandenSeleccionado].pos.y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     focosUranden[focoUrandenSeleccionado].pos.y -= velAjuste;

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)     focosUranden[focoUrandenSeleccionado].yaw += velRot;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)     focosUranden[focoUrandenSeleccionado].yaw -= velRot;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)     focosUranden[focoUrandenSeleccionado].pitch += velRot;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)     focosUranden[focoUrandenSeleccionado].pitch -= velRot;
    }

    // --- Movimiento y Rotación: NUEVOS FOCOS DE LAS LETRAS ---
    if (focoLetrasSeleccionado >= 0 && focoLetrasSeleccionado < (int)focosLetras.size()) {
        float velAjuste = 0.6f * deltaTime;
        float velRot = 60.0f * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    focosLetras[focoLetrasSeleccionado].pos.z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  focosLetras[focoLetrasSeleccionado].pos.z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  focosLetras[focoLetrasSeleccionado].pos.x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) focosLetras[focoLetrasSeleccionado].pos.x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     focosLetras[focoLetrasSeleccionado].pos.y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     focosLetras[focoLetrasSeleccionado].pos.y -= velAjuste;

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)     focosLetras[focoLetrasSeleccionado].yaw += velRot;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)     focosLetras[focoLetrasSeleccionado].yaw -= velRot;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)     focosLetras[focoLetrasSeleccionado].pitch += velRot;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)     focosLetras[focoLetrasSeleccionado].pitch -= velRot;
    }

    if (postePuenteSeleccionado >= 0 && postePuenteSeleccionado < (int)postesPuente.size()) {
        float velAjuste = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    postesPuente[postePuenteSeleccionado].z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  postesPuente[postePuenteSeleccionado].z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  postesPuente[postePuenteSeleccionado].x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) postesPuente[postePuenteSeleccionado].x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     postesPuente[postePuenteSeleccionado].y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     postesPuente[postePuenteSeleccionado].y -= velAjuste;
    }

    if (reflectorOpuestoSeleccionado >= 0 && reflectorOpuestoSeleccionado < (int)lucesManantialOpuesto.size()) {
        float velAjuste = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    lucesManantialOpuesto[reflectorOpuestoSeleccionado].z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  lucesManantialOpuesto[reflectorOpuestoSeleccionado].z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  lucesManantialOpuesto[reflectorOpuestoSeleccionado].x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) lucesManantialOpuesto[reflectorOpuestoSeleccionado].x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     lucesManantialOpuesto[reflectorOpuestoSeleccionado].y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     lucesManantialOpuesto[reflectorOpuestoSeleccionado].y -= velAjuste;
    }

    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        calibrandoA1 = true; postePuenteSeleccionado = -1; reflectorOpuestoSeleccionado = -1; focoUrandenSeleccionado = -1; focoLetrasSeleccionado = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) calibrandoA1 = false;

    if (calibrandoA1) {
        float v = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    posCanoaA1.z -= v;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  posCanoaA1.z += v;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  posCanoaA1.x -= v;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) posCanoaA1.x += v;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     posCanoaA1.y += v;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     posCanoaA1.y -= v;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)     yawCanoaA1 += 60.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)     yawCanoaA1 -= 60.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)     escalaA1 += 0.3f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) { escalaA1 -= 0.3f * deltaTime; if (escalaA1 < 0.05f) escalaA1 = 0.05f; }
        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)     escalaCanoaA1 += 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) { escalaCanoaA1 -= 0.6f * deltaTime; if (escalaCanoaA1 < 0.1f) escalaCanoaA1 = 0.1f; }
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)     pisoCanoaA1 += 0.2f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)     pisoCanoaA1 -= 0.2f * deltaTime;
    }

    static bool tabPresionado = false;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!tabPresionado) {
            tabPresionado = true;
            articSeleccionadaA1 = (articSeleccionadaA1 + 1) % NUM_ARTIC_A1;
            std::cout << "Articulacion seleccionada: " << NOMBRES_ARTIC_A1[articSeleccionadaA1] << std::endl;
        }
    }
    else {
        tabPresionado = false;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) poseA1 = PoseA1{};
    if (articSeleccionadaA1 >= 0 && !reproduciendoA1) {
        float* a = articA1(poseA1, articSeleccionadaA1);
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) *a += 45.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) *a -= 45.0f * deltaTime;
    }

    if (teclaPulsada(window, GLFW_KEY_1)) {
        reproduciendoA1 = !reproduciendoA1;
        std::cout << (reproduciendoA1 ? "A1: reproduciendo" : "A1: pausa (edicion manual)") << std::endl;
    }
    if (teclaPulsada(window, GLFW_KEY_2)) {
        keyframesA1.push_back(poseA1);
        std::cout << "A1: cuadro agregado. Total = " << keyframesA1.size() << std::endl;
    }
    if (teclaPulsada(window, GLFW_KEY_3)) {
        keyframesA1.clear();
        reproduciendoA1 = false;
        std::cout << "A1: cuadros borrados (captura con 2)" << std::endl;
    }
    if (teclaPulsada(window, GLFW_KEY_4)) {
        if (guardarKeyframesA1(ARCHIVO_KF_A1)) std::cout << "A1: guardado en " << ARCHIVO_KF_A1 << std::endl;
        else std::cout << "A1: no se pudo guardar (existe la carpeta 'animaciones'?)" << std::endl;
    }
    if (teclaPulsada(window, GLFW_KEY_5)) {
        if (cargarKeyframesA1(ARCHIVO_KF_A1)) std::cout << "A1: cargados " << keyframesA1.size() << " cuadros" << std::endl;
        else std::cout << "A1: no se pudo cargar " << ARCHIVO_KF_A1 << std::endl;
    }

    static bool pKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) {
            pKeyPressed = true;

            std::cout << "\n=== COORDENADAS Y ROTACION NUEVOS FOCOS LETRAS ===" << std::endl;
            for (size_t i = 0; i < focosLetras.size(); i++) {
                std::cout << "    { glm::vec3(" << focosLetras[i].pos.x << "f, " << focosLetras[i].pos.y << "f, " << focosLetras[i].pos.z << "f), "
                    << focosLetras[i].pitch << "f, " << focosLetras[i].yaw << "f }," << std::endl;
            }
            std::cout << "==================================================\n" << std::endl;
        }
    }
    else {
        pKeyPressed = false;
    }
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
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "ERROR::GLEW::Fallo al inicializar GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Shader skyboxShader("shader/skybox.vert", "shader/skybox.frag");
    Shader waterShader("shader/water.vert", "shader/water.frag");

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

    Model modeloCanoa("Models/canoa.obj");
    Model modeloCanoaPro("Models/canoaPro.obj");
    Model modeloCopalera("Models/copalera.gltf");
    construirRutasProcesion();

    std::cout << "Cargando mapa completo de Uranden..." << std::endl;
    Model modeloMapaCompleto("Models/MapaCompleto/Uranden_MAPA.gltf");

    std::cout << "Cargando mariposero..." << std::endl;
    Model mCuerpo("Models/mariposero/cuerpo.obj");
    Model mBrazoDer("Models/mariposero/brazoDer.obj");
    Model mBrazoIzq("Models/mariposero/brazoIzq.obj");
    Model mAntebrazoDer("Models/mariposero/antebrazoDer.obj");
    Model mAntebrazoIzq("Models/mariposero/antebrazoIzq.obj");
    Model mMusloDer("Models/mariposero/musloDer.obj");
    Model mMusloIzq("Models/mariposero/musloIzq.obj");
    Model mPantorrillaDer("Models/mariposero/pantorrillaDer.obj");
    Model mPantorrillaIzq("Models/mariposero/pantorrillaIzq.obj");
    Model mMango("Models/mariposero/mango.obj");
    Model mAlaIzq("Models/mariposero/alaIzq.obj");
    Model mAlaDer("Models/mariposero/alaDer.obj");

    if (cargarKeyframesA1(ARCHIVO_KF_A1))
        std::cout << "A1: keyframes cargados desde " << ARCHIVO_KF_A1 << std::endl;
    else
        std::cout << "A1: usando los 14 keyframes predeterminados" << std::endl;

    auto dibujar = [&](Model& m, const glm::mat4& M) {
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(M));
        m.Draw(miShader);
        };

    auto dibujarPurepecha = [&](const glm::mat4& matBaseCanoa, float xOffset, float yawRotacion, bool invertirLado) {
        float aC = glm::radians(poseA1.cadera);
        float aR = glm::radians(poseA1.rodilla);
        float bajada = (L_MUSLO + L_PIERNA) - (L_MUSLO * std::cos(aC) + L_PIERNA * std::cos(aC - aR));

        const glm::vec3 EJE_X(1.0f, 0.0f, 0.0f), EJE_Y(0.0f, 1.0f, 0.0f), EJE_Z(0.0f, 0.0f, 1.0f);

        float yLocal = pisoCanoaA1 + PIES_A_ORIGEN * escalaA1;
        glm::mat4 base = glm::translate(matBaseCanoa, glm::vec3(xOffset, yLocal, 0.0f));
        base = glm::rotate(base, glm::radians(yawRotacion), EJE_Y);
        base = glm::scale(base, glm::vec3(escalaA1));
        if (invertirLado) base = glm::scale(base, glm::vec3(-1.0f, 1.0f, 1.0f));

        glm::mat4 pelvis = glm::translate(base, glm::vec3(0.0f, -bajada, 0.0f));
        glm::mat4 torso = rotarEnPivote(pelvis, P_PELVIS, poseA1.torso, EJE_X);
        dibujar(mCuerpo, torso);

        glm::mat4 brazoExt = rotarEnPivote(torso, P_HOMBRO_IZQ, -poseA1.hombroExtFrente, EJE_X);
        brazoExt = rotarEnPivote(brazoExt, P_HOMBRO_IZQ, -poseA1.hombroExtLado, EJE_Z);
        glm::mat4 antebrazoExt = rotarEnPivote(brazoExt, P_CODO_IZQ, -poseA1.codoExt, EJE_X);
        dibujar(mBrazoIzq, brazoExt);
        dibujar(mAntebrazoIzq, antebrazoExt);

        glm::mat4 brazoInt = rotarEnPivote(torso, P_HOMBRO_DER, -poseA1.hombroIntFrente, EJE_X);
        brazoInt = rotarEnPivote(brazoInt, P_HOMBRO_DER, poseA1.hombroIntLado, EJE_Z);
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
    if (!audioEngine.init()) {
        std::cerr << "ERROR::MINIAUDIO::No se pudo inicializar el motor de audio" << std::endl;
    }
    audioEngine.playMusic("audio/Ofrenda_en_el_Agua.mp3", 0.35f, true);
    audioEngine.playAmbient("audio/ambiente.mp3", 0.25f, true);

    Shader hudShader("shader/hud.vs", "shader/hud.fs");
    unsigned int hudVAO, hudVBO;
    float hudVertices[] = {
         0.0f,  1.0f,       0.0f, 1.0f,
         1.0f,  0.0f,       1.0f, 0.0f,
         0.0f,  0.0f,       0.0f, 0.0f,
         0.0f,  1.0f,       0.0f, 1.0f,
         1.0f,  1.0f,       1.0f, 1.0f,
         1.0f,  0.0f,       1.0f, 0.0f
    };
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);
    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(hudVertices), hudVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    unsigned int texCedula1 = SOIL_load_OGL_texture("Models/textures/1_Las-copaleras-y-el-rito-de-purificacion.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula2 = SOIL_load_OGL_texture("Models/textures/2_Danzas-y-cocina-tradicional.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula3 = SOIL_load_OGL_texture("Models/textures/3_Procesion-de-Canoas.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    unsigned int texCedula4 = SOIL_load_OGL_texture("Models/textures/4_El-rescate-de-los-manantiales.png", SOIL_LOAD_RGBA, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        for (int i = 0; i < (int)estaciones.size(); i++) {
            float distancia = glm::distance(glm::vec2(camera.Position.x, camera.Position.z),
                glm::vec2(estaciones[i].posicion.x, estaciones[i].posicion.z));
            estaciones[i].interactuable = (distancia <= estaciones[i].radioDeteccion);
        }

        float tiempoAnim = currentFrame;

        if (animacionProcesionActiva) {
            tiempoProcesion += deltaTime;
        }

        float avance = animacionProcesionActiva ? avanceProcesion(tiempoProcesion) : 0.0f;
        for (int k = 0; k < NUM_CANOAS_PROC; k++) {
            const RutaCanoa& r = rutasProcesion[k];
            float L = r.dist.back();
            float d = ESPACIO_CANOAS * (NUM_CANOAS_PROC - 1 - k) + avance;
            float inicioFrenado = L - FRENADO_PROCESION;
            if (d > inicioFrenado) {
                float e = d - inicioFrenado;
                d = (e >= 2.0f * FRENADO_PROCESION) ? L : inicioFrenado + e - e * e / (4.0f * FRENADO_PROCESION);
            }
            evaluarRuta(r, d, posProc[k], yawProc[k]);
        }

        glm::vec3 fogColor = glm::vec3(0.012f, 0.018f, 0.035f);
        float fogDensity = 0.012f;

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // --- D. Shader Principal: Luces y Cámara ---
        miShader.use();
        glUniform3fv(glGetUniformLocation(miShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.ambient"), 0.04f, 0.05f, 0.08f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.diffuse"), 0.18f, 0.20f, 0.25f);
        glUniform3f(glGetUniformLocation(miShader.ID, "dirLight.specular"), 0.15f, 0.15f, 0.20f);

        for (unsigned int i = 0; i < 4; i++) {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            glm::vec3 posLuzReal = pointLightOffsets[i] + offsetMuelle;
            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzReal));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.09f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.032f);
        }

        {
            glm::vec4 offLocal = glm::vec4(0.0f, 0.9f, 0.8f, 1.0f);
            glm::vec3 posLuzA1 = posCanoaA1 + glm::vec3(
                glm::rotate(glm::mat4(1.0f), glm::radians(yawCanoaA1), glm::vec3(0.0f, 1.0f, 0.0f)) * offLocal);
            glUniform3fv(glGetUniformLocation(miShader.ID, "pointLights[0].position"), 1, glm::value_ptr(posLuzA1));
            glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[0].ambient"), 0.03f, 0.03f, 0.03f);
            glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[0].diffuse"), 0.95f, 0.90f, 0.80f);
            glUniform3f(glGetUniformLocation(miShader.ID, "pointLights[0].specular"), 0.20f, 0.20f, 0.20f);
            glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[0].constant"), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[0].linear"), 0.35f);
            glUniform1f(glGetUniformLocation(miShader.ID, "pointLights[0].quadratic"), 0.45f);
        }

        for (unsigned int i = 0; i < 5; i++) {
            int lightIndex = 5 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(postesPlaza[i]));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.005f, 0.005f, 0.005f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.68f, 0.62f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.25f, 0.25f, 0.22f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.60f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 1.40f);
        }

        for (unsigned int i = 0; i < 6; i++) {
            int lightIndex = 10 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < lucesOrilla.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesOrilla[i]));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.010f, 0.003f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.75f, 0.28f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.32f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.38f);
            }
        }

        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 16 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < lucesLetrero.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesLetrero[i]));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.015f, 0.005f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.10f, 0.70f, 0.25f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.70f, 0.45f, 0.18f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.22f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.20f);
            }
        }

        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 18 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            float tP = (i == 0) ? 0.35f : 0.65f;
            glm::vec3 pIzq = glm::mix(postesPuente[0], postesPuente[1], tP);
            glm::vec3 pDer = glm::mix(postesPuente[2], postesPuente[3], tP);

            glm::vec3 posLuzPuente = (pIzq + pDer) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);

            glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzPuente));
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.008f, 0.001f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.32f, 0.05f);
            glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.30f, 0.15f, 0.03f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.70f);
            glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 1.80f);
        }

        for (unsigned int i = 0; i < 4; i++) {
            int lightIndex = 20 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < lucesManantialOpuesto.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesManantialOpuesto[i]));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.010f, 0.003f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.25f, 0.78f, 0.28f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.32f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.38f);
            }
        }

        // 10. FOCOS ORIGINALES DEL LETRERO URANDÉN (Índices 40 al 43)
        for (unsigned int i = 0; i < 4; i++) {
            int lightIndex = 40 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < focosUranden.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(focosUranden[i].pos));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.01f, 0.00f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.65f, 0.20f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.22f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.20f);
            }
        }

        // 11. NUEVOS FOCOS EN FRENTE DE LAS LETRAS (Índices 44 al 47)
        for (unsigned int i = 0; i < focosLetras.size(); i++) {
            int lightIndex = 44 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";

            if (i < focosLetras.size()) {
                glUniform3fv(glGetUniformLocation(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(focosLetras[i].pos));
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.01f, 0.00f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.65f, 0.20f);
                glUniform3f(glGetUniformLocation(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "constant").c_str()), 1.0f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "linear").c_str()), 0.22f);
                glUniform1f(glGetUniformLocation(miShader.ID, (base + "quadratic").c_str()), 0.20f);
            }
        }

        glm::vec3 spotLightPos = glm::vec3(0.0f, 3.5f, 1.0f);
        glm::vec3 spotLightDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        glUniform3fv(glGetUniformLocation(miShader.ID, "spotLight.position"), 1, glm::value_ptr(spotLightPos));
        glUniform3fv(glGetUniformLocation(miShader.ID, "spotLight.direction"), 1, glm::value_ptr(spotLightDir));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.cutOff"), glm::cos(glm::radians(18.0f)));
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.outerCutOff"), glm::cos(glm::radians(24.0f)));
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.ambient"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.diffuse"), 0.75f, 0.75f, 0.80f);
        glUniform3f(glGetUniformLocation(miShader.ID, "spotLight.specular"), 0.40f, 0.40f, 0.45f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.constant"), 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.linear"), 0.14f);
        glUniform1f(glGetUniformLocation(miShader.ID, "spotLight.quadratic"), 0.07f);

        glUniform3fv(glGetUniformLocation(miShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), fogDensity);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), true);
        glUniform1i(glGetUniformLocation(miShader.ID, "isLightSource"), false);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "materialAlpha"), 1.0f);

        glm::mat4 mapaModel = glm::mat4(1.0f);
        mapaModel = glm::translate(mapaModel, glm::vec3(0.0f, -0.2f, 0.0f));
        mapaModel = glm::scale(mapaModel, glm::vec3(0.2f));
        glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(mapaModel));
        modeloMapaCompleto.Draw(miShader);

        for (int k = 0; k < NUM_CANOAS_PROC; k++) {
            glm::vec3 posK = posProc[k];
            float yawK = yawProc[k];

            float faseK = tiempoAnim + (k * 0.5f);
            float bamboleoK = std::sin(faseK * 2.85f) * 1.2f;
            float cabeceoK = std::cos(faseK * 2.85f) * 0.6f;
            posK.y += std::sin(tiempoAnim * 1.2f + (k * 0.7f)) * 0.008f;

            glm::mat4 canoaMat = glm::mat4(1.0f);
            canoaMat = glm::translate(canoaMat, posK);
            canoaMat = glm::rotate(canoaMat, glm::radians(yawK), glm::vec3(0.0f, 1.0f, 0.0f));
            canoaMat = glm::rotate(canoaMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            canoaMat = glm::rotate(canoaMat, glm::radians(bamboleoK), glm::vec3(0.0f, 0.0f, 1.0f));
            canoaMat = glm::rotate(canoaMat, glm::radians(cabeceoK), glm::vec3(1.0f, 0.0f, 0.0f));
            canoaMat = glm::scale(canoaMat, glm::vec3(escalaCanoaProcesion));

            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaMat));
            modeloCanoaPro.Draw(miShader);

            glm::mat4 copaleraMat = canoaMat;
            copaleraMat = glm::translate(copaleraMat, offsetCopalera);
            copaleraMat = glm::rotate(copaleraMat, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            copaleraMat = glm::scale(copaleraMat, glm::vec3(escalaCopalera));

            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(copaleraMat));
            glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
            modeloCopalera.Draw(miShader);

            glm::vec3 posSahumador = glm::vec3(copaleraMat * glm::vec4(offsetFuegoSahumador, 1.0f));
            int luzIndexSahumador = 25 + k;
            std::string baseSah = "pointLights[" + std::to_string(luzIndexSahumador) + "].";

            float parpadeoBrasa = 0.6f + 0.4f * std::sin(tiempoAnim * 28.0f + k * 7.0f) * std::cos(tiempoAnim * 15.0f + k);
            glm::vec3 colorBrasa = glm::vec3(1.0f, 0.30f, 0.05f) * parpadeoBrasa;

            glUniform3fv(glGetUniformLocation(miShader.ID, (baseSah + "position").c_str()), 1, glm::value_ptr(posSahumador));
            glUniform3f(glGetUniformLocation(miShader.ID, (baseSah + "ambient").c_str()), colorBrasa.r * 0.05f, colorBrasa.g * 0.05f, colorBrasa.b * 0.05f);
            glUniform3f(glGetUniformLocation(miShader.ID, (baseSah + "diffuse").c_str()), colorBrasa.r, colorBrasa.g, colorBrasa.b);
            glUniform3f(glGetUniformLocation(miShader.ID, (baseSah + "specular").c_str()), colorBrasa.r * 0.5f, colorBrasa.g * 0.5f, colorBrasa.b * 0.5f);
            glUniform1f(glGetUniformLocation(miShader.ID, (baseSah + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (baseSah + "linear").c_str()), 2.5f);
            glUniform1f(glGetUniformLocation(miShader.ID, (baseSah + "quadratic").c_str()), 4.5f);

            glm::vec3 posLuzProa = glm::vec3(canoaMat * glm::vec4(offsetLuzProa, 1.0f));
            glm::vec3 posLuzPopa = glm::vec3(canoaMat * glm::vec4(offsetLuzPopa, 1.0f));

            int idxProa = 30 + (k * 2);
            int idxPopa = 31 + (k * 2);

            float parpadeoVela = 0.8f + 0.2f * std::sin(tiempoAnim * 10.0f + k * 3.14f);
            glm::vec3 colorVela = glm::vec3(0.9f, 0.6f, 0.2f) * parpadeoVela;

            std::string baseProa = "pointLights[" + std::to_string(idxProa) + "].";
            glUniform3fv(glGetUniformLocation(miShader.ID, (baseProa + "position").c_str()), 1, glm::value_ptr(posLuzProa));
            glUniform3f(glGetUniformLocation(miShader.ID, (baseProa + "ambient").c_str()), colorVela.r * 0.05f, colorVela.g * 0.05f, colorVela.b * 0.05f);
            glUniform3f(glGetUniformLocation(miShader.ID, (baseProa + "diffuse").c_str()), colorVela.r, colorVela.g, colorVela.b);
            glUniform3f(glGetUniformLocation(miShader.ID, (baseProa + "specular").c_str()), colorVela.r * 0.3f, colorVela.g * 0.3f, colorVela.b * 0.3f);
            glUniform1f(glGetUniformLocation(miShader.ID, (baseProa + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (baseProa + "linear").c_str()), 1.2f);
            glUniform1f(glGetUniformLocation(miShader.ID, (baseProa + "quadratic").c_str()), 1.8f);

            std::string basePopa = "pointLights[" + std::to_string(idxPopa) + "].";
            glUniform3fv(glGetUniformLocation(miShader.ID, (basePopa + "position").c_str()), 1, glm::value_ptr(posLuzPopa));
            glUniform3f(glGetUniformLocation(miShader.ID, (basePopa + "ambient").c_str()), colorVela.r * 0.05f, colorVela.g * 0.05f, colorVela.b * 0.05f);
            glUniform3f(glGetUniformLocation(miShader.ID, (basePopa + "diffuse").c_str()), colorVela.r, colorVela.g, colorVela.b);
            glUniform3f(glGetUniformLocation(miShader.ID, (basePopa + "specular").c_str()), colorVela.r * 0.3f, colorVela.g * 0.3f, colorVela.b * 0.3f);
            glUniform1f(glGetUniformLocation(miShader.ID, (basePopa + "constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(miShader.ID, (basePopa + "linear").c_str()), 1.2f);
            glUniform1f(glGetUniformLocation(miShader.ID, (basePopa + "quadratic").c_str()), 1.8f);
        }

        glm::mat4 canoaA1 = glm::translate(glm::mat4(1.0f), posCanoaA1);
        canoaA1 = glm::rotate(canoaA1, glm::radians(yawCanoaA1), glm::vec3(0.0f, 1.0f, 0.0f));
        dibujar(modeloCanoa, glm::scale(canoaA1, glm::vec3(escalaCanoaA1)));

        if (reproduciendoA1 && keyframesA1.size() >= 2) {
            int tramos = (int)keyframesA1.size() - 1;
            tiempoAnimA1 += deltaTime;
            float duracionTotal = tramos * DURACION_TRAMO_A1;
            if (tiempoAnimA1 >= duracionTotal) tiempoAnimA1 = std::fmod(tiempoAnimA1, duracionTotal);
            int k = (int)(tiempoAnimA1 / DURACION_TRAMO_A1);
            if (k >= tramos) k = tramos - 1;
            float tA1 = (tiempoAnimA1 - k * DURACION_TRAMO_A1) / DURACION_TRAMO_A1;
            poseA1 = interpolarPose(keyframesA1[k], keyframesA1[k + 1], tA1);
        }

        float sepA1 = SEPARACION_PESCADOR * escalaCanoaA1;
        dibujarPurepecha(canoaA1, sepA1, 0.0f, false);
        dibujarPurepecha(canoaA1, -sepA1, 0.0f, true);

        // --- Render Focos Físicos (Geometría) ---
        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), false);
        glUniform1i(glGetUniformLocation(miShader.ID, "isLightSource"), true);
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), 0.0f);

        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 0.98f, 0.90f);
        for (unsigned int i = 0; i < postesPlaza.size(); i++) {
            glm::mat4 focoModel = glm::mat4(1.0f);
            focoModel = glm::translate(focoModel, postesPlaza[i]);
            focoModel = glm::scale(focoModel, glm::vec3(0.009f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
        for (unsigned int i = 0; i < lucesOrilla.size(); i++) {
            glm::mat4 focoOrillaModel = glm::mat4(1.0f);
            focoOrillaModel = glm::translate(focoOrillaModel, lucesOrilla[i]);
            focoOrillaModel = glm::scale(focoOrillaModel, glm::vec3(0.019f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoOrillaModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        for (unsigned int i = 0; i < lucesManantialOpuesto.size(); i++) {
            if ((int)i == reflectorOpuestoSeleccionado) {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 0.0f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            }

            glm::mat4 focoOpuestoModel = glm::mat4(1.0f);
            focoOpuestoModel = glm::translate(focoOpuestoModel, lucesManantialOpuesto[i]);
            focoOpuestoModel = glm::scale(focoOpuestoModel, glm::vec3(0.019f));
            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoOpuestoModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // DIBUJAR LOS CUBOS DE URANDÉN ORIGINALES ROTADOS
        for (unsigned int i = 0; i < focosUranden.size(); i++) {
            if ((int)i == focoUrandenSeleccionado) {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 5.0f, 5.0f, 0.0f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            }

            glm::mat4 focoUrandenModel = glm::mat4(1.0f);
            focoUrandenModel = glm::translate(focoUrandenModel, focosUranden[i].pos);
            focoUrandenModel = glm::rotate(focoUrandenModel, glm::radians(focosUranden[i].yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            focoUrandenModel = glm::rotate(focoUrandenModel, glm::radians(focosUranden[i].pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            focoUrandenModel = glm::scale(focoUrandenModel, glm::vec3(0.035f));

            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoUrandenModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // DIBUJAR LOS NUEVOS CUBOS DE LAS LETRAS ROTADOS
        for (unsigned int i = 0; i < focosLetras.size(); i++) {
            if ((int)i == focoLetrasSeleccionado) {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 5.0f, 5.0f, 0.0f);
            }
            else {
                glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            }

            glm::mat4 focoLetrasModel = glm::mat4(1.0f);
            focoLetrasModel = glm::translate(focoLetrasModel, focosLetras[i].pos);
            focoLetrasModel = glm::rotate(focoLetrasModel, glm::radians(focosLetras[i].yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            focoLetrasModel = glm::rotate(focoLetrasModel, glm::radians(focosLetras[i].pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            focoLetrasModel = glm::scale(focoLetrasModel, glm::vec3(0.035f));

            glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoLetrasModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        int numFocosHilera = 9;
        for (int cuerda = 0; cuerda < 2; cuerda++) {
            glm::vec3 pA = (cuerda == 0) ? postesPuente[0] : postesPuente[2];
            glm::vec3 pB = (cuerda == 0) ? postesPuente[1] : postesPuente[3];

            std::vector<glm::vec3> puntosCuerda(numFocosHilera);
            for (int i = 0; i < numFocosHilera; i++) {
                float tC = (float)i / (float)(numFocosHilera - 1);
                glm::vec3 p = glm::mix(pA, pB, tC);
                p.y -= std::sin(tC * 3.14159f) * 0.075f;
                puntosCuerda[i] = p;
            }

            glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 0.03f, 0.03f, 0.03f);
            for (int i = 0; i < numFocosHilera - 1; i++) {
                glm::vec3 pInicio = puntosCuerda[i];
                glm::vec3 pFin = puntosCuerda[i + 1];

                glm::vec3 dirSegmento = pFin - pInicio;
                float distSegmento = glm::length(dirSegmento);
                glm::vec3 centroSegmento = (pInicio + pFin) * 0.5f;

                glm::vec3 dirNorm = glm::normalize(dirSegmento);
                float yawSegmento = glm::degrees(std::atan2(dirNorm.x, dirNorm.z));
                float pitchSegmento = glm::degrees(-std::asin(dirNorm.y));

                glm::mat4 cableModel = glm::mat4(1.0f);
                cableModel = glm::translate(cableModel, centroSegmento);
                cableModel = glm::rotate(cableModel, glm::radians(yawSegmento), glm::vec3(0.0f, 1.0f, 0.0f));
                cableModel = glm::rotate(cableModel, glm::radians(pitchSegmento), glm::vec3(1.0f, 0.0f, 0.0f));
                cableModel = glm::scale(cableModel, glm::vec3(0.0018f, 0.0018f, distSegmento * 0.5f));

                glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(cableModel));
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }

            for (int i = 0; i < numFocosHilera; i++) {
                int idPosteActual = -1;
                if (cuerda == 0 && i == 0) idPosteActual = 0;
                else if (cuerda == 0 && i == numFocosHilera - 1) idPosteActual = 1;
                else if (cuerda == 1 && i == 0) idPosteActual = 2;
                else if (cuerda == 1 && i == numFocosHilera - 1) idPosteActual = 3;

                if (idPosteActual != -1 && idPosteActual == postePuenteSeleccionado) {
                    glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 5.0f, 5.0f, 0.0f);
                }
                else {
                    glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 4.5f, 2.2f, 0.4f);
                }

                glm::mat4 foquitoModel = glm::mat4(1.0f);
                foquitoModel = glm::translate(foquitoModel, puntosCuerda[i]);
                foquitoModel = glm::scale(foquitoModel, glm::vec3(0.007f));
                glUniformMatrix4fv(glGetUniformLocation(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(foquitoModel));

                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }

        glUniform1i(glGetUniformLocation(miShader.ID, "isLightSource"), false);
        glUniform1i(glGetUniformLocation(miShader.ID, "useTexture"), true);
        glUniform3f(glGetUniformLocation(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(miShader.ID, "fogDensity"), fogDensity);

        waterShader.use();
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(waterShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3f(glGetUniformLocation(waterShader.ID, "dirLightDir"), -0.2f, -1.0f, -0.3f);

        glUniform1f(glGetUniformLocation(waterShader.ID, "time"), currentFrame * 0.08f);
        glUniform3fv(glGetUniformLocation(waterShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(glGetUniformLocation(waterShader.ID, "fogDensity"), fogDensity);

        glm::mat4 waterModel = glm::mat4(1.0f);
        waterModel = glm::translate(waterModel, glm::vec3(0.0f, -0.88f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(waterShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(waterModel));

        glDisable(GL_CULL_FACE);
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

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

        if (estacionActivaHUD != -1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);

            hudShader.use();
            glUniform1i(glGetUniformLocation(hudShader.ID, "hudTexture"), 0);
            glm::mat4 orthoProj = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
            hudShader.setMat4("projection", orthoProj);

            glm::mat4 orthoModel = glm::mat4(1.0f);
            orthoModel = glm::translate(orthoModel, glm::vec3(80.0f, 120.0f, 0.0f));
            orthoModel = glm::scale(orthoModel, glm::vec3(640.0f, 360.0f, 1.0f));
            hudShader.setMat4("model", orthoModel);

            glActiveTexture(GL_TEXTURE0);
            if (estacionActivaHUD == 0) glBindTexture(GL_TEXTURE_2D, texCedula1);
            else if (estacionActivaHUD == 1) glBindTexture(GL_TEXTURE_2D, texCedula2);
            else if (estacionActivaHUD == 2) glBindTexture(GL_TEXTURE_2D, texCedula3);
            else if (estacionActivaHUD == 3) glBindTexture(GL_TEXTURE_2D, texCedula4);

            glBindVertexArray(hudVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        showFPS(window);
    }

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &waterVAO);
    glDeleteBuffers(1, &waterVBO);
    glDeleteBuffers(1, &waterEBO);
    glDeleteVertexArrays(1, &hudVAO);
    glDeleteBuffers(1, &hudVBO);

    audioEngine.cleanup();
    glfwTerminate();
    return 0;
}