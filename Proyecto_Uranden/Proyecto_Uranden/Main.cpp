#ifndef NOMINMAX
#define NOMINMAX
#endif
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
#include "StreamingMap.h"
#include "GroundWalker.h"
#include "WaterRenderer.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "AudioEngine.h"
#include "Camera.h"
#include "ViejitoRig.h" // <-- IMPORTAMOS LA CLASE DE LUIS

// Fuerza el uso de la GPU dedicada en laptops con graficos hibridos (NVIDIA Optimus / AMD).
// En equipos sin GPU dedicada el driver lo ignora.
#ifdef _WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// Cache de ubicaciones de uniforms: glGetUniformLocation se consulta una sola vez por nombre
// y despues se reutiliza (antes se llamaba ~350 veces por cuadro).
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

// ================= AJUSTES DEL AGUA =================
const float ALTURA_AGUA = -0.99f;       // Mas negativo = agua mas abajo.
const float VELOCIDAD_AGUA = 0.65f;     // Velocidad de las ondas, no del bote.
const float FUERZA_ONDAS = 1.0f;        // 0.0 = calma; 1.0 = lago suave.
const float CLARIDAD_AGUA = 2.2f;       // Mas alto = se ve el fondo a mayor distancia.
const float REFLEJO_AGUA = 1.0f;        // Intensidad Fresnel del reflejo.
bool mostrarAgua = true;              // F7: comparar FPS con/sin agua.
int calidadAgua = 0;                   // F6: 0 ligero, 1 equilibrado, 2 alto.
// ===================================================


// Transformacion UNICA para mapa y barrera. glTF de Blender exportado con +Y Up.
const float ESCALA_MAPA = 0.2f;
const glm::vec3 DESPLAZAMIENTO_MAPA(0.0f, -0.2f, 0.0f);
const bool USAR_BARRERA_ANTIGUA = false; // La caminata usa ahora el suelo real como limite.
const double RADIO_BARRERA = 0.12; // Margen horizontal en unidades de OpenGL.

uranden::StreamStats estadisticasMapa;
uranden::StreamingMap* mapaCaminata = nullptr;
GroundWalker caminante;
bool reubicarCaminante=true;
const char* RUTA_MAPA = "Models/MapaCompleto/Rendido.gltf";
const float VELOCIDAD_CIRCUITO = 0.035f;
bool modoCalibracion = false;
int orillaSeleccionada=-1,letreroSeleccionado=-1,postePuenteSeleccionado=-1,reflectorOpuestoSeleccionado=-1;
glm::mat4 vistaProyeccionAnimaciones(1.0f);

Camera camera(glm::vec3(0.0f, 0.10f, 4.0f));
// --- VARIABLES PARA MODOS DE CAMARA ---
bool modoDron = false;
bool modoCaminataPOV = true; // Activa el seguimiento de terreno y zancada humana
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
double lastFpsTime = 0.0;
int nbFrames = 0;
float tiempoInicioPato = 0.0f;
bool patoIniciado = false;

// Multiplicador de luz ambiental global fijo
float intensidadGlobal = 2.3284f;

// --- VARIABLES FINALES PARA LOS VIEJITOS ---
float escalaViejitos = 0.161891f;
glm::vec3 offsetViejitos(0.352146f, -0.199837f, 0.954319f);
float velAnimViejitos = 0.233078f;
float rotacionViejitos = -109.633f;
float factorEscalaBaston = 0.759367f; // Multiplicador para compensar el OBJ del baston

// =========================================================================
// 2. DATOS DE ESCENA Y TRAYECTORIAS FIJAS
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

// 4 postes exactos del PRIMER puente de madera
std::vector<glm::vec3> postesPuente = {
    glm::vec3(-0.254487f, -0.363107f, 16.8868f),
    glm::vec3(0.719089f, -0.35674f,  16.8865f),
    glm::vec3(-0.253564f, -0.370363f, 16.6411f),
    glm::vec3(0.726335f, -0.359537f, 16.64f)
};

// --- Postes SEGUNDO puente de madera (Valores fijos calibrados) ---
std::vector<glm::vec3> postesPuente2 = {
    glm::vec3(-7.35653f, -0.430664f, -12.7874f),
    glm::vec3(-5.88413f, -0.426269f, -13.6162f),
    glm::vec3(-7.18257f, -0.440817f, -12.4f),
    glm::vec3(-5.69923f, -0.426434f, -13.2347f)
};
int postePuente2Seleccionado = -1; // Selector activo para calibración

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

// Valores fijos calibrados de los 4 focos originales de URANDÉN
std::vector<FocoEstructura> focosUranden = {
    { glm::vec3(0.472333f, -0.277844f, -11.3845f), 0.0f, 12.621f },
    { glm::vec3(1.043f, -0.351215f, -11.5507f), 0.0f, 19.0009f },
    { glm::vec3(1.5f, -0.354501f, -11.7209f), 0.0f, 14.1641f },
    { glm::vec3(2.07332f, -0.383588f, -11.8695f), 0.0f, 16.9766f }
};

// Valores fijos calibrados de los NUEVOS 4 FOCOS PARA EL FRENTE DE LAS LETRAS
std::vector<FocoEstructura> focosLetras = {
    { glm::vec3(0.604192f, -0.277844f, -10.8625f), 0.0f, 14.057f },
    { glm::vec3(1.19124f, -0.321721f, -11.0867f), 0.0f, 17.422f },
    { glm::vec3(1.64225f, -0.333041f, -11.2188f), 0.0f, 16.8869f },
    { glm::vec3(2.20184f, -0.383588f, -11.415f), 0.0f, 16.8951f }
};

int estacionActivaHUD = -1;
bool eKeyPressed = false;

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

// =========================================================================
// VARIABLES GLOBALES DE ESCALA, ALTURA Y MARIPOSEROS (Fijas)
// =========================================================================
float escalaCanoaA1 = 0.934082f;
float escalaCanoaProcesion = 0.205165f;
float alturaCanoasProcesion = -0.210887f;

float escalaA1 = 0.31759f;
const float SEPARACION_PESCADOR = 0.225f;
float pisoCanoaA1 = -0.07f;
const float PIES_A_ORIGEN = 0.43f;

// --- VALORES CALIBRADOS DE LA COPALERA ---
float escalaCopalera = 0.85f;
glm::vec3 offsetCopalera = glm::vec3(0.0f, 0.331801f, 0.05f);
glm::vec3 offsetFuegoSahumador = glm::vec3(0.0f, 0.45f, 0.25f);

// Luces del interior de la canoa (Proa y Popa)
glm::vec3 offsetLuzProa = glm::vec3(0.0f, 0.30f, 2.5f);
glm::vec3 offsetLuzPopa = glm::vec3(0.0f, 0.30f, -2.5f);

// =========================================================================
// DATOS DEL HUD Y ESTACIONES
// =========================================================================
struct Estacion {
    glm::vec3 posicion;
    float radioDeteccion;
    bool interactuable;
    bool activa;
};

// ESTACIONES CALIBRADAS
std::vector<Estacion> estaciones = {
    {glm::vec3(-1.40175f, 0.0f, 1.49381f), 3.0f, false, false},  // 0: Embarcadero
    {glm::vec3(1.61685f, 0.0f, -9.52137f), 3.0f, false, false},  // 1: Islote de Uranden
    {glm::vec3(2.15513f, 0.0f, 15.4634f), 3.0f, false, false},   // 2: Plaza
    {glm::vec3(0.611198f, 0.0f, 6.4615f), 3.0f, false, false}    // 3: Altar central
};

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

PoseA1 interpolarPose(const PoseA1& a, const PoseA1& b, float t) {
    PoseA1 r;
    PoseA1& ra = const_cast<PoseA1&>(a);
    PoseA1& rb = const_cast<PoseA1&>(b);
    for (int i = 0; i < NUM_ARTIC_A1; i++)
        *articA1(r, i) = *articA1(ra, i) + (*articA1(rb, i) - *articA1(ra, i)) * t;
    return r;
}

bool guardarKeyframesA1(const char* ruta) {
    std::filesystem::path destino(ruta);
    std::error_code ec;std::filesystem::create_directories(destino.parent_path(),ec);
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
    if(!f||n<2||n>10000)return false;
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
// NUEVA LOGICA: BAILE DE LOS VIEJITOS (LUIS)
// =========================================================================
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

static PoseViejito calcularBaile(int estado, float t, float reloj,
    float& posX, float& posY, float& posZ,
    float& bastonZ, float& bastonX) {
    PoseViejito p = posturaBase();
    posX = posY = posZ = bastonZ = bastonX = 0.0f;
    t = limitar01(t);

    // Postura viva: se endereza y luego se encorva poco a poco.
    const float flexion = suavizar(0.5f - 0.5f * std::cos(reloj * 1.45f));
    p.espalda += 19.0f * flexion;
    p.espaldaMedia += 14.0f * flexion;
    p.espaldaAlta += 11.0f * flexion;
    p.cuello -= 7.0f * flexion;
    p.caderaX += 2.0f * flexion;
    p.rodillaIzq += 3.5f * flexion;
    p.rodillaDer += 3.5f * flexion;

    // Mirada suave a ambos lados, sin girar ni tambalear todo el cuerpo.
    p.cabezaY = 15.0f * std::sin(reloj * 0.92f)
        + 3.0f * std::sin(reloj * 1.70f + 0.25f);
    p.cuello += 2.0f * std::sin(reloj * 0.85f);
    p.espaldaLateral += 0.55f * std::sin(reloj * 1.40f);
    p.espaldaAlta += 1.0f * std::sin(reloj * 1.10f);

    // Achaques constantes: temblor pequeno de rodillas.
    p.rodillaIzq += 1.25f * std::sin(reloj * 22.0f);
    p.rodillaDer += 1.25f * std::sin(reloj * 24.0f + 1.3f);

    // El brazo con baston RESPIRA y responde al ritmo.
    p.brazoDerX += 3.5f * std::sin(reloj * 1.8f);
    p.brazoDerZ -= 2.0f * std::sin(reloj * 1.35f + 0.5f);
    p.codoDerX += 7.0f * std::sin(reloj * 2.6f + 0.3f);
    p.munecaDerX += 4.0f * std::sin(reloj * 2.2f);
    p.munecaDerZ += 3.0f * std::sin(reloj * 1.9f + 0.4f);
    p.agarreDerecho = 0.90f + 0.06f * std::sin(reloj * 2.7f);

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

    if (estado == SALTO_IZQUIERDA || estado == SALTO_DERECHA ||
        estado == SALTO_ADELANTE) {
        float avance = suavizar((t - 0.16f) / 0.68f);
        float vuelo = std::pow(std::max(0.0f, std::sin(PI_BAILE * t)), 1.2f);
        float prepara = 1.0f - suavizar(t / 0.16f);
        float aterriza = suavizar((t - 0.75f) / 0.11f) *
            (1.0f - suavizar((t - 0.87f) / 0.13f));
        posY = (estado == SALTO_ADELANTE ? 0.115f : 0.095f) * vuelo;
        p.rodillaIzq += 8.0f * prepara + 12.0f * aterriza + 7.0f * vuelo;
        p.rodillaDer += 8.0f * prepara + 12.0f * aterriza + 7.0f * vuelo;
        p.musloIzq -= 7.0f * vuelo;
        p.musloDer -= 7.0f * vuelo;
        p.pieIzq += 4.0f * vuelo;
        p.pieDer += 4.0f * vuelo;

        p.brazoDerX -= 8.0f * vuelo;
        p.brazoDerZ -= 3.0f * vuelo;
        p.codoDerX += 16.0f * vuelo + 5.0f * aterriza;
        p.munecaDerX -= 5.0f * vuelo;
        if (estado == SALTO_IZQUIERDA) {
            posX = -0.22f * avance;
            p.musloIzqZ += 4.0f * vuelo;
            bastonZ = 5.0f * vuelo;
        }
        else if (estado == SALTO_DERECHA) {
            posX = -0.22f + 0.22f * avance;
            p.musloDerZ -= 4.0f * vuelo;
            bastonZ = -5.0f * vuelo;
        }
        else { // SALTO_ADELANTE
            posZ = 0.18f * avance;
            float golpeFinal = suavizar((t - 0.72f) / 0.10f) *
                (1.0f - suavizar((t - 0.85f) / 0.15f));
            p.rodillaIzq += 7.0f * golpeFinal;
            p.rodillaDer += 7.0f * golpeFinal;
            bastonX = 12.0f * vuelo - 9.0f * golpeFinal;
        }
    }
    else if (estado >= RETROCESO_1 && estado <= RETROCESO_4) {
        int numero = estado - RETROCESO_1;
        float avance = suavizar(t);
        float levanta = std::sin(PI_BAILE * t);
        posZ = 0.18f - 0.045f * (numero + avance);
        bool derecho = (numero % 2 == 0);
        if (derecho) {
            p.musloDer += 19.0f * levanta;
            p.rodillaDer += 25.0f * levanta;
            p.pieDer -= 10.0f * levanta;
        }
        else {
            p.musloIzq += 19.0f * levanta;
            p.rodillaIzq += 25.0f * levanta;
            p.pieIzq -= 10.0f * levanta;
        }
        p.espaldaLateral += (derecho ? 2.2f : -2.2f) * levanta;
        p.caderaZ += (derecho ? 1.2f : -1.2f) * levanta;
        p.codoDerX += 7.0f * levanta;
        p.brazoDerZ -= 2.5f * levanta;
        bastonZ = (derecho ? -6.0f : 6.0f) * levanta;
        posY = 0.008f * levanta;
    }
    else if (estado == PASO_DERECHO_BASTON) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        p.musloDer -= 67.0f * levanta;
        p.rodillaDer += 73.0f * levanta + 9.0f * pisoton;
        p.pieDer -= 22.0f * levanta;
        p.rodillaIzq += 5.0f * levanta; // pierna de apoyo
        posY = 0.012f * levanta - 0.012f * pisoton;
        p.brazoDerX -= 9.0f * levanta;
        p.brazoDerZ -= 12.0f * levanta;
        p.codoDerX += 18.0f * levanta;
        p.munecaDerX -= 6.0f * levanta;
        p.munecaDerZ += 5.0f * levanta;
        bastonZ = 30.0f * levanta - 12.0f * pisoton;
        bastonX = -15.0f * levanta + 8.0f * pisoton;
    }
    else if (estado == PASO_IZQUIERDO) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        p.musloIzq -= 67.0f * levanta;
        p.rodillaIzq += 73.0f * levanta + 9.0f * pisoton;
        p.pieIzq -= 22.0f * levanta;
        p.rodillaDer += 5.0f * levanta;
        posY = 0.012f * levanta - 0.012f * pisoton;
        bastonZ = -8.0f * levanta + 4.0f * pisoton;
    }
    else if (estado == ZAP_DERECHO_1 || estado == ZAP_IZQUIERDO ||
        estado == ZAP_DERECHO_2) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        bool derecho = (estado != ZAP_IZQUIERDO);
        if (derecho) {
            p.musloDer -= 38.0f * levanta;
            p.rodillaDer += 42.0f * levanta + 5.0f * pisoton;
            p.pieDer -= 12.0f * levanta;
        }
        else {
            p.musloIzq -= 38.0f * levanta;
            p.rodillaIzq += 42.0f * levanta + 5.0f * pisoton;
            p.pieIzq -= 12.0f * levanta;
        }
        posY = 0.005f * levanta - 0.009f * pisoton;
        bastonZ = (derecho ? 9.0f : -9.0f) * levanta;
    }
    else if (estado == REMATE) {
        float levanta = alzarPie(t);
        float pisoton = impacto(t);
        p.musloIzq -= 14.0f * levanta;
        p.musloDer -= 14.0f * levanta;
        p.rodillaIzq += 15.0f * levanta + 12.0f * pisoton;
        p.rodillaDer += 15.0f * levanta + 12.0f * pisoton;
        p.pieIzq += 13.0f * levanta;
        p.pieDer += 13.0f * levanta;
        posY = 0.022f * levanta - 0.018f * pisoton;
        p.espalda += 3.0f * pisoton;
        p.codoDerX += 9.0f * levanta + 4.0f * pisoton;
        p.munecaDerX -= 5.0f * levanta;
        bastonZ = 17.0f * levanta - 14.0f * pisoton;
        bastonX = -9.0f * levanta + 7.0f * pisoton;
    }
    return p;
}

// =========================================================================
// 4. ENTRADA, CONTROL Y TOPOGRAFIA DEL TERRENO
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

class BarreraRecorrido {
public:
    std::vector<glm::dvec2> puntos;
    double radio;
    BarreraRecorrido(const std::vector<glm::dvec2>& vertices, double margen)
        : puntos(vertices), radio(margen + 0.0001) {
    } // Holgura para convertir a float.

    static double productoCruz(const glm::dvec2& a, const glm::dvec2& b) {
        return a.x * b.y - a.y * b.x;
    }
    static glm::dvec2 cercano(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b) {
        glm::dvec2 ab = b - a;
        double longitud2 = glm::dot(ab, ab);
        double t = longitud2 > 1e-20 ? glm::dot(p - a, ab) / longitud2 : 0.0;
        t = glm::clamp(t, 0.0, 1.0);
        return a + t * ab;
    }
    static double distancia2(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b) {
        glm::dvec2 d = p - cercano(p, a, b);
        return glm::dot(d, d);
    }
    static double distanciaSegmentos2(const glm::dvec2& a, const glm::dvec2& b,
        const glm::dvec2& c, const glm::dvec2& d) {
        glm::dvec2 r = b - a, s = d - c;
        double denominador = productoCruz(r, s);
        if (std::abs(denominador) > 1e-15) {
            double t = productoCruz(c - a, s) / denominador;
            double u = productoCruz(c - a, r) / denominador;
            if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) return 0.0;
        }
        // Incluye segmentos paralelos, colineales y extremos coincidentes.
        double valor = distancia2(a, c, d);
        double candidato = distancia2(b, c, d); if (candidato < valor) valor = candidato;
        candidato = distancia2(c, a, b); if (candidato < valor) valor = candidato;
        candidato = distancia2(d, a, b); if (candidato < valor) valor = candidato;
        return valor;
    }
    bool dentro(const glm::dvec2& p) const {
        bool interior = false;
        for (size_t i = 0, j = puntos.size() - 1; i < puntos.size(); j = i++) {
            const glm::dvec2& a = puntos[i];
            const glm::dvec2& b = puntos[j];
            if ((a.y > p.y) != (b.y > p.y)) {
                double cruce = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (p.x < cruce) interior = !interior;
            }
        }
        return interior;
    }
    bool posicionValida(const glm::dvec2& p) const {
        if (puntos.size() < 3 || !std::isfinite(p.x) || !std::isfinite(p.y) || !dentro(p)) return false;
        for (size_t i = 0; i < puntos.size(); ++i)
            if (distancia2(p, puntos[i], puntos[(i + 1) % puntos.size()]) < radio * radio) return false;
        return true;
    }
    bool trayectoValido(const glm::dvec2& a, const glm::dvec2& b) const {
        if (!posicionValida(a) || !posicionValida(b)) return false;
        for (size_t i = 0; i < puntos.size(); ++i)
            if (distanciaSegmentos2(a, b, puntos[i], puntos[(i + 1) % puntos.size()]) < radio * radio)
                return false;
        return true;
    }
    glm::dvec2 mover(const glm::dvec2& origen, const glm::dvec2& destino) const {
        if (!posicionValida(origen)) return origen;
        if (!std::isfinite(destino.x) || !std::isfinite(destino.y)) return origen;
        glm::dvec2 p = origen, restante = destino - origen;
        // Contacto y deslizamiento; en una esquina estrecha se detiene de forma segura.
        for (int iteracion = 0; iteracion < 4; ++iteracion) {
            if (glm::dot(restante, restante) < 1e-16) break;
            if (trayectoValido(p, p + restante)) return p + restante;
            double lo = 0.0, hi = 1.0;
            for (int k = 0; k < 28; ++k) {
                double t = (lo + hi) * 0.5;
                if (trayectoValido(p, p + restante * t)) lo = t; else hi = t;
            }
            glm::dvec2 contacto = p + restante * lo;
            glm::dvec2 borde = cercano(contacto, puntos[0], puntos[1]);
            double mejor = glm::dot(contacto - borde, contacto - borde);
            for (size_t i = 1; i < puntos.size(); ++i) {
                glm::dvec2 q = cercano(contacto, puntos[i], puntos[(i + 1) % puntos.size()]);
                double d2 = glm::dot(contacto - q, contacto - q);
                if (d2 < mejor) { mejor = d2; borde = q; }
            }
            glm::dvec2 normalInterior = contacto - borde;
            double longitud = glm::length(normalInterior);
            p = contacto;
            if (longitud < 1e-12) break;
            normalInterior /= longitud;
            restante *= (1.0 - lo);
            double haciaBorde = glm::dot(restante, normalInterior);
            if (haciaBorde >= -1e-12) break;
            restante -= normalInterior * haciaBorde;
        }
        return p;
    }
};

std::vector<glm::dvec2> crearContornoRecorrido() {
    // Coordenadas de mundo del JSON: Blender (x,y,z) -> glTF (x,z,-y).
    // Aqui cada par es (x,-y), todavia sin escala del Main.
    // Orden reconstruido siguiendo las aristas, no suponiendo el orden de vertices.
    std::vector<glm::dvec2> p = {
        glm::dvec2(44.126277923584, 97.240852355957),
        glm::dvec2(29.821449279785, 110.783615112305),
        glm::dvec2(-43.616027832031, 110.951660156250),
        glm::dvec2(-66.974853515625, 93.306495666504),
        glm::dvec2(-66.470710754395, 46.588832855225),
        glm::dvec2(-75.377319335938, 34.489295959473),
        glm::dvec2(-71.176086425781, 6.929237365723),
        glm::dvec2(-68.151199340820, -59.954322814941),
        glm::dvec2(-48.489448547363, -76.423133850098),
        glm::dvec2(-35.717720031738, -97.093177795410),
        glm::dvec2(0.412849426270, -94.740486145020),
        glm::dvec2(28.477058410645, -87.850471496582),
        glm::dvec2(38.559997558594, -84.153404235840),
        glm::dvec2(53.852470397949, -67.516532897949),
        glm::dvec2(66.792259216309, -39.452339172363),
        glm::dvec2(65.783966064453, -7.354942321777),
        glm::dvec2(61.918838500977, 0.711418151855),
        glm::dvec2(62.086887359619, 1.719711303711),
        glm::dvec2(59.566139221191, 103.221389770508),
        glm::dvec2(44.609764099121, 97.171630859375),
    };
    for (auto& punto : p)
        punto = punto * double(ESCALA_MAPA)
        + glm::dvec2(DESPLAZAMIENTO_MAPA.x, DESPLAZAMIENTO_MAPA.z);
    return p;
}
const BarreraRecorrido barreraRecorrido(crearContornoRecorrido(), RADIO_BARRERA);

// --- FUNCION AUXILIAR PARA TELETRANSPORTAR LA CAMARA A PUNTOS CLAVE ---
void fijarCamara(const glm::vec3& pos, float pitch, float yaw) {
    if(USAR_BARRERA_ANTIGUA&&!barreraRecorrido.posicionValida(glm::dvec2(pos.x,pos.z))) {
        std::cerr<<"Vista fuera de la barrera: se conserva la posicion actual.\n";return;
    }
    camera.Position = pos;
    reubicarCaminante=true;
    if(mapaCaminata&&!modoDron) {
        float piso;
        if(mapaCaminata->collisionSurface().sampleNearest(pos.x,pos.z,pos.y-caminante.eyeHeight,piso)) {
            float base;
            if(mapaCaminata->collisionSurface().sampleFoundation(pos.x,pos.z,base))piso=(std::max)(piso,base);
            camera.Position.y=(std::max)(camera.Position.y,piso+caminante.eyeHeight);
        }
    }
    camera.Pitch = pitch;
    camera.Yaw = yaw;
    // Forzamos la actualizacion de los vectores directores
    camera.ProcessMouseMovement(0.0f, 0.0f);
}

// Consulta del suelo desde la cache; no recorre millones de triangulos cada frame.
float obtenerAlturaTerreno(float x,float z,float alturaActual=-0.20f) {
    float y=alturaActual;
    if(mapaCaminata&&mapaCaminata->collisionSurface().sampleNearest(x,z,alturaActual,y)) {
        float base;
        if(mapaCaminata->collisionSurface().sampleFoundation(x,z,base))y=(std::max)(y,base);
        return y;
    }
    return alturaActual;
}

void processCalibrationInput(GLFWwindow* window) {
    // --- SELECCIÓN DE LOS 4 POSTES DEL PUENTE (Teclas 7, 8, 9, 0) ---
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) { postePuenteSeleccionado = 0; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) { postePuenteSeleccionado = 1; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) { postePuenteSeleccionado = 2; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) { postePuenteSeleccionado = 3; orillaSeleccionada = -1; letreroSeleccionado = -1; reflectorOpuestoSeleccionado = -1; }

    // --- SELECCIÓN DE REFLECTORES OPUESTOS DEL MANANTIAL (Teclas I, O, K, L) ---
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 0; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 1; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 2; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) { reflectorOpuestoSeleccionado = 3; postePuenteSeleccionado = -1; orillaSeleccionada = -1; }

    // Movimiento del poste seleccionado
    if (postePuenteSeleccionado >= 0 && postePuenteSeleccionado < (int)postesPuente.size()) {
        float velAjuste = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    postesPuente[postePuenteSeleccionado].z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  postesPuente[postePuenteSeleccionado].z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  postesPuente[postePuenteSeleccionado].x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) postesPuente[postePuenteSeleccionado].x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     postesPuente[postePuenteSeleccionado].y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     postesPuente[postePuenteSeleccionado].y -= velAjuste;
    }

    // Movimiento de los reflectores opuestos del manantial
    if (reflectorOpuestoSeleccionado >= 0 && reflectorOpuestoSeleccionado < (int)lucesManantialOpuesto.size()) {
        float velAjuste = 0.6f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    lucesManantialOpuesto[reflectorOpuestoSeleccionado].z -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  lucesManantialOpuesto[reflectorOpuestoSeleccionado].z += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  lucesManantialOpuesto[reflectorOpuestoSeleccionado].x -= velAjuste;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) lucesManantialOpuesto[reflectorOpuestoSeleccionado].x += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)     lucesManantialOpuesto[reflectorOpuestoSeleccionado].y += velAjuste;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)     lucesManantialOpuesto[reflectorOpuestoSeleccionado].y -= velAjuste;
    }

    // Guardar coordenadas con P
    static bool pKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) {
            pKeyPressed = true;
            std::cout << "\n=== COORDENADAS EXACTAS DE LOS 4 POSTES DEL PUENTE ===" << std::endl;
            for (size_t i = 0; i < postesPuente.size(); i++) {
                std::cout << "    glm::vec3("
                    << postesPuente[i].x << "f, "
                    << postesPuente[i].y << "f, "
                    << postesPuente[i].z << "f)," << std::endl;
            }
            std::cout << "\n=== COORDENADAS EXACTAS REFLECTORES OPUESTOS MANANTIAL ===" << std::endl;
            for (size_t i = 0; i < lucesManantialOpuesto.size(); i++) {
                std::cout << "    glm::vec3("
                    << lucesManantialOpuesto[i].x << "f, "
                    << lucesManantialOpuesto[i].y << "f, "
                    << lucesManantialOpuesto[i].z << "f)," << std::endl;
            }
            std::cout << "=========================================================\n" << std::endl;
        }
    }
    else {
        pKeyPressed = false;
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Restaurar una posicion de recorrido valida al abandonar el dron.
    static glm::vec3 retornoDron = camera.Position;
    static float retornoPitch = camera.Pitch, retornoYaw = camera.Yaw;
    bool movimiento = glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS || glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS ||
        glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS || glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS;
    if(teclaPulsada(window,GLFW_KEY_5) && !modoCalibracion) {
        if(!modoDron){retornoDron=camera.Position;retornoPitch=camera.Pitch;retornoYaw=camera.Yaw;modoDron=true;}
        else {modoDron=false;fijarCamara(retornoDron,retornoPitch,retornoYaw);}
    }
    if(movimiento&&modoDron){modoDron=false;fijarCamara(retornoDron,retornoPitch,retornoYaw);}
    glm::vec3 antes=camera.Position;
    if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS)camera.ProcessKeyboard(FORWARD,deltaTime);
    if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS)camera.ProcessKeyboard(BACKWARD,deltaTime);
    if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS)camera.ProcessKeyboard(LEFT,deltaTime);
    if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS)camera.ProcessKeyboard(RIGHT,deltaTime);
    if(movimiento)modoCaminataPOV=true;
    if(!modoDron) {
        if(USAR_BARRERA_ANTIGUA) {
            glm::dvec2 limitada=barreraRecorrido.mover(glm::dvec2(antes.x,antes.z),glm::dvec2(camera.Position.x,camera.Position.z));
            camera.Position.x=float(limitada.x);camera.Position.z=float(limitada.y);
        }
        if(modoCaminataPOV)camera.Position.y=antes.y; // mirar arriba no permite volar en POV.
    }
    if(teclaPulsada(window,GLFW_KEY_F6))calidadAgua=(calidadAgua+1)%3;
    if(teclaPulsada(window,GLFW_KEY_F7))mostrarAgua=!mostrarAgua;
    if(teclaPulsada(window,GLFW_KEY_F8)) {
        modoCalibracion=!modoCalibracion;
        std::cout<<"Calibracion de luces: "<<(modoCalibracion?"ON":"OFF")<<'\n';
    }
    if(teclaPulsada(window,GLFW_KEY_R)){tiempoInicioPato=float(glfwGetTime());patoIniciado=true;}
    if(modoCalibracion){processCalibrationInput(window);return;}

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        if (!eKeyPressed) {
            eKeyPressed = true;
            for (int i = 0; i < (int)estaciones.size(); i++) {
                if (estaciones[i].interactuable) {
                    if (estacionActivaHUD == i) {
                        estacionActivaHUD = -1; // Cerrar el HUD actual
                    }
                    else {
                        estacionActivaHUD = i; // Abrir este HUD
                    }
                    break;
                }
            }
        }
    }
    else {
        eKeyPressed = false;
    }

    bool tecla6=teclaPulsada(window,GLFW_KEY_6);
    bool espacio=teclaPulsada(window,GLFW_KEY_SPACE);
    if (tecla6 || espacio) {
        animacionProcesionActiva = true;
        tiempoProcesion = 0.0f;
        std::cout << ">> Procesion de canoas iniciada en el manantial." << std::endl;
    }

    // =========================================================================
    // ATAJOS DE CAMARA PARA GRABACION DE VIDEOS (RUBRICA)
    // =========================================================================

    // Tecla 7: Video A - Panoramica elevada del entorno (Desactiva gravedad para no caer)
    if (teclaPulsada(window, GLFW_KEY_7)) {
        modoDron = false;
        modoCaminataPOV = false;
        fijarCamara(glm::vec3(0.0f, 6.5f, 5.0f), -35.0f, -90.0f);
        std::cout << ">> [CAMARA 7]: Vista panoramica del entorno activada." << std::endl;
    }

    // Tecla 8: Video B - Inicio de recorrido (Activa modo caminata POV sobre el terreno)
    if (teclaPulsada(window, GLFW_KEY_8)) {
        modoDron = false;
        modoCaminataPOV = true;
        float ySuelo = obtenerAlturaTerreno(4.5f, -12.5f) + 0.25f;
        fijarCamara(glm::vec3(4.5f, ySuelo, -12.5f), -5.0f, -135.0f);
        std::cout << ">> [CAMARA 8]: Vista inicial de recorrido (Letras Uranden) activada." << std::endl;
    }

    // Tecla 9: Video C - Plaza de los Viejitos Danzantes (Estacion 2)
    if (teclaPulsada(window, GLFW_KEY_9)) {
        modoDron = false;
        modoCaminataPOV = false;
        fijarCamara(glm::vec3(2.8f, 0.2f, 14.5f), -5.0f, 40.0f);
        std::cout << ">> [CAMARA 9]: Vista enfocada a los Viejitos activada." << std::endl;
    }

    // Tecla 0: Video C - Canoa de los Mariposeros (Estacion 3)
    if (teclaPulsada(window, GLFW_KEY_0)) {
        modoDron = false;
        modoCaminataPOV = false;
        fijarCamara(glm::vec3(-0.6f, -0.2f, 10.7f), -8.0f, -170.0f);
        std::cout << ">> [CAMARA 0]: Vista enfocada a los Mariposeros activada." << std::endl;
    }

    // --- CONTROLES DE ANIMACIÓN DE MARIPOSEROS (A1) ---
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
    // Se cambio a tecla L para no chocar con la tecla 5 del Modo Dron
    if (teclaPulsada(window, GLFW_KEY_L)) {
        if (cargarKeyframesA1(ARCHIVO_KF_A1)) std::cout << "A1: cargados " << keyframesA1.size() << " cuadros" << std::endl;
        else std::cout << "A1: no se pudo cargar " << ARCHIVO_KF_A1 << std::endl;
    }

    // --- AJUSTE DE ESCALA DEL BASTON ---
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) factorEscalaBaston = std::max(0.1f, factorEscalaBaston - 0.5f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) factorEscalaBaston += 0.5f * deltaTime;

    static bool bPulsada = false;
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if (!bPulsada) {
            bPulsada = true;
            std::cout << ">> Factor Escala Baston: " << factorEscalaBaston << "f" << std::endl;
        }
    }
    else {
        bPulsada = false;
    }
}

void showFPS(GLFWwindow* window) {
    double currentTime = glfwGetTime();
    nbFrames++;
    if (currentTime - lastFpsTime >= 1.0) {
        double fps = double(nbFrames) / (currentTime - lastFpsTime);
        std::ostringstream info;
        info << "Uranden | " << int(fps) << " FPS | dibujadas " << estadisticasMapa.drawn
            << "/" << estadisticasMapa.total << " | residentes " << estadisticasMapa.resident
            << " | " << estadisticasMapa.triangles / 1000 << "k tris | mapa "
            << (estadisticasMapa.geometryBytes + estadisticasMapa.textureBytes) / uranden::MiB << " MiB"
            << (estadisticasMapa.loading ? " | cargando" : "")
            << (estadisticasMapa.budgetLimited ? " | limite memoria" : "");
        std::string title = info.str();
        glfwSetWindowTitle(window, title.c_str());
        nbFrames = 0;
        lastFpsTime = currentTime;
    }
}

// =========================================================================
// 5. FUNCIÓN PRINCIPAL
// =========================================================================

int main() {
    bool assetsOK=true;
    const char* necesarios[]={"Models/Baston.obj","Models/MapaCompleto/Rendido.gltf","Models/PatoNadando.glb","Models/PatoPreparadoVuelo.glb","Models/Textura_Viejito.png","Models/ViejitoRig.bin","Models/canoa.obj","Models/canoaPro.obj","Models/copalera.gltf","Models/mariposero/alaDer.obj","Models/mariposero/alaIzq.obj","Models/mariposero/antebrazoDer.obj","Models/mariposero/antebrazoIzq.obj","Models/mariposero/brazoDer.obj","Models/mariposero/brazoIzq.obj","Models/mariposero/cuerpo.obj","Models/mariposero/mango.obj","Models/mariposero/musloDer.obj","Models/mariposero/musloIzq.obj","Models/mariposero/pantorrillaDer.obj","Models/mariposero/pantorrillaIzq.obj"};
    for(const char* p:necesarios)if(!std::filesystem::is_regular_file(p)){std::cerr<<"FALTA ARCHIVO: "<<p<<'\n';assetsOK=false;}
    if(!assetsOK){std::cerr<<"Copia los assets de animaciones indicados en LEEME_INTEGRACION.txt.\n";return -1;}

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,0);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "El Camino de las Animas", NULL, NULL);
    if (window == NULL) {
        std::cerr << "ERROR::GLFW::Fallo al crear la ventana" << std::endl;
        glfwTerminate();
        return -1;
    }

    // --- ACOMODAR VENTANAS AUTOMATICAMENTE ---
#ifdef _WIN32
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) {
        SetWindowPos(consoleWindow, NULL, 0, 0, 600, 600, SWP_NOZORDER);
    }
#endif
    glfwSetWindowPos(window, 620, 50);

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
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;

    glEnable(GL_DEPTH_TEST);
    { // Recursos GL destruidos antes de cerrar el contexto.
    Shader miShader("shader/shader.vert", "shader/shader.frag");
    Shader skyboxShader("shader/skybox.vert", "shader/skybox.frag");
    Shader waterShader("shader/water.vert", "shader/water.frag");

    GLint mapLinked = GL_FALSE;
    glGetProgramiv(miShader.ID, GL_LINK_STATUS, &mapLinked);
    if (!mapLinked) {
        std::cerr << "ERROR: reemplaza juntos shader/shader.vert y shader/shader.frag." << std::endl;
        glfwTerminate(); return -1;
    }
    GLint waterLinked = GL_FALSE;
    glGetProgramiv(waterShader.ID, GL_LINK_STATUS, &waterLinked);
    if (!waterLinked) {
        std::cerr << "ERROR: revisa shader/water.vert y shader/water.frag del paquete de agua." << std::endl;
        glfwTerminate();
        return -1;
    }
    WaterRenderer waterRenderer;

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
    glUniform1i(ubicacionUniform(skyboxShader.ID, "skybox"), 0);

    std::vector<float> waterVertices;
    std::vector<unsigned int> waterIndices;
    int waterResolution = 80;
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
    Model bastonViejito("Models/Baston.obj");
    std::cout << "Cargando modelo de pato..." << std::endl;
    Model patoNadando("Models/PatoNadando.glb");
    Model patoPreparandoVuelo("Models/PatoPreparadoVuelo.glb");
    construirRutasProcesion();

    std::cout << "Preparando carga por zonas de Uranden..." << std::endl;
    uranden::StreamSettings ajustesMapa;
    // Ajustes en unidades finales de OpenGL (ESCALA_MAPA ya aplicada).
    ajustesMapa.terrainScale=ESCALA_MAPA;
    ajustesMapa.terrainOffset=DESPLAZAMIENTO_MAPA;
    ajustesMapa.drawDistance = 24.0f;
    ajustesMapa.preloadDistance = 28.0f;
    ajustesMapa.unloadDistance = 34.0f;
    ajustesMapa.fogStart = 17.0f;
    ajustesMapa.maxTextureSize = 2048; // 1024 ahorra mas memoria; originales intactos.
    ajustesMapa.geometryBudget = 384 * uranden::MiB;
    ajustesMapa.textureBudget = 512 * uranden::MiB;
    uranden::StreamingMap modeloMapaCompleto(ajustesMapa);
    auto preparacionMapa = std::async(std::launch::async, [&]() {
        modeloMapaCompleto.prepare(RUTA_MAPA);
    });
    while (preparacionMapa.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        const char* fases[] = { "Preparando mapa...", "Leyendo glTF / revisando cache...",
            "Creando zonas de mapa...", "Agrupando zonas / guardando indice...", "Mapa listo" };
        glfwSetWindowTitle(window, fases[modeloMapaCompleto.preparationPhase()]);
        glClearColor(0.012f, 0.018f, 0.035f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.03);
        if (glfwWindowShouldClose(window) || glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            modeloMapaCompleto.cancelPreparation();
            // Assimp no permite interrumpir ReadFile: esperamos su liberacion segura.
            try { preparacionMapa.get(); } catch (...) {}
            waterRenderer.release(); glfwTerminate(); return 0;
        }
    }
    try { preparacionMapa.get(); }
    catch (const std::exception& e) {
        std::cerr << "ERROR MAPA: " << e.what() << std::endl;
        waterRenderer.release(); glfwTerminate(); return -1;
    }

    mapaCaminata=&modeloMapaCompleto;

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

    // --- CARGAR LOS 2 VIEJITOS DE LUIS ---
    std::cout << "Cargando Viejitos Danzantes..." << std::endl;
    ViejitoRig viejito1("Models/ViejitoRig.bin", "Models/Textura_Viejito.png");
    ViejitoRig viejito2("Models/ViejitoRig.bin", "Models/Textura_Viejito.png");
    viejito1.animarPose(posturaBase());
    viejito2.animarPose(posturaBase());

    // Variables de animacion para los viejitos (se autoinicia)
    int estadoBaile1 = SALTO_IZQUIERDA;
    int estadoBaile2 = PASO_IZQUIERDO;
    float tiempoEstado1 = 0.0f;
    float tiempoEstado2 = 0.0f;
    float ritmoViejitos = 1.0f;
    int vueltasBaile = 0;
    float relojCuerpo1 = 0.0f;
    float relojCuerpo2 = 1.5f;

    if (cargarKeyframesA1(ARCHIVO_KF_A1))
        std::cout << "A1: keyframes cargados desde " << ARCHIVO_KF_A1 << std::endl;
    else
        std::cout << "A1: usando los 14 keyframes predeterminados" << std::endl;

    auto dibujar = [&](Model& m, const glm::mat4& M) {
        struct Bounds {uranden::Box box;bool valid=false;};
        static std::unordered_map<Model*,Bounds> bounds;
        auto it=bounds.find(&m);
        if(it==bounds.end()) {
            Bounds b;
            for(const auto& mesh:m.meshes)for(const auto& v:mesh.vertices){b.box.add(v.Position);b.valid=true;}
            it=bounds.emplace(&m,b).first;
        }
        if(!it->second.valid)return;
        const auto& box=it->second.box;
        glm::vec3 center=glm::vec3(M*glm::vec4((box.lo+box.hi)*0.5f,1));
        float scale=std::max({glm::length(glm::vec3(M[0])),glm::length(glm::vec3(M[1])),glm::length(glm::vec3(M[2]))});
        float radius=glm::length(box.hi-box.lo)*0.5f*scale;
        if(glm::distance(camera.Position,center)-radius>24.0f)return;
        if(!uranden::inFrustum(box,uranden::planes(vistaProyeccionAnimaciones*M)))return;
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(M));
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
         0.0f,  1.0f,        0.0f, 1.0f,
         1.0f,  0.0f,        1.0f, 0.0f,
         0.0f,  0.0f,        0.0f, 0.0f,
         0.0f,  1.0f,        0.0f, 1.0f,
         1.0f,  1.0f,        1.0f, 1.0f,
         1.0f,  0.0f,        1.0f, 0.0f
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

    lastFrame=float(glfwGetTime());lastFpsTime=glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = glm::clamp(currentFrame - lastFrame,0.0f,0.05f);
        lastFrame = currentFrame;
        int framebufferWidth=0,framebufferHeight=0;
        glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);
        if(framebufferWidth<=0||framebufferHeight<=0){glfwWaitEventsTimeout(0.05);continue;}
        glViewport(0,0,framebufferWidth,framebufferHeight);

        if (!patoIniciado) {
            tiempoInicioPato = currentFrame;
            patoIniciado = true;
        }
        float tiempoActualPato = currentFrame - tiempoInicioPato;

        glm::vec3 posPato = glm::vec3(-2.0f, -0.82f + ALTURA_AGUA + 0.88f, 4.0f);
        float rotacionPatoX = 0.0f;
        Model* modeloPatoActual = &patoNadando;

        if (tiempoActualPato < 7.0f) {
            modeloPatoActual = &patoNadando;
            posPato.y += std::sin(currentFrame * 3.0f) * 0.015f;
        }
        else if (tiempoActualPato >= 7.0f && tiempoActualPato < 10.0f) {
            modeloPatoActual = &patoPreparandoVuelo;
            posPato.y += std::sin(currentFrame * 15.0f) * 0.025f;
        }
        else {
            modeloPatoActual = &patoPreparandoVuelo;
            float tiempoVuelo = tiempoActualPato - 10.0f;
            posPato.y += 0.7f * tiempoVuelo;
            posPato.z -= 1.2f * tiempoVuelo;
            rotacionPatoX = 20.0f;
        }

        // =========================================================================
        // 1. GUARDAR LA POSICION FISICA ANTES DE MOVERSE
        // =========================================================================
        glm::vec3 posCamaraAnterior = camera.Position;
        bool caminabaAntes=!modoDron&&modoCaminataPOV;
        processInput(window);
        if(!caminabaAntes&&!modoDron&&modoCaminataPOV)reubicarCaminante=true;

        // =========================================================================
        // 2. APOYO SOBRE TRIANGULOS REALES, INDEPENDIENTE DE LO QUE SE DIBUJA
        // =========================================================================
        static float fasePaso=0.0f;
        glm::vec3 offsetVisualOjos(0.0f);
        if(!modoDron&&modoCaminataPOV) {
            const TerrainSurface& suelo=modeloMapaCompleto.collisionSurface();
            if(reubicarCaminante||!caminante.initialized()) {
                float referencia=caminante.initialized()?caminante.position().y:camera.Position.y-caminante.eyeHeight;
                if(!caminante.place(suelo,camera.Position.x,camera.Position.z,referencia)) {
                    // Sin apoyo: conserva la ultima posicion segura, no continua por el vacio.
                    camera.Position=caminante.initialized()?caminante.eye():posCamaraAnterior;
                    static bool avisado=false;
                    if(!avisado){std::cerr<<"No hay suelo bajo la camara. Usa 8 para ir al inicio del recorrido.\n";avisado=true;}
                }
                reubicarCaminante=false;
            } else caminante.move(suelo,camera.Position.x,camera.Position.z);
            if(caminante.initialized())camera.Position=caminante.eye();
            float avanceReal=glm::length(glm::vec2(camera.Position.x-posCamaraAnterior.x,camera.Position.z-posCamaraAnterior.z));
            if(avanceReal>0.0001f&&avanceReal<0.5f) {
                fasePaso+=deltaTime*7.5f;
                // El balanceo nunca baja los ojos del minimo ni desplaza la camara fuera del apoyo.
                offsetVisualOjos.y=std::abs(std::sin(fasePaso*2.0f))*0.012f;
            } else fasePaso=0.0f;
        }

        // --- VERIFICAR SI HAY ESTACIONES CERCA ---
        bool cercaDeAlguna = false;
        for (int i = 0; i < (int)estaciones.size(); i++) {
            float distancia = glm::distance(glm::vec2(camera.Position.x, camera.Position.z),
                glm::vec2(estaciones[i].posicion.x, estaciones[i].posicion.z));
            estaciones[i].interactuable = (distancia <= estaciones[i].radioDeteccion);
            if (estaciones[i].interactuable) {
                cercaDeAlguna = true;
            }
        }

        // --- LOGICA DE AVISO EN CONSOLA ---
        static bool mensajeMostrado = false;
        if (cercaDeAlguna && estacionActivaHUD == -1) {
            if (!mensajeMostrado) {
                std::cout << "\n>> [ESTACION CERCANA] Presiona la tecla 'E' para leer la informacion de la cedula." << std::endl;
                mensajeMostrado = true;
            }
        }
        else {
            mensajeMostrado = false;
        }

        // --- ACTUALIZACION DE TIEMPOS DE ANIMACION ---
        float tiempoAnim = currentFrame;
        // =========================================================================
        // LOGICA DE VUELO DEL DRON AUTOMATICO
        // =========================================================================
        if (modoDron) {
            float radioDron = 10.0f; // Que tan amplia es la vuelta circular
            float velDron = 0.12f;   // Velocidad de giro (suave para que se vea bien en video)

            // Calculamos altura con un seno para que "levite" suavemente arriba y abajo
            float alturaDron = 7.5f + 1.2f * std::sin(currentFrame * 0.4f);

            // El punto central del lago (el altar de cempasuchil)
            glm::vec3 centroObjetivo(0.0f, 0.0f, 6.0f);

            // Posicion circular automatica
            float dronX = centroObjetivo.x + std::sin(currentFrame * velDron) * radioDron;
            float dronZ = centroObjetivo.z + std::cos(currentFrame * velDron) * radioDron;
            glm::dvec2 recorrido=barreraRecorrido.mover(glm::dvec2(camera.Position.x,camera.Position.z),glm::dvec2(dronX,dronZ));
            camera.Position=glm::vec3(float(recorrido.x),alturaDron,float(recorrido.y));

            // Calcular los angulos matematicos (Yaw y Pitch) para que siempre mire al centro
            glm::vec3 direccion = glm::normalize(centroObjetivo - camera.Position);
            camera.Pitch = glm::degrees(std::asin(direccion.y));
            camera.Yaw = glm::degrees(std::atan2(direccion.z, direccion.x));

            // Aplicamos los angulos a la camara
            camera.ProcessMouseMovement(0.0f, 0.0f);
        }
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

        // --- ACTUALIZAR ESTADO DEL BAILE DE LOS VIEJITOS ---
        float deltaModificado = deltaTime * velAnimViejitos;

        relojCuerpo1 += deltaModificado * ritmoViejitos;
        relojCuerpo2 += deltaModificado * ritmoViejitos;
        tiempoEstado1 += deltaModificado * ritmoViejitos;
        tiempoEstado2 += deltaModificado * ritmoViejitos;

        if (tiempoEstado1 >= DURACION_ESTADO[estadoBaile1]) {
            tiempoEstado1 -= DURACION_ESTADO[estadoBaile1];
            if (estadoBaile1 == REMATE) {
                estadoBaile1 = SALTO_IZQUIERDA;
                ++vueltasBaile;
                ritmoViejitos = std::min(1.40f, 1.0f + 0.05f * vueltasBaile); // Aceleran poco a poco
            }
            else {
                ++estadoBaile1;
            }
        }
        if (tiempoEstado2 >= DURACION_ESTADO[estadoBaile2]) {
            tiempoEstado2 -= DURACION_ESTADO[estadoBaile2];
            if (estadoBaile2 == REMATE) {
                estadoBaile2 = SALTO_IZQUIERDA;
            }
            else {
                ++estadoBaile2;
            }
        }

        float tBaile1 = tiempoEstado1 / DURACION_ESTADO[estadoBaile1];
        float posX1 = 0.0f, posY1 = 0.0f, posZ1 = 0.0f, bastonZ1 = 0.0f, bastonX1 = 0.0f;
        PoseViejito poseV1 = calcularBaile(estadoBaile1, tBaile1, relojCuerpo1, posX1, posY1, posZ1, bastonZ1, bastonX1);
        bool visiblesViejitos=glm::distance(camera.Position,glm::vec3(3.2f,-0.05f,15.8f)+offsetViejitos)<26.0f;
        if(visiblesViejitos)viejito1.animarPose(poseV1);

        float tBaile2 = tiempoEstado2 / DURACION_ESTADO[estadoBaile2];
        float posX2 = 0.0f, posY2 = 0.0f, posZ2 = 0.0f, bastonZ2 = 0.0f, bastonX2 = 0.0f;
        PoseViejito poseV2 = calcularBaile(estadoBaile2, tBaile2, relojCuerpo2, posX2, posY2, posZ2, bastonZ2, bastonX2);
        if(visiblesViejitos)viejito2.animarPose(poseV2);


        float cicloRemo = std::fmod(tiempoAnim, 2.2f) / 2.2f;
        float impulsoRemo = std::pow(std::sin(cicloRemo * 3.14159f), 0.7f) * 0.4f;
        float avanceGlobal = (tiempoAnim * VELOCIDAD_CIRCUITO) + (impulsoRemo * 0.015f);

        int numPuntos = (int)puntosCircuito.size();
        float progreso = std::fmod(avanceGlobal, 1.0f) * numPuntos;
        int i1 = (int)progreso;
        float t = progreso - i1;

        int i0 = (i1 - 1 + numPuntos) % numPuntos;
        int i2 = (i1 + 1) % numPuntos;
        int i3 = (i1 + 2) % numPuntos;

        glm::vec3 posActual = calcularCatmullRom(puntosCircuito[i0], puntosCircuito[i1],
            puntosCircuito[i2], puntosCircuito[i3], t);

        float deltaT = 0.01f;
        glm::vec3 posFutura = calcularCatmullRom(puntosCircuito[i0], puntosCircuito[i1],
            puntosCircuito[i2], puntosCircuito[i3], t + deltaT);
        glm::vec3 direccionAvance = glm::normalize(posFutura - posActual);
        float anguloRumbo = glm::degrees(std::atan2(direccionAvance.x, direccionAvance.z));

        float bamboleoRoll = std::sin(cicloRemo * 6.28318f) * 1.5f;
        float cabeceoPitch = std::cos(cicloRemo * 6.28318f) * 0.8f;
        float flotacionOlas = std::sin(tiempoAnim * 1.0f) * 0.01f;
        // Conserva el desfase original del modelo respecto del agua (-0.65 - -0.88).
        posActual.y += (ALTURA_AGUA + 0.88f) + flotacionOlas
            + alturaOndaAgua(posActual.x, posActual.z, currentFrame * VELOCIDAD_AGUA, FUERZA_ONDAS);

        glm::vec3 posLuzCanoa = posActual + glm::vec3(0.0f, 0.18f, 0.0f);
        float parpadeoCanoa = 0.85f + 0.15f * std::sin(tiempoAnim * 14.0f + 2.5f);
        glm::vec3 colorLuzCanoa = glm::vec3(0.85f, 0.45f, 0.12f) * parpadeoCanoa;

        glm::vec3 posCanoaA1Actual=posCanoaA1;
        posCanoaA1Actual.y+=ALTURA_AGUA+0.88f;
        posCanoaA1Actual.y+=alturaOndaAgua(posCanoaA1.x,posCanoaA1.z,currentFrame*VELOCIDAD_AGUA,FUERZA_ONDAS);

        glm::vec3 fogColor = glm::vec3(0.012f, 0.018f, 0.035f);
        float fogDensity = 0.012f;

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Aplicamos temporalmente el offset visual de la zancada para generar la vista sin alterar la fisica
        glm::vec3 posFisicaGuardada = camera.Position;
        camera.Position += offsetVisualOjos;

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)framebufferWidth / (float)framebufferHeight, 0.02f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        vistaProyeccionAnimaciones=projection*view;

        // --- D. Shader Principal: Luces y Cámara ---
        miShader.use();
        glUniform3fv(ubicacionUniform(miShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 1. Luz Direccional (Luna original multiplicada por intensidad)
        glm::vec3 baseAmbient = glm::vec3(0.04f, 0.05f, 0.08f) * intensidadGlobal;
        glm::vec3 baseDiffuse = glm::vec3(0.18f, 0.20f, 0.25f) * intensidadGlobal;

        glUniform3f(ubicacionUniform(miShader.ID, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3fv(ubicacionUniform(miShader.ID, "dirLight.ambient"), 1, glm::value_ptr(baseAmbient));
        glUniform3fv(ubicacionUniform(miShader.ID, "dirLight.diffuse"), 1, glm::value_ptr(baseDiffuse));
        // El especular no lo multiplicamos tanto para que el agua no brille exageradamente
        glUniform3f(ubicacionUniform(miShader.ID, "dirLight.specular"), 0.15f, 0.15f, 0.20f);

        for (unsigned int i = 0; i < 4; i++) {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            glm::vec3 posLuzReal = pointLightOffsets[i] + offsetMuelle;
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzReal));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.0f, 0.0f, 0.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.09f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 0.032f);
        }

        {
            glm::vec4 offLocal = glm::vec4(0.0f, 0.9f, 0.8f, 1.0f);
            glm::vec3 posLuzA1 = posCanoaA1Actual + glm::vec3(
                glm::rotate(glm::mat4(1.0f), glm::radians(yawCanoaA1), glm::vec3(0.0f, 1.0f, 0.0f)) * offLocal);
            glUniform3fv(ubicacionUniform(miShader.ID, "pointLights[0].position"), 1, glm::value_ptr(posLuzA1));
            glUniform3f(ubicacionUniform(miShader.ID, "pointLights[0].ambient"), 0.03f, 0.03f, 0.03f);
            glUniform3f(ubicacionUniform(miShader.ID, "pointLights[0].diffuse"), 0.95f, 0.90f, 0.80f);
            glUniform3f(ubicacionUniform(miShader.ID, "pointLights[0].specular"), 0.20f, 0.20f, 0.20f);
            glUniform1f(ubicacionUniform(miShader.ID, "pointLights[0].constant"), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, "pointLights[0].linear"), 0.35f);
            glUniform1f(ubicacionUniform(miShader.ID, "pointLights[0].quadratic"), 0.45f);
        }

        // 3. Luz Puntual 4 (Canoa)
        glUniform3fv(ubicacionUniform(miShader.ID, "pointLights[4].position"), 1, glm::value_ptr(posLuzCanoa));
        glUniform3f(ubicacionUniform(miShader.ID, "pointLights[4].ambient"), colorLuzCanoa.r * 0.05f, colorLuzCanoa.g * 0.05f, colorLuzCanoa.b * 0.05f);
        glUniform3f(ubicacionUniform(miShader.ID, "pointLights[4].diffuse"), colorLuzCanoa.r, colorLuzCanoa.g, colorLuzCanoa.b);
        glUniform3f(ubicacionUniform(miShader.ID, "pointLights[4].specular"), colorLuzCanoa.r * 0.6f, colorLuzCanoa.g * 0.6f, colorLuzCanoa.b * 0.6f);
        glUniform1f(ubicacionUniform(miShader.ID, "pointLights[4].constant"), 1.0f);
        glUniform1f(ubicacionUniform(miShader.ID, "pointLights[4].linear"), 0.18f);
        glUniform1f(ubicacionUniform(miShader.ID, "pointLights[4].quadratic"), 0.10f);

        for (unsigned int i = 0; i < 5; i++) {
            int lightIndex = 5 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(postesPlaza[i]));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.005f, 0.005f, 0.005f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.68f, 0.62f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.25f, 0.25f, 0.22f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.60f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 1.40f);
        }

        for (unsigned int i = 0; i < 6; i++) {
            int lightIndex = 10 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesOrilla[i]));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.010f, 0.003f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.75f, 0.28f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.32f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 0.38f);
        }

        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 16 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesLetrero[i]));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.015f, 0.005f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 1.10f, 0.70f, 0.25f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.70f, 0.45f, 0.18f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.22f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 0.20f);
        }

        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 18 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            float tP = (i == 0) ? 0.35f : 0.65f;
            glm::vec3 pIzq = glm::mix(postesPuente[0], postesPuente[1], tP);
            glm::vec3 pDer = glm::mix(postesPuente[2], postesPuente[3], tP);
            glm::vec3 posLuzPuente = (pIzq + pDer) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzPuente));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.008f, 0.001f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.32f, 0.05f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.30f, 0.15f, 0.03f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.70f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 1.80f);
        }

        for (unsigned int i = 0; i < 4; i++) {
            int lightIndex = 20 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(lucesManantialOpuesto[i]));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.010f, 0.003f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 1.25f, 0.78f, 0.28f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.32f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 0.38f);
        }

        // --- 12. LUCES MAESTRAS DEL SEGUNDO PUENTE (Índices 48 y 49) ---
        for (unsigned int i = 0; i < 2; i++) {
            int lightIndex = 48 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            float tP = (i == 0) ? 0.35f : 0.65f;
            glm::vec3 pIzq = glm::mix(postesPuente2[0], postesPuente2[1], tP);
            glm::vec3 pDer = glm::mix(postesPuente2[2], postesPuente2[3], tP);
            glm::vec3 posLuzPuente = (pIzq + pDer) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(posLuzPuente));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.015f, 0.008f, 0.001f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 0.70f, 0.32f, 0.05f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.30f, 0.15f, 0.03f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.70f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 1.80f);
        }

        // FOCOS ORIGINALES DEL LETRERO URANDÉN (Índices 40 al 43)
        for (unsigned int i = 0; i < 4; i++) {
            int lightIndex = 40 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(focosUranden[i].pos));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.01f, 0.00f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.65f, 0.20f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.22f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 0.20f);
        }

        // NUEVOS FOCOS EN FRENTE DE LAS LETRAS (Índices 44 al 47)
        for (unsigned int i = 0; i < focosLetras.size(); i++) {
            int lightIndex = 44 + i;
            std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (base + "position").c_str()), 1, glm::value_ptr(focosLetras[i].pos));
            glUniform3f(ubicacionUniform(miShader.ID, (base + "ambient").c_str()), 0.02f, 0.01f, 0.00f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "diffuse").c_str()), 1.20f, 0.65f, 0.20f);
            glUniform3f(ubicacionUniform(miShader.ID, (base + "specular").c_str()), 0.50f, 0.35f, 0.15f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "linear").c_str()), 0.22f);
            glUniform1f(ubicacionUniform(miShader.ID, (base + "quadratic").c_str()), 0.20f);
        }

        glm::vec3 spotLightPos = glm::vec3(0.0f, 3.5f, 1.0f);
        glm::vec3 spotLightDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        glUniform3fv(ubicacionUniform(miShader.ID, "spotLight.position"), 1, glm::value_ptr(spotLightPos));
        glUniform3fv(ubicacionUniform(miShader.ID, "spotLight.direction"), 1, glm::value_ptr(spotLightDir));
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.cutOff"), glm::cos(glm::radians(18.0f)));
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.outerCutOff"), glm::cos(glm::radians(24.0f)));
        glUniform3f(ubicacionUniform(miShader.ID, "spotLight.ambient"), 0.0f, 0.0f, 0.0f);
        glUniform3f(ubicacionUniform(miShader.ID, "spotLight.diffuse"), 0.75f, 0.75f, 0.80f);
        glUniform3f(ubicacionUniform(miShader.ID, "spotLight.specular"), 0.40f, 0.40f, 0.45f);
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.constant"), 1.0f);
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.linear"), 0.14f);
        glUniform1f(ubicacionUniform(miShader.ID, "spotLight.quadratic"), 0.07f);

        glUniform3fv(ubicacionUniform(miShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(ubicacionUniform(miShader.ID, "fogDensity"), fogDensity);

        std::array<glm::mat4,NUM_CANOAS_PROC> matricesProcesion,matricesCopaleras;
        for (int k = 0; k < NUM_CANOAS_PROC; k++) {
            glm::vec3 posK = posProc[k];
            float yawK = yawProc[k];

            float faseK = tiempoAnim + (k * 0.5f);
            float bamboleoK = std::sin(faseK * 2.85f) * 1.2f;
            float cabeceoK = std::cos(faseK * 2.85f) * 0.6f;

            // --- Ajuste de altura ---
            posK.y += std::sin(tiempoAnim * 1.2f + (k * 0.7f)) * 0.008f;
            posK.y += alturaCanoasProcesion + ALTURA_AGUA + 0.88f;
            posK.y += alturaOndaAgua(posK.x,posK.z,currentFrame*VELOCIDAD_AGUA,FUERZA_ONDAS);

            glm::mat4 canoaMat = glm::mat4(1.0f);
            canoaMat = glm::translate(canoaMat, posK);
            canoaMat = glm::rotate(canoaMat, glm::radians(yawK), glm::vec3(0.0f, 1.0f, 0.0f));
            canoaMat = glm::rotate(canoaMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            canoaMat = glm::rotate(canoaMat, glm::radians(bamboleoK), glm::vec3(0.0f, 0.0f, 1.0f));
            canoaMat = glm::rotate(canoaMat, glm::radians(cabeceoK), glm::vec3(1.0f, 0.0f, 0.0f));
            canoaMat = glm::scale(canoaMat, glm::vec3(escalaCanoaProcesion));

            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaMat));
            matricesProcesion[k]=canoaMat;

            glm::mat4 copaleraMat = canoaMat;
            copaleraMat = glm::translate(copaleraMat, offsetCopalera);
            copaleraMat = glm::rotate(copaleraMat, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            copaleraMat = glm::scale(copaleraMat, glm::vec3(escalaCopalera));

            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(copaleraMat));
            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
            matricesCopaleras[k]=copaleraMat;

            glm::vec3 posSahumador = glm::vec3(copaleraMat * glm::vec4(offsetFuegoSahumador, 1.0f));
            int luzIndexSahumador = 25 + k;
            std::string baseSah = "pointLights[" + std::to_string(luzIndexSahumador) + "].";

            float parpadeoBrasa = 0.6f + 0.4f * std::sin(tiempoAnim * 28.0f + k * 7.0f) * std::cos(tiempoAnim * 15.0f + k);
            glm::vec3 colorBrasa = glm::vec3(1.0f, 0.30f, 0.05f) * parpadeoBrasa;

            glUniform3fv(ubicacionUniform(miShader.ID, (baseSah + "position").c_str()), 1, glm::value_ptr(posSahumador));
            glUniform3f(ubicacionUniform(miShader.ID, (baseSah + "ambient").c_str()), colorBrasa.r * 0.05f, colorBrasa.g * 0.05f, colorBrasa.b * 0.05f);
            glUniform3f(ubicacionUniform(miShader.ID, (baseSah + "diffuse").c_str()), colorBrasa.r, colorBrasa.g, colorBrasa.b);
            glUniform3f(ubicacionUniform(miShader.ID, (baseSah + "specular").c_str()), colorBrasa.r * 0.5f, colorBrasa.g * 0.5f, colorBrasa.b * 0.5f);
            glUniform1f(ubicacionUniform(miShader.ID, (baseSah + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (baseSah + "linear").c_str()), 2.5f);
            glUniform1f(ubicacionUniform(miShader.ID, (baseSah + "quadratic").c_str()), 4.5f);

            glm::vec3 posLuzProa = glm::vec3(canoaMat * glm::vec4(offsetLuzProa, 1.0f));
            glm::vec3 posLuzPopa = glm::vec3(canoaMat * glm::vec4(offsetLuzPopa, 1.0f));

            int idxProa = 30 + (k * 2);
            int idxPopa = 31 + (k * 2);

            float parpadeoVela = 0.8f + 0.2f * std::sin(tiempoAnim * 10.0f + k * 3.14f);
            glm::vec3 colorVela = glm::vec3(0.9f, 0.6f, 0.2f) * parpadeoVela;

            std::string baseProa = "pointLights[" + std::to_string(idxProa) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (baseProa + "position").c_str()), 1, glm::value_ptr(posLuzProa));
            glUniform3f(ubicacionUniform(miShader.ID, (baseProa + "ambient").c_str()), colorVela.r * 0.05f, colorVela.g * 0.05f, colorVela.b * 0.05f);
            glUniform3f(ubicacionUniform(miShader.ID, (baseProa + "diffuse").c_str()), colorVela.r, colorVela.g, colorVela.b);
            glUniform3f(ubicacionUniform(miShader.ID, (baseProa + "specular").c_str()), colorVela.r * 0.3f, colorVela.g * 0.3f, colorVela.b * 0.3f);
            glUniform1f(ubicacionUniform(miShader.ID, (baseProa + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (baseProa + "linear").c_str()), 1.2f);
            glUniform1f(ubicacionUniform(miShader.ID, (baseProa + "quadratic").c_str()), 1.8f);

            std::string basePopa = "pointLights[" + std::to_string(idxPopa) + "].";
            glUniform3fv(ubicacionUniform(miShader.ID, (basePopa + "position").c_str()), 1, glm::value_ptr(posLuzPopa));
            glUniform3f(ubicacionUniform(miShader.ID, (basePopa + "ambient").c_str()), colorVela.r * 0.05f, colorVela.g * 0.05f, colorVela.b * 0.05f);
            glUniform3f(ubicacionUniform(miShader.ID, (basePopa + "diffuse").c_str()), colorVela.r, colorVela.g, colorVela.b);
            glUniform3f(ubicacionUniform(miShader.ID, (basePopa + "specular").c_str()), colorVela.r * 0.3f, colorVela.g * 0.3f, colorVela.b * 0.3f);
            glUniform1f(ubicacionUniform(miShader.ID, (basePopa + "constant").c_str()), 1.0f);
            glUniform1f(ubicacionUniform(miShader.ID, (basePopa + "linear").c_str()), 1.2f);
            glUniform1f(ubicacionUniform(miShader.ID, (basePopa + "quadratic").c_str()), 1.8f);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUniform1i(ubicacionUniform(miShader.ID, "useTexture"), true);
        glUniform1i(ubicacionUniform(miShader.ID, "isLightSource"), false);
        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(ubicacionUniform(miShader.ID, "materialAlpha"), 1.0f);

        glm::mat4 mapaModel = glm::mat4(1.0f);
        mapaModel = glm::translate(mapaModel, DESPLAZAMIENTO_MAPA);
        mapaModel = glm::scale(mapaModel, glm::vec3(ESCALA_MAPA));
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(mapaModel));
        modeloMapaCompleto.draw(miShader.ID,mapaModel,view,projection,camera.Position,framebufferHeight,glfwGetTime());
        estadisticasMapa=modeloMapaCompleto.statistics();
        glm::mat4 matrizPato=glm::translate(glm::mat4(1.0f),posPato);
        matrizPato=glm::rotate(matrizPato,glm::radians(rotacionPatoX),glm::vec3(1,0,0));
        matrizPato=glm::scale(matrizPato,glm::vec3(0.25f));
        dibujar(*modeloPatoActual,matrizPato);
        // Canoa
        glm::mat4 canoaModel = glm::mat4(1.0f);
        canoaModel = glm::translate(canoaModel, posActual);
        canoaModel = glm::rotate(canoaModel, glm::radians(anguloRumbo), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(bamboleoRoll), glm::vec3(0.0f, 0.0f, 1.0f));
        canoaModel = glm::rotate(canoaModel, glm::radians(cabeceoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        canoaModel = glm::scale(canoaModel, glm::vec3(2.0f));
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(canoaModel));
        dibujar(modeloCanoa,canoaModel);



        for(int k=0;k<NUM_CANOAS_PROC;++k){dibujar(modeloCanoaPro,matricesProcesion[k]);dibujar(modeloCopalera,matricesCopaleras[k]);}

        glm::mat4 canoaA1 = glm::translate(glm::mat4(1.0f), posCanoaA1Actual);
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

        // --- DIBUJAR A LOS 2 VIEJITOS EN LA PLAZA (ESTACION 2) ---
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);

        // Viejito 1 (Lado izquierdo de la plaza)
        glm::mat4 modeloViejito1 = glm::mat4(1.0f);
        modeloViejito1 = glm::translate(modeloViejito1, glm::vec3(2.5f, -0.05f, 15.6f) + offsetViejitos);
        modeloViejito1 = glm::translate(modeloViejito1, glm::vec3(posX1, posY1, posZ1));
        modeloViejito1 = glm::rotate(modeloViejito1, glm::radians(-45.0f + rotacionViejitos), glm::vec3(0.0f, 1.0f, 0.0f));
        modeloViejito1 = glm::scale(modeloViejito1, glm::vec3(escalaViejitos));
        if(visiblesViejitos)viejito1.dibujar(modeloViejito1, view, projection);

        // Viejito 2 (Lado derecho de la plaza)
        glm::mat4 modeloViejito2 = glm::mat4(1.0f);
        modeloViejito2 = glm::translate(modeloViejito2, glm::vec3(3.2f, -0.05f, 16.0f) + offsetViejitos);
        modeloViejito2 = glm::translate(modeloViejito2, glm::vec3(posX2, posY2, posZ2));
        modeloViejito2 = glm::rotate(modeloViejito2, glm::radians(-90.0f + rotacionViejitos), glm::vec3(0.0f, 1.0f, 0.0f));
        modeloViejito2 = glm::scale(modeloViejito2, glm::vec3(escalaViejitos));
        if(visiblesViejitos)viejito2.dibujar(modeloViejito2, view, projection);

        // Volver al shader normal para dibujar los bastones de los viejitos
        miShader.use();

        const glm::vec3 agarreDedosReposo(-0.351f, 0.765f, 0.126f);
        const glm::vec3 puntoEnBaston(-0.045f, 0.48f, -0.015f);

        // --- Baston Viejito 1 ---
        glm::vec3 agarreAnimado1 = glm::vec3(viejito1.movimientoManoDerecha() * glm::vec4(agarreDedosReposo, 1.0f));
        glm::mat4 modeloBaston1 = glm::translate(modeloViejito1, agarreAnimado1);
        float inclinacionZ1 = 8.0f + 1.5f * std::sin(relojCuerpo1 * 2.1f) + glm::clamp(bastonZ1 * 0.16f, -4.0f, 4.0f);
        float inclinacionX1 = glm::clamp(bastonX1 * 0.14f, -3.5f, 3.5f);
        modeloBaston1 = glm::rotate(modeloBaston1, glm::radians(inclinacionZ1), glm::vec3(0, 0, 1));
        modeloBaston1 = glm::rotate(modeloBaston1, glm::radians(inclinacionX1), glm::vec3(1, 0, 0));
        modeloBaston1 = glm::scale(modeloBaston1, glm::vec3(factorEscalaBaston));
        modeloBaston1 = glm::translate(modeloBaston1, -puntoEnBaston);
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(modeloBaston1));
        dibujar(bastonViejito,modeloBaston1);

        // --- Baston Viejito 2 ---
        glm::vec3 agarreAnimado2 = glm::vec3(viejito2.movimientoManoDerecha() * glm::vec4(agarreDedosReposo, 1.0f));
        glm::mat4 modeloBaston2 = glm::translate(modeloViejito2, agarreAnimado2);
        float inclinacionZ2 = 8.0f + 1.5f * std::sin(relojCuerpo2 * 2.1f) + glm::clamp(bastonZ2 * 0.16f, -4.0f, 4.0f);
        float inclinacionX2 = glm::clamp(bastonX2 * 0.14f, -3.5f, 3.5f);
        modeloBaston2 = glm::rotate(modeloBaston2, glm::radians(inclinacionZ2), glm::vec3(0, 0, 1));
        modeloBaston2 = glm::rotate(modeloBaston2, glm::radians(inclinacionX2), glm::vec3(1, 0, 0));
        modeloBaston2 = glm::scale(modeloBaston2, glm::vec3(factorEscalaBaston));
        modeloBaston2 = glm::translate(modeloBaston2, -puntoEnBaston);
        glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(modeloBaston2));
        dibujar(bastonViejito,modeloBaston2);

        // --- Render Focos Físicos (Geometría) ---
        glUniform1i(ubicacionUniform(miShader.ID, "useTexture"), false);
        glUniform1i(ubicacionUniform(miShader.ID, "isLightSource"), true);
        glUniform1f(ubicacionUniform(miShader.ID, "fogDensity"), 0.0f);

        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.0f, 0.98f, 0.90f);
        for (unsigned int i = 0; i < postesPlaza.size(); i++) {
            glm::mat4 focoModel = glm::mat4(1.0f);
            focoModel = glm::translate(focoModel, postesPlaza[i]);
            focoModel = glm::scale(focoModel, glm::vec3(0.009f));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
        for (unsigned int i = 0; i < lucesOrilla.size(); i++) {
            glm::mat4 focoOrillaModel = glm::mat4(1.0f);
            focoOrillaModel = glm::translate(focoOrillaModel, lucesOrilla[i]);
            focoOrillaModel = glm::scale(focoOrillaModel, glm::vec3(0.019f));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoOrillaModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        for (unsigned int i = 0; i < lucesManantialOpuesto.size(); i++) {
            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            glm::mat4 focoOpuestoModel = glm::mat4(1.0f);
            focoOpuestoModel = glm::translate(focoOpuestoModel, lucesManantialOpuesto[i]);
            focoOpuestoModel = glm::scale(focoOpuestoModel, glm::vec3(0.019f));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoOpuestoModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // DIBUJAR LOS CUBOS DE URANDÉN ORIGINALES ROTADOS
        for (unsigned int i = 0; i < focosUranden.size(); i++) {
            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            glm::mat4 focoUrandenModel = glm::mat4(1.0f);
            focoUrandenModel = glm::translate(focoUrandenModel, focosUranden[i].pos);
            focoUrandenModel = glm::rotate(focoUrandenModel, glm::radians(focosUranden[i].yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            focoUrandenModel = glm::rotate(focoUrandenModel, glm::radians(focosUranden[i].pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            focoUrandenModel = glm::scale(focoUrandenModel, glm::vec3(0.035f));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoUrandenModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // DIBUJAR LOS NUEVOS CUBOS DE LAS LETRAS ROTADOS
        for (unsigned int i = 0; i < focosLetras.size(); i++) {
            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.15f, 0.75f, 0.25f);
            glm::mat4 focoLetrasModel = glm::mat4(1.0f);
            focoLetrasModel = glm::translate(focoLetrasModel, focosLetras[i].pos);
            focoLetrasModel = glm::rotate(focoLetrasModel, glm::radians(focosLetras[i].yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            focoLetrasModel = glm::rotate(focoLetrasModel, glm::radians(focosLetras[i].pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            focoLetrasModel = glm::scale(focoLetrasModel, glm::vec3(0.035f));
            glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(focoLetrasModel));
            glBindVertexArray(skyboxVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        // --- HILERA DE FOQUITOS: PRIMER PUENTE ---
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

            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 0.03f, 0.03f, 0.03f);
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

                glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(cableModel));
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }

            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 4.5f, 2.2f, 0.4f);
            for (int i = 0; i < numFocosHilera; i++) {
                glm::mat4 foquitoModel = glm::mat4(1.0f);
                foquitoModel = glm::translate(foquitoModel, puntosCuerda[i]);
                foquitoModel = glm::scale(foquitoModel, glm::vec3(0.007f));
                glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(foquitoModel));
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }

        // --- HILERA DE FOQUITOS: SEGUNDO PUENTE ---
        int numFocosHilera2 = 12;
        for (int cuerda = 0; cuerda < 2; cuerda++) {
            glm::vec3 pA = (cuerda == 0) ? postesPuente2[0] : postesPuente2[2];
            glm::vec3 pB = (cuerda == 0) ? postesPuente2[1] : postesPuente2[3];

            std::vector<glm::vec3> puntosCuerda(numFocosHilera2);
            for (int i = 0; i < numFocosHilera2; i++) {
                float tC = (float)i / (float)(numFocosHilera2 - 1);
                glm::vec3 p = glm::mix(pA, pB, tC);
                p.y += std::sin(tC * 3.14159f) * 0.1f;
                puntosCuerda[i] = p;
            }

            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 0.03f, 0.03f, 0.03f);
            for (int i = 0; i < numFocosHilera2 - 1; i++) {
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

                glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(cableModel));
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }

            glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 4.5f, 2.2f, 0.4f);
            for (int i = 0; i < numFocosHilera2; i++) {
                glm::mat4 foquitoModel = glm::mat4(1.0f);
                foquitoModel = glm::translate(foquitoModel, puntosCuerda[i]);
                foquitoModel = glm::scale(foquitoModel, glm::vec3(0.007f));
                glUniformMatrix4fv(ubicacionUniform(miShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(foquitoModel));
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }

        glUniform1i(ubicacionUniform(miShader.ID, "isLightSource"), false);
        glUniform1i(ubicacionUniform(miShader.ID, "useTexture"), true);
        glUniform3f(ubicacionUniform(miShader.ID, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(ubicacionUniform(miShader.ID, "fogDensity"), fogDensity);

        // --- F. Cielo ANTES del agua para incluirlo en la captura ---
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(ubicacionUniform(skyboxShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(skyboxView));
        glUniformMatrix4fv(ubicacionUniform(skyboxShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // --- G. Agua: escena copiada sin volver a dibujar el mapa ---
        if (mostrarAgua) {
        waterRenderer.capture(framebufferWidth, framebufferHeight);
        std::vector<LuzAgua> lucesAgua;
        lucesAgua.reserve(20);
        lucesAgua.push_back({ posLuzCanoa, colorLuzCanoa });
        for (const auto& p : postesPlaza)
            lucesAgua.push_back({ p, glm::vec3(0.70f, 0.68f, 0.62f) });
        for (const auto& p : lucesOrilla)
            lucesAgua.push_back({ p, glm::vec3(1.20f, 0.75f, 0.28f) });
        for (const auto& p : lucesManantialOpuesto)
            lucesAgua.push_back({ p, glm::vec3(1.25f, 0.78f, 0.28f) });
        for (const auto& p : lucesLetrero)
            lucesAgua.push_back({ p, glm::vec3(1.10f, 0.70f, 0.25f) });
        for (int i = 0; i < 2; ++i) {
            float tPuente = i == 0 ? 0.35f : 0.65f;
            glm::vec3 p = (glm::mix(postesPuente[0], postesPuente[1], tPuente)
                + glm::mix(postesPuente[2], postesPuente[3], tPuente)) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);
            lucesAgua.push_back({ p, glm::vec3(0.70f, 0.32f, 0.05f) });
        }
        for(const auto& f:focosUranden)lucesAgua.push_back({f.pos,glm::vec3(1.2f,0.65f,0.2f)});
        for(const auto& f:focosLetras)lucesAgua.push_back({f.pos,glm::vec3(1.2f,0.65f,0.2f)});
        for(const auto& p:postesPuente2)lucesAgua.push_back({p,glm::vec3(0.7f,0.32f,0.05f)});
        for(const auto& m:matricesProcesion)lucesAgua.push_back({glm::vec3(m*glm::vec4(offsetLuzProa,1)),glm::vec3(0.9f,0.6f,0.2f)});
        // Menos reflejos puntuales por pixel en modo ligero.
        std::sort(lucesAgua.begin(), lucesAgua.end(), [](const LuzAgua& a, const LuzAgua& b) {
            glm::vec3 da = a.posicion - camera.Position, db = b.posicion - camera.Position;
            return glm::dot(da, da) < glm::dot(db, db);
        });
        std::size_t maxLucesAgua = calidadAgua == 0 ? 8u : calidadAgua == 1 ? 12u : 20u;
        if (lucesAgua.size() > maxLucesAgua) lucesAgua.resize(maxLucesAgua);
        waterShader.use();
        waterRenderer.bind(waterShader.ID, cubemapTexture, lucesAgua);
        glm::mat4 inverseProjection = glm::inverse(projection);
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "inverseProjection"), 1, GL_FALSE, glm::value_ptr(inverseProjection));
        glUniform3fv(ubicacionUniform(waterShader.ID, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3f(ubicacionUniform(waterShader.ID, "dirLightDir"), -0.2f, -1.0f, -0.3f);
        glUniform1f(ubicacionUniform(waterShader.ID, "time"), currentFrame * VELOCIDAD_AGUA);
        glUniform1f(ubicacionUniform(waterShader.ID, "waveStrength"), FUERZA_ONDAS);
        glUniform1f(ubicacionUniform(waterShader.ID, "waterLevel"), ALTURA_AGUA);
        glUniform1f(ubicacionUniform(waterShader.ID, "clarity"), CLARIDAD_AGUA);
        glUniform1f(ubicacionUniform(waterShader.ID, "reflectionStrength"), REFLEJO_AGUA);
        glUniform1i(ubicacionUniform(waterShader.ID, "reflectionSteps"), calidadAgua == 0 ? 0 : calidadAgua == 1 ? 20 : 40);
        glUniform3fv(ubicacionUniform(waterShader.ID, "fogColor"), 1, glm::value_ptr(fogColor));
        glUniform1f(ubicacionUniform(waterShader.ID, "fogDensity"), fogDensity);
        glm::mat4 waterModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, ALTURA_AGUA, 0.0f));
        glUniformMatrix4fv(ubicacionUniform(waterShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(waterModel));
        glDisable(GL_BLEND); // La transparencia ya esta calculada con color/profundidad.
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)waterIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);

        } // mostrarAgua

        // --- RENDERIZADO DEL HUD (MOCKUPS) ---
        if (estacionActivaHUD != -1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);

            hudShader.use();
            glUniform1i(ubicacionUniform(hudShader.ID, "hudTexture"), 0);
            glm::mat4 orthoProj = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
            hudShader.setMat4("projection", orthoProj);

            glm::mat4 orthoModel = glm::mat4(1.0f);
            orthoModel = glm::translate(orthoModel, glm::vec3(80.0f, 120.0f, 0.0f));
            orthoModel = glm::scale(orthoModel, glm::vec3(640.0f, 360.0f, 1.0f));
            hudShader.setMat4("model", orthoModel);

            glActiveTexture(GL_TEXTURE0);

            if (estacionActivaHUD == 0) glBindTexture(GL_TEXTURE_2D, texCedula1);
            else if (estacionActivaHUD == 1) glBindTexture(GL_TEXTURE_2D, texCedula4);
            else if (estacionActivaHUD == 2) glBindTexture(GL_TEXTURE_2D, texCedula2);
            else if (estacionActivaHUD == 3) glBindTexture(GL_TEXTURE_2D, texCedula3);

            glBindVertexArray(hudVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }

        // Restauramos la posicion fisica limpia de la camara para el siguiente cuadro
        camera.Position = posFisicaGuardada;

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

    mapaCaminata=nullptr;
    modeloMapaCompleto.release();waterRenderer.release();
    audioEngine.cleanup();
    } // Destruye modelos y rigs con contexto GL activo.
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}