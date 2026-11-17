/**
 * @file process_params.hpp
 * @brief Parámetros cinemáticos, temporales y de control de los procesos de dosificación y tapado.
 *
 * Centraliza cotas de pasos, velocidades, tiempos de proceso, ángulos de servomotores y umbrales de supervisión.
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"

/**
 * @namespace ProcessParams
 * @brief Constantes de ingeniería de proceso y perfiles de producto.
 */
namespace ProcessParams
{
    // ==========================================
    // SENSORES DIGITALES (antirrebote y polaridad)
    // ==========================================
    constexpr uint16_t SENSOR_DEBOUNCE_MS      = 25;    ///< Ventana de estabilidad de lectura digital [ms]
    constexpr bool     OPTICAL_ACTIVE_HIGH     = false; ///< false: sensor da LOW con objeto (NPN/típico IR); true: HIGH con objeto
    constexpr bool     OPTICAL_USE_PULLUP      = true;  ///< Habilita pullup interno para salidas NPN a colector abierto

    // ==========================================
    // FLUJO ENTRE ETAPAS Y HABILITACIÓN DE MÓDULOS
    // ==========================================
    constexpr bool     ENABLE_INTERSTAGE_QUEUE = false; ///< Retiene botellas en llenado cuando la cola alcanza MAX_INTERSTAGE_QUEUE
    constexpr bool     ENABLE_FILLING_STATION        = true;  ///< Habilita el autómata de llenado (false opera únicamente tapado/banda)
    constexpr bool     ENABLE_CAPPING_STATION        = true;  ///< Habilita el autómata de tapado (false opera únicamente llenado/banda)
    constexpr bool     ENABLE_FILL_SINGLE_POS_SENSOR = false; ///< true: Llenado opera solo con POS1 (ignora PRE1); false: usa PRE1 + POS1
    constexpr bool     ENABLE_CAP_SINGLE_POS_SENSOR  = false; ///< true: Tapado opera solo con POS2 (ignora PRE2); false: usa PRE2 + POS2
    constexpr uint8_t  MAX_INTERSTAGE_QUEUE    = 2;     ///< Capacidad máxima de botellas entre etapas [unidades]

    // ==========================================
    // EJES Z (MOTORES A PASOS)
    // ==========================================
    /**
     * @enum StepperDir
     * @brief Sentido de giro del motor a pasos (polaridad del pin DIR) para bajar hacia la botella.
     *        - CW : Nivel HIGH en DIR para bajar (LOW para subir).
     *        - CCW: Nivel LOW en DIR para bajar (HIGH para subir).
     */
    enum class StepperDir : uint8_t
    {
        CW  = 0, ///< Bajar con DIR = HIGH, subir con DIR = LOW
        CCW = 1  ///< Bajar con DIR = LOW,  subir con DIR = HIGH
    };

    constexpr StepperDir FILL_Z_DOWN_DIR       = StepperDir::CCW; ///< Sentido para BAJAR eje Z1 (Llenado): CW o CCW
    constexpr StepperDir CAP_Z_DOWN_DIR        = StepperDir::CCW; ///< Sentido para BAJAR eje Z2 (Tapado): CW o CCW

    constexpr uint16_t Z_STEPS_PER_MM          = 400;   ///< Relación cinemática vertical [step/mm]
    constexpr uint16_t Z_CRUISE_SPEED          = 4000;  ///< Velocidad de crucero en traslación [step/s]
    constexpr uint16_t CAP_Z_ENGAGE_SPEED      = 800;   ///< Velocidad de bajada lenta en tapado tras capEngageSteps [step/s] (800 = 2 mm/s)
    constexpr uint16_t Z_HOMING_SPEED          = 1000;  ///< Velocidad de búsqueda de origen [step/s]
    constexpr uint16_t Z_ACCEL                 = 8000;  ///< Aceleración y desaceleración de rampa [step/s^2]
    constexpr uint16_t Z_HOME_BACKOFF_STEPS    = 200;   ///< Retroceso tras contacto con fin de carrera [step]
    constexpr uint16_t Z_JOG_STEPS             = 400;   ///< Paso de avance manual en recuperación [step]
    constexpr int32_t  FILL_Z_MAX_TRAVEL_STEPS = 24000; ///< Límite inferior de recorrido en llenado [step]
    constexpr int32_t  CAP_Z_MAX_TRAVEL_STEPS  = 24000; ///< Límite inferior de recorrido en tapado [step]
    constexpr uint16_t Z_MOVE_TIMEOUT_MS       = 12000; ///< Tiempo límite de desplazamiento vertical [ms]
    constexpr uint16_t Z_HOMING_TIMEOUT_MS     = 21000; ///< Tiempo límite de búsqueda de origen en arranque [ms]

    // ==========================================
    // BANDA TRANSPORTADORA Y COMPENSACIÓN I*R
    // ==========================================
    constexpr uint16_t BELT_SUPPLY_MV          = 12000; ///< Tensión nominal de la fuente de potencia [mV]
    constexpr uint16_t BELT_TARGET_MV          = 4500;  ///< Tensión base en vacío del motor de la banda (4.5 V) [mV]
    constexpr uint16_t BELT_MAX_BOOST_MV       = 7500;  ///< Tensión máxima bajo carga con compensación I*R (7.5 V) [mV]
    constexpr uint8_t  BELT_PWM                = static_cast<uint8_t>((255UL * BELT_TARGET_MV) / BELT_SUPPLY_MV);    ///< Ciclo PWM base en vacío (~95)
    constexpr uint8_t  BELT_MAX_BOOST_PWM      = static_cast<uint8_t>((255UL * BELT_MAX_BOOST_MV) / BELT_SUPPLY_MV); ///< Ciclo PWM tope bajo carga (~159)
    constexpr bool     ENABLE_BELT_IR_COMP     = false;  ///< Habilita compensación automática de voltaje según corriente IS
    constexpr uint16_t BELT_IDLE_CURRENT_ADC   = 300;    ///< Umbral ADC en vacío (ver valor "IS" en pantalla DEBUG sin peso) [0..1023]
    constexpr uint16_t BELT_FULL_LOAD_ADC      = 220;   ///< Lectura ADC con carga donde alcanza BELT_MAX_BOOST_MV [0..1023]
    constexpr uint8_t  BELT_ACCEL_PWM_STEP     = 5;     ///< Incremento de PWM por ciclo en aceleración
    constexpr uint8_t  BELT_DECEL_PWM_STEP     = 8;     ///< Decremento de PWM por ciclo en desaceleración
    constexpr bool     ENABLE_BELT_STALL_PROT  = false; ///< Habilita corte por atasco ERR-303 en A8 (activar tras calibrar IS)
    constexpr uint16_t BELT_CURRENT_LIMIT_ADC  = 900;   ///< Umbral de sobrecorriente para falla de atasco ERR-303 [cuentas ADC]
    constexpr uint16_t BELT_STALL_MS           = 500;   ///< Tiempo de persistencia de sobrecorriente para declarar atasco [ms]

    // ==========================================
    // SERVOMOTORES DE RETENCIÓN
    // ==========================================
    /**
     * @enum ServoDir
     * @brief Sentido de giro del servomotor según su montaje mecánico.
     *        - CW : Sentido horario directo (escribe el ángulo tal cual: 0..180°).
     *        - CCW: Sentido antihorario / espejo (escribe 180° - ángulo).
     */
    enum class ServoDir : uint8_t
    {
        CW  = 0, ///< Sentido horario normal (0° -> 180°)
        CCW = 1  ///< Sentido antihorario / espejo (180° -> 0°)
    };

    constexpr ServoDir FILL_STOP_DIR           = ServoDir::CCW; ///< Sentido del servo de tope 1 (Llenado): CW o CCW
    constexpr ServoDir FILL_CLAMP_DIR          = ServoDir::CW; ///< Sentido del servo de sujeción 1 (Llenado): CW o CCW
    constexpr ServoDir CAP_STOP_DIR            = ServoDir::CCW; ///< Sentido del servo de tope 2 (Tapado): CW o CCW
    constexpr ServoDir CAP_CLAMP_DIR           = ServoDir::CW; ///< Sentido del servo de sujeción 2 (Tapado): CW o CCW

    constexpr uint8_t  FILL_STOP_UP_DEG        = 10;    ///< Ángulo de reposo del brazo de tope de llenado [grados]
    constexpr uint8_t  FILL_STOP_DOWN_DEG      = 100;   ///< Ángulo de bloqueo del brazo de tope de llenado [grados]
    constexpr uint8_t  FILL_CLAMP_UP_DEG       = 10;    ///< Ángulo de reposo del brazo de sujeción de llenado [grados]
    constexpr uint8_t  FILL_CLAMP_DOWN_DEG     = 95;    ///< Ángulo de bloqueo del brazo de sujeción de llenado [grados]
    constexpr uint8_t  CAP_STOP_UP_DEG         = 10;    ///< Ángulo de reposo del brazo de tope de tapado [grados]
    constexpr uint8_t  CAP_STOP_DOWN_DEG       = 100;   ///< Ángulo de bloqueo del brazo de tope de tapado [grados]
    constexpr uint8_t  CAP_CLAMP_UP_DEG        = 10;    ///< Ángulo de reposo del brazo de sujeción de tapado [grados]
    constexpr uint8_t  CAP_CLAMP_DOWN_DEG      = 95;    ///< Ángulo de bloqueo del brazo de sujeción de tapado [grados]
    constexpr uint16_t CLAMP_DELAY_MS          = 1000;  ///< Tiempo de espera entre accionamiento de brazos [ms]
    constexpr uint16_t CLAMP_SENSOR_TIMEOUT_MS = 3000;  ///< Tiempo límite para detección en sensor de posición [ms]
    constexpr uint16_t ARM_MOVE_MS             = 600;   ///< Tiempo de transición mecánica de los brazos [ms]
    constexpr uint16_t BOTTLE_LOST_GRACE_MS    = 500;  ///< Espera antes de declarar botella perdida tras perder presencia [ms]
    constexpr uint16_t RELEASE_GAP_MS          = 500;   ///< Intervalo entre liberación de sujeción y tope [ms]
    constexpr uint16_t RELEASE_MS              = 1500;  ///< Tiempo total de despeje de botella [ms]

    // ==========================================
    // DOSIFICACIÓN (BOMBA)
    // ==========================================
    constexpr bool     PUMP_RELAY_ACTIVE_LOW   = true;  ///< Nivel activo del relevador de la bomba (true = LOW, false = HIGH)
    constexpr uint16_t DRIP_MS                 = 1000;  ///< Tiempo de escurrimiento tras corte de bombeo [ms]
    constexpr uint16_t MAX_PUMP_ON_MS          = 12500; ///< Límite de seguridad de activación continua de bomba [ms]

    // ==========================================
    // APRIETE DE TAPA
    // ==========================================
    constexpr uint16_t TIGHTEN_MS              = 2500;  ///< Duración nominal de ciclo de apriete [ms]
    constexpr uint8_t  CHUCK_PWM               = 200;   ///< Ciclo de trabajo PWM de apriete [0..255]
    constexpr uint16_t CHUCK_CURRENT_LIMIT_ADC = 600;   ///< Umbral de sobrecorriente crítica en apriete [cuentas ADC]
    constexpr uint16_t CHUCK_STALL_ADC         = 450;   ///< Umbral de corriente para detección de tope de rosca [cuentas ADC]
    constexpr uint16_t CHUCK_MIN_TIGHTEN_MS    = 350;   ///< Tiempo mínimo de inicio antes de evaluar bloqueo de rosca [ms]

    // ==========================================
    // VISIÓN ARTIFICIAL (ESP32 + MATLAB)
    // ==========================================
    constexpr bool     ENABLE_VISION_INSPECTION   = true;  ///< Habilita la inspección de tapa por visión artificial tras tapado
    constexpr bool     ENABLE_VISION_BELT_ADVANCE = false; ///< true: levanta brazo de tope y avanza la banda antes de evaluar visión
    constexpr bool     VISION_ALARM_ACTIVE_HIGH   = true;  ///< true: 3.3V (HIGH) del ESP32 = sin tapa (alarma); false: 0V (LOW) = alarma
    constexpr uint16_t VISION_ADVANCE_MS          = 400;   ///< Tiempo que camina la banda para llevar la botella a la cámara [ms]
    constexpr uint16_t VISION_SETTLE_MS           = 2000;  ///< Tiempo con banda detenida esperando el resultado de MATLAB/ESP32 [ms]

    // ==========================================
    // SEÑALIZACIÓN (TORRETA Y ALARMA SONORA)
    // ==========================================
    constexpr bool     TOWER_RELAY_ACTIVE_LOW  = true;  ///< Nivel activo del módulo de relevadores (true = LOW)
    constexpr bool     ENABLE_BUZZER           = true; ///< Habilita la alarma sonora (false desactiva el buzzer temporalmente)
    constexpr uint16_t BUZZER_BOOT_ON_MS       = 80;    ///< Duración de pulso de inicio [ms]
    constexpr uint16_t BUZZER_BOOT_OFF_MS      = 80;    ///< Intervalo entre pulsos de inicio [ms]
    constexpr uint8_t  BUZZER_BOOT_PULSES      = 2;     ///< Cantidad de pulsos en inicio
    constexpr uint16_t BUZZER_WARN_ON_MS       = 100;   ///< Duración de pulso de advertencia [ms]
    constexpr uint16_t BUZZER_WARN_OFF_MS      = 4900;  ///< Intervalo entre pulsos de advertencia [ms]
    constexpr uint16_t BUZZER_FAULT_ON_MS      = 300;   ///< Duración de pulso en condición de fallo [ms]
    constexpr uint16_t BUZZER_FAULT_OFF_MS     = 300;   ///< Intervalo de silencio en condición de fallo [ms]
    constexpr uint16_t BUZZER_ESTOP_ON_MS      = 120;   ///< Duración de pulso en parada de emergencia [ms]
    constexpr uint16_t BUZZER_ESTOP_OFF_MS     = 120;   ///< Intervalo de silencio en parada de emergencia [ms]

    // ==========================================
    // PANTALLA TÁCTIL
    // ==========================================
    constexpr int16_t  TS_MINX                 = 945;   ///< Calibración eje X mínimo [cuentas ADC]
    constexpr int16_t  TS_MAXX                 = 101;   ///< Calibración eje X máximo [cuentas ADC]
    constexpr int16_t  TS_MINY                 = 925;   ///< Calibración eje Y mínimo [cuentas ADC]
    constexpr int16_t  TS_MAXY                 = 105;   ///< Calibración eje Y máximo [cuentas ADC]
    constexpr int16_t  TS_MIN_PRESSURE         = 80;    ///< Umbral mínimo de presión de contacto [cuentas ADC]
    constexpr int16_t  TS_MAX_PRESSURE         = 1000;  ///< Umbral máximo de presión de contacto [cuentas ADC]
    constexpr bool     TOUCH_DEBUG_COORDS      = false; ///< Habilita visualización de coordenadas de depuración
    constexpr bool     TOUCH_DEBUG_MARKER      = false; ///< Habilita marcador gráfico de pulsación

    // ==========================================
    // HABILITACIÓN DE CÓDIGOS DE ERROR (true = activo, false = ignorar en pruebas)
    // ==========================================
    constexpr bool     ENABLE_ERR_101_FILL_BOTTLE_LOST     = true; ///< ERR-101: Botella perdida en llenado
    constexpr bool     ENABLE_ERR_102_CAP_BOTTLE_LOST      = true; ///< ERR-102: Botella perdida en tapado
    constexpr bool     ENABLE_ERR_201_FILL_Z_HOME_FAILED   = true; ///< ERR-201: Fallo de homing en eje Z de llenado
    constexpr bool     ENABLE_ERR_202_FILL_Z_SOFT_LIMIT    = true; ///< ERR-202: Soft limit inferior en eje Z de llenado
    constexpr bool     ENABLE_ERR_203_FILL_Z_LOWER_TIMEOUT = true; ///< ERR-203: Timeout al bajar boquilla de llenado
    constexpr bool     ENABLE_ERR_204_FILL_Z_RAISE_TIMEOUT = true; ///< ERR-204: Timeout al subir boquilla de llenado
    constexpr bool     ENABLE_ERR_211_CAP_Z_HOME_FAILED    = true; ///< ERR-211: Fallo de homing en eje Z de tapado
    constexpr bool     ENABLE_ERR_212_CAP_Z_SOFT_LIMIT     = true; ///< ERR-212: Soft limit inferior en eje Z de tapado
    constexpr bool     ENABLE_ERR_213_CAP_Z_LOWER_TIMEOUT  = true; ///< ERR-213: Timeout al bajar cabezal de tapado
    constexpr bool     ENABLE_ERR_214_CAP_Z_RAISE_TIMEOUT  = true; ///< ERR-214: Timeout al subir cabezal de tapado
    constexpr bool     ENABLE_ERR_301_CHUCK_OVERCURRENT    = true; ///< ERR-301: Sobrecorriente crítica en motor de roscado
    constexpr bool     ENABLE_ERR_302_CHUCK_STALL          = true; ///< ERR-302: Atasco prematuro de rosca en tapado
    constexpr bool     ENABLE_ERR_303_CONVEYOR_STALL       = true; ///< ERR-303: Sobrecorriente / atasco en banda
    constexpr bool     ENABLE_ERR_304_CAP_VISION           = true; ///< ERR-304: Botella sin tapa detectada por visión artificial
    constexpr bool     ENABLE_ERR_901_EMERGENCY_STOP       = true; ///< ERR-901: Parada de emergencia por hardware
    constexpr bool     ENABLE_ERR_902_NETWORK_LINK_LOST    = true; ///< ERR-902: Advertencia de pérdida de enlace SCADA

    /**
     * @brief Construye la máscara inicial de ConfigFlags a partir de los valores constexpr por defecto.
     */
    constexpr uint16_t defaultConfigFlags()
    {
        return (ENABLE_FILLING_STATION          ? ConfigFlags::FILL_STATION     : 0U) |
               (ENABLE_CAPPING_STATION          ? ConfigFlags::CAP_STATION      : 0U) |
               (ENABLE_VISION_INSPECTION        ? ConfigFlags::VISION_INSPECT   : 0U) |
               (ENABLE_VISION_BELT_ADVANCE      ? ConfigFlags::VISION_ADVANCE   : 0U) |
               (ENABLE_INTERSTAGE_QUEUE         ? ConfigFlags::INTERSTAGE_QUEUE : 0U) |
               (ENABLE_BUZZER                   ? ConfigFlags::BUZZER_ENABLED   : 0U) |
               (ENABLE_BELT_IR_COMP             ? ConfigFlags::BELT_IR_COMP     : 0U) |
               ((ENABLE_BELT_STALL_PROT && ENABLE_ERR_303_CONVEYOR_STALL) ? ConfigFlags::BELT_STALL_PROT : 0U) |
               ((ENABLE_ERR_101_FILL_BOTTLE_LOST || ENABLE_ERR_102_CAP_BOTTLE_LOST) ? ConfigFlags::ERR_BOTTLE_LOST : 0U) |
               ((ENABLE_ERR_201_FILL_Z_HOME_FAILED || ENABLE_ERR_202_FILL_Z_SOFT_LIMIT ||
                 ENABLE_ERR_211_CAP_Z_HOME_FAILED  || ENABLE_ERR_212_CAP_Z_SOFT_LIMIT) ? ConfigFlags::ERR_Z_HOME_LIMIT : 0U) |
               ((ENABLE_ERR_203_FILL_Z_LOWER_TIMEOUT || ENABLE_ERR_204_FILL_Z_RAISE_TIMEOUT ||
                 ENABLE_ERR_213_CAP_Z_LOWER_TIMEOUT  || ENABLE_ERR_214_CAP_Z_RAISE_TIMEOUT) ? ConfigFlags::ERR_Z_TIMEOUT : 0U) |
               ((ENABLE_ERR_301_CHUCK_OVERCURRENT || ENABLE_ERR_302_CHUCK_STALL) ? ConfigFlags::ERR_CHUCK_TORQUE : 0U) |
               (ENABLE_ERR_901_EMERGENCY_STOP   ? ConfigFlags::ERR_ESTOP_HW     : 0U) |
               (ENABLE_ERR_902_NETWORK_LINK_LOST ? ConfigFlags::ERR_SCADA_LINK  : 0U) |
               (ENABLE_FILL_SINGLE_POS_SENSOR   ? ConfigFlags::FILL_SINGLE_POS_SENSOR : 0U) |
               (ENABLE_CAP_SINGLE_POS_SENSOR    ? ConfigFlags::CAP_SINGLE_POS_SENSOR  : 0U);
    }

    /**
     * @brief Almacena en tiempo de ejecución las banderas ConfigFlags modificables desde Settings.
     */
    inline uint16_t& runtimeConfigFlags()
    {
        static uint16_t s_flags = defaultConfigFlags();
        return s_flags;
    }

    /**
     * @brief Consulta si una bandera ConfigFlags está activa actualmente.
     */
    inline bool hasConfigFlag(uint16_t mask)
    {
        return (runtimeConfigFlags() & mask) != 0U;
    }

    /**
     * @brief Evalúa si un código de error específico se encuentra habilitado en tiempo de ejecución.
     * @param code Código de error a consultar.
     * @return true si el error debe procesarse normalmente; false si está desactivado desde Settings.
     */
    inline bool isErrorEnabled(ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::FillBottleLost:
            case ErrorCode::CapBottleLost:        return hasConfigFlag(ConfigFlags::ERR_BOTTLE_LOST);
            case ErrorCode::FillZHomeFailed:
            case ErrorCode::FillZSoftLimitHit:
            case ErrorCode::CapZHomeFailed:
            case ErrorCode::CapZSoftLimitHit:     return hasConfigFlag(ConfigFlags::ERR_Z_HOME_LIMIT);
            case ErrorCode::FillZLoweringTimeout:
            case ErrorCode::FillZRaisingTimeout:
            case ErrorCode::CapZLoweringTimeout:
            case ErrorCode::CapZRaisingTimeout:   return hasConfigFlag(ConfigFlags::ERR_Z_TIMEOUT);
            case ErrorCode::ChuckOvercurrent:
            case ErrorCode::ChuckStallPremature:  return hasConfigFlag(ConfigFlags::ERR_CHUCK_TORQUE);
            case ErrorCode::ConveyorStall:        return hasConfigFlag(ConfigFlags::BELT_STALL_PROT);
            case ErrorCode::CapMissingVision:     return hasConfigFlag(ConfigFlags::VISION_INSPECT);
            case ErrorCode::EmergencyStopPressed: return hasConfigFlag(ConfigFlags::ERR_ESTOP_HW);
            case ErrorCode::NetworkLinkLost:      return hasConfigFlag(ConfigFlags::ERR_SCADA_LINK);
            default:                              return false;
        }
    }

    /**
     * @struct BottleProfileParams
     * @brief Parámetros dependientes del tipo de envase seleccionado.
     */
    struct BottleProfileParams
    {
        int32_t  fillZSteps;     ///< Desplazamiento vertical de boquilla [step]
        int32_t  capZSteps;      ///< Desplazamiento vertical de cabezal [step]
        int32_t  capEngageSteps; ///< Paso desde el cual enciende el motor de tapado y baja lento hasta capZSteps [step]
        uint16_t fillMs;         ///< Tiempo de dosificación [ms]
    };

    /**
     * @brief Tabla de perfiles indexada por BottleProfile.
     */
    constexpr BottleProfileParams BOTTLE_PROFILES[] =
    {
        { 18443, 21240, 16400, 7500 },  // Small
        { 5600, 8800, 2800, 10500 }     // Large
    };

    static_assert(
        (MAX_PUMP_ON_MS > BOTTLE_PROFILES[static_cast<uint8_t>(BottleProfile::Small)].fillMs) &&
        (MAX_PUMP_ON_MS > BOTTLE_PROFILES[static_cast<uint8_t>(BottleProfile::Large)].fillMs),
        "Error en ProcessParams: MAX_PUMP_ON_MS debe ser mayor que fillMs en todos los perfiles de BOTTLE_PROFILES."
    );
}
