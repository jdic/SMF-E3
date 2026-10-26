/**
 * @file station_types.hpp
 * @brief Catálogo de códigos de error, enumeraciones de estados, comandos táctiles y estructuras de datos.
 */

#pragma once
#include <Arduino.h>

/**
 * @enum ErrorCode
 * @brief Taxonomía numérica de códigos de error para diagnóstico en pantalla TFT y SCADA.
 */
enum class ErrorCode : uint16_t
{
    None                    = 0,   ///< Sin errores activos (operación nominal)

    // 100s: Sensores y Presencia de Botella
    FillBottleLost          = 101, ///< ERR-101: Botella no detectada en llenado tras bajar los brazos
    CapBottleLost           = 102, ///< ERR-102: Botella no detectada en tapado tras bajar los brazos

    // 200s: Cinemática y Movimiento - Eje Z de Llenado (201-209)
    FillZHomeFailed         = 201, ///< ERR-201: Eje Z de llenado no halló fin de carrera en Boot
    FillZSoftLimitHit       = 202, ///< ERR-202: Cota máxima de llenado rebasada (Soft limit en ISR)
    FillZLoweringTimeout    = 203, ///< ERR-203: Boquilla no alcanzó la cota de llenado a tiempo
    FillZRaisingTimeout     = 204, ///< ERR-204: Boquilla no alcanzó el fin de carrera superior a tiempo

    // 200s: Cinemática y Movimiento - Eje Z de Tapado (211-219)
    CapZHomeFailed          = 211, ///< ERR-211: Eje Z de tapado no halló fin de carrera en Boot
    CapZSoftLimitHit        = 212, ///< ERR-212: Cota máxima de tapado rebasada (Soft limit en ISR)
    CapZLoweringTimeout     = 213, ///< ERR-213: Cabezal no alcanzó la cota de tapado a tiempo
    CapZRaisingTimeout      = 214, ///< ERR-214: Cabezal no alcanzó el fin de carrera superior a tiempo

    // 300s: Apriete, Banda y Potencia
    ChuckOvercurrent        = 301, ///< ERR-301: Corriente excesiva en motor DC de apriete
    ChuckStallPremature     = 302, ///< ERR-302: Bloqueo prematuro del cabezal por corriente (rosca trasroscada)
    ConveyorStall           = 303, ///< ERR-303: Sobrecorriente sostenida / atasco en motor DC de la banda

    // 900s: Seguridad y Red
    EmergencyStopPressed    = 901, ///< ERR-901: Parada de emergencia por hardware activada
    NetworkLinkLost         = 902  ///< ERR-902: Pérdida prolongada de enlace con SCADA (advertencia no bloqueante)
};

/**
 * @enum StationMode
 * @brief Modo operativo global gobernado por StationController. Un fallo crítico detiene ambos autómatas.
 */
enum class StationMode : uint8_t
{
    Boot,           ///< Homing simultáneo de ambos ejes Z; brazos arriba; bomba y banda apagadas
    Running,        ///< Banda en giro continuo por PWM; ambos autómatas operan en paralelo
    ErrorStop,      ///< Parada segura: bomba apagada, PWM a cero y drivers deshabilitados
    ManualRecovery  ///< Asistencia manual por pantalla táctil para despeje de línea
};

/**
 * @enum BottleProfile
 * @brief Formato de botella activo. Selecciona cotas verticales Z y tiempo de bombeo en BOTTLE_PROFILES.
 */
enum class BottleProfile : uint8_t
{
    Small = 0, ///< Formato de botella pequeña
    Large = 1  ///< Formato de botella grande
};

/**
 * @enum FillState
 * @brief Estados de la máquina de tres fases del Autómata 1 (Llenado).
 */
enum class FillState : uint8_t
{
    Idle,             ///< Brazos arriba y bomba apagada; aguardando flanco/nivel de presencia
    StopArmDown,      ///< Baja el brazo de tope; espera CLAMP_DELAY_MS con la banda en movimiento
    ClampArmDown,     ///< Baja el brazo de sujeción y valida permanencia de la botella
    LoweringNozzle,   ///< Desciende el eje Z de llenado hasta la cota del perfil activo
    Filling,          ///< Energiza la bomba de llenado durante fillMs del perfil activo
    Draining,         ///< Espera DRIP_MS con la bomba apagada para escurrimiento de boquilla
    RaisingNozzle,    ///< Asciende el eje Z de llenado hasta accionar el fin de carrera superior
    WaitingQueueSlot, ///< Retiene la botella llena si la cola intermedia alcanzó MAX_INTERSTAGE_QUEUE
    ReleasingBottle   ///< Sube el brazo de sujeción y, tras RELEASE_GAP_MS, sube el de tope
};

/**
 * @enum CappingState
 * @brief Estados de la máquina de tres fases del Autómata 2 (Tapado).
 */
enum class CappingState : uint8_t
{
    Idle,            ///< Brazos arriba y cabezal detenido; aguardando presencia de botella
    StopArmDown,     ///< Baja el brazo de tope; espera CLAMP_DELAY_MS con la banda en movimiento
    ClampArmDown,    ///< Baja el brazo de sujeción y valida permanencia de la botella
    LoweringHead,    ///< Desciende el eje Z de tapado hasta la cota del perfil activo
    TighteningCap,   ///< Acciona el motor DC de apriete vigilando corriente en el puente H BTS7960
    RaisingHead,     ///< Frena el motor DC y asciende el eje Z hasta el fin de carrera superior
    ReleasingBottle  ///< Incrementa bottleCount y ejecuta la liberación escalonada de brazos
};

/**
 * @enum TouchCommand
 * @brief Comandos originados por los botones táctiles de la pantalla TFT.
 *
 * StationController descarta todo comando mientras la seta E-STOP permanezca activa (PIN_ESTOP_AUX).
 */
enum class TouchCommand : uint8_t
{
    None = 0,           ///< Sin comando táctil pendiente
    CycleBottleProfile, ///< En Running con ambos autómatas en Idle: alterna entre Small y Large
    AcknowledgeAlarm,   ///< En ErrorStop (con seta liberada): transiciona a ManualRecovery
    JogFillZUp,         ///< En ManualRecovery: sube el eje Z de llenado en pasos seguros (Z_JOG_STEPS)
    JogCapZUp,          ///< En ManualRecovery: sube el eje Z de tapado en pasos seguros (Z_JOG_STEPS)
    ReleaseArms,        ///< En ManualRecovery: levanta inmediatamente ambos pares de brazos
    ConfirmLineClear    ///< En ManualRecovery: confirma despeje físico y reinicia secuencia Boot
};

/**
 * @enum BuzzerPattern
 * @brief Patrones acústicos no bloqueantes emitidos por el driver AlarmBuzzer según severidad.
 */
enum class BuzzerPattern : uint8_t
{
    Silent = 0,    ///< Silencio total (operación nominal o alarma ya reconocida en ManualRecovery)
    BootReady,     ///< Secuencia única de pitidos cortos al concluir Homing y pasar a Running
    Warning,       ///< Pulso breve periódico para advertencias no bloqueantes (ERR-902)
    Fault,         ///< Intermitencia media para fallos de proceso en ErrorStop (ERR-101..303)
    EmergencyStop  ///< Intermitencia rápida urgente cuando la seta física está activa (ERR-901)
};

/**
 * @struct DisplaySnapshot
 * @brief Instantánea del estado de la celda para renderizado incremental en la pantalla TFT.
 */
struct DisplaySnapshot
{
    ErrorCode     error;       ///< Código de fallo activo (ErrorCode::None si no hay alarma)
    StationMode   mode;        ///< Modo global actual de la estación
    FillState     fillState;   ///< Fase activa del autómata de llenado
    CappingState  capState;    ///< Fase activa del autómata de tapado
    BottleProfile profile;     ///< Perfil de botella seleccionado
    uint8_t       queueCount;  ///< Botellas actualmente en la cola intermedia entre Llenado y Tapado [unidades]
    uint32_t      bottleCount; ///< Conteo acumulado de botellas completadas desde el arranque [unidades]
};

/**
 * @struct TelemetrySnapshot
 * @brief Instantánea de telemetría publicada periódicamente hacia Node-RED vía MQTT/JSON.
 */
struct TelemetrySnapshot
{
    ErrorCode     error;        ///< Código de fallo activo
    StationMode   mode;         ///< Modo global actual de la estación
    FillState     fillState;    ///< Fase activa del autómata de llenado
    CappingState  capState;     ///< Fase activa del autómata de tapado
    BottleProfile profile;      ///< Perfil de botella seleccionado
    uint8_t       queueCount;   ///< Botellas actualmente en la cola intermedia entre Llenado y Tapado [unidades]
    uint8_t       beltPwm;      ///< Ciclo de trabajo PWM activo en la banda transportadora [0..255]
    uint32_t      bottleCount;  ///< Conteo acumulado de botellas producidas [unidades]
    bool          presenceFill; ///< Estado filtrado del sensor de presencia en llenado
    bool          presenceCap;  ///< Estado filtrado del sensor de presencia en tapado
    bool          estopActive;  ///< Estado del contacto auxiliar de la seta de emergencia E-STOP
};
