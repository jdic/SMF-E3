/**
 * @file station_types.hpp
 * @brief Catalogo de codigos de error, enumeraciones de estados, comandos tactiles y estructuras de datos.
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Taxonomia numerica de codigos de error para diagnostico local y SCADA.
 */
enum class ErrorCode : uint16_t
{
    None                    = 0,

    // 100s: Sensores y Presencia de Botella
    FillBottleLost          = 101, // ERR-101: Botella no detectada en llenado tras bajar los brazos
    CapBottleLost           = 102, // ERR-102: Botella no detectada en tapado tras bajar los brazos

    // 200s: Cinematica y Movimiento - Eje Z de Llenado (201-209)
    FillZHomeFailed         = 201, // ERR-201: Eje Z de llenado no hallo fin de carrera
    FillZSoftLimitHit       = 202, // ERR-202: Cota maxima de llenado rebasada (Soft limit)
    FillZLoweringTimeout    = 203, // ERR-203: Boquilla no alcanzo la cota de llenado a tiempo
    FillZRaisingTimeout     = 204, // ERR-204: Boquilla no alcanzo el fin de carrera superior a tiempo

    // 200s: Cinematica y Movimiento - Eje Z de Tapado (211-219)
    CapZHomeFailed          = 211, // ERR-211: Eje Z de tapado no hallo fin de carrera
    CapZSoftLimitHit        = 212, // ERR-212: Cota maxima de tapado rebasada (Soft limit)
    CapZLoweringTimeout     = 213, // ERR-213: Cabezal no alcanzo la cota de tapado a tiempo
    CapZRaisingTimeout      = 214, // ERR-214: Cabezal no alcanzo el fin de carrera superior a tiempo

    // 300s: Apriete, Banda y Potencia
    ChuckOvercurrent        = 301, // ERR-301: Corriente excesiva en motor de apriete
    ChuckStallPremature     = 302, // ERR-302: Bloqueo prematuro (rosca trasroscada)
    ConveyorStall           = 303, // ERR-303: Banda atascada (PWM aplicado y encoder sin pulsos)

    // 900s: Seguridad y Red
    EmergencyStopPressed    = 901, // ERR-901: Parada de emergencia por hardware activada
    NetworkLinkLost         = 902  // ERR-902: Perdida prolongada de enlace con SCADA
};

/**
 * @brief Modo global de la estacion. Un error en cualquier automata detiene ambos.
 */
enum class StationMode : uint8_t
{
    Boot,           // Homing de ambos ejes Z; brazos arriba; bomba y banda apagadas
    Running,        // Banda en giro continuo (lazo PI); ambos automatas operan en paralelo
    ErrorStop,      // Bomba, PWM y drivers deshabilitados
    ManualRecovery  // Comandos manuales por pantalla tactil
};

/**
 * @brief Perfil de botella (dos tamanos). Define cotas Z y tiempo de llenado.
 */
enum class BottleProfile : uint8_t
{
    Small = 0,
    Large = 1
};

/**
 * @brief Estados del Automata 1 (Llenado).
 */
enum class FillState : uint8_t
{
    Idle,
    StopArmDown,    // Baja el brazo de tope; espera CLAMP_DELAY_MS (1 s) con la banda girando
    ClampArmDown,   // Baja el brazo de sujecion y confirma presencia
    LoweringNozzle,
    Filling,
    Draining,       // Espera de goteo con la bomba apagada
    RaisingNozzle,
    ReleasingBottle // Sube brazo de sujecion y despues el de tope
};

/**
 * @brief Estados del Automata 2 (Tapado).
 */
enum class CappingState : uint8_t
{
    Idle,
    StopArmDown,
    ClampArmDown,
    LoweringHead,
    TighteningCap,
    RaisingHead,
    ReleasingBottle
};

/**
 * @brief Comandos originados por los botones tactiles de la pantalla TFT.
 * Todo comando se descarta mientras la seta E-STOP siga activa (PIN_ESTOP_AUX).
 */
enum class TouchCommand : uint8_t
{
    None = 0,
    CycleBottleProfile, // En Running con ambos automatas en Idle: alterna Small/Large
    AcknowledgeAlarm,   // En ErrorStop: pasar a ManualRecovery
    JogFillZUp,         // En ManualRecovery: subir eje Z de llenado por pasos seguros
    JogCapZUp,          // En ManualRecovery: subir eje Z de tapado por pasos seguros
    ReleaseArms,        // En ManualRecovery: subir ambos pares de brazos
    ConfirmLineClear    // En ManualRecovery: confirmar despeje y volver a Boot
};

/**
 * @brief Instantanea del estado a proyectar en la pantalla TFT.
 * DisplayManager traduce cada enum a su texto en espanol desde tablas PROGMEM.
 */
struct DisplaySnapshot
{
    ErrorCode     error;
    StationMode   mode;
    FillState     fillState;
    CappingState  capState;
    BottleProfile profile;
    uint32_t      bottleCount;
};

/**
 * @brief Instantanea de telemetria enviada periodicamente al SCADA (Node-RED) via MQTT.
 */
struct TelemetrySnapshot
{
    ErrorCode     error;
    StationMode   mode;
    FillState     fillState;
    CappingState  capState;
    BottleProfile profile;
    uint16_t      beltRpm;
    uint32_t      bottleCount;
    bool          presenceFill;
    bool          presenceCap;
    bool          estopActive;
};
