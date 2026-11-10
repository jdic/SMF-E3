/**
 * @file process_params.hpp
 * @brief Parámetros cinemáticos, temporales y de control de los procesos de dosificación y tapado.
 *
 * Centraliza cotas de pasos, velocidades, tiempos de proceso, ángulos de servomotores y umbrales de supervisión.
 */

#pragma once
#include <Arduino.h>

/**
 * @namespace ProcessParams
 * @brief Constantes de ingeniería de proceso y perfiles de producto.
 */
namespace ProcessParams
{
    // ==========================================
    // SENSORES DIGITALES (antirrebote)
    // ==========================================
    constexpr uint16_t SENSOR_DEBOUNCE_MS      = 25;    ///< Ventana de estabilidad de lectura digital [ms]

    // ==========================================
    // FLUJO ENTRE ETAPAS Y HABILITACIÓN DE MÓDULOS
    // ==========================================
    constexpr bool     ENABLE_INTERSTAGE_QUEUE = false; ///< Retiene botellas en llenado cuando la cola alcanza MAX_INTERSTAGE_QUEUE
    constexpr bool     ENABLE_CAPPING_STATION  = false; ///< Habilita el autómata de tapado (false opera únicamente llenado)
    constexpr uint8_t  MAX_INTERSTAGE_QUEUE    = 2;     ///< Capacidad máxima de botellas entre etapas [unidades]

    // ==========================================
    // EJES Z (MOTORES A PASOS)
    // ==========================================
    constexpr uint16_t Z_STEPS_PER_MM          = 400;   ///< Relación cinemática vertical [step/mm]
    constexpr uint16_t Z_CRUISE_SPEED          = 4000;  ///< Velocidad de crucero en traslación [step/s]
    constexpr uint16_t Z_HOMING_SPEED          = 1000;  ///< Velocidad de búsqueda de origen [step/s]
    constexpr uint16_t Z_ACCEL                 = 8000;  ///< Aceleración y desaceleración de rampa [step/s^2]
    constexpr uint16_t Z_HOME_BACKOFF_STEPS    = 200;   ///< Retroceso tras contacto con fin de carrera [step]
    constexpr uint16_t Z_JOG_STEPS             = 400;   ///< Paso de avance manual en recuperación [step]
    constexpr int32_t  FILL_Z_MAX_TRAVEL_STEPS = 24000; ///< Límite inferior de recorrido en llenado [step]
    constexpr int32_t  CAP_Z_MAX_TRAVEL_STEPS  = 24000; ///< Límite inferior de recorrido en tapado [step]
    constexpr uint16_t Z_MOVE_TIMEOUT_MS       = 5000;  ///< Tiempo límite de desplazamiento vertical [ms]
    constexpr uint16_t Z_HOMING_TIMEOUT_MS     = 10000; ///< Tiempo límite de búsqueda de origen en arranque [ms]

    // ==========================================
    // BANDA TRANSPORTADORA
    // ==========================================
    constexpr uint16_t BELT_SUPPLY_MV          = 12000; ///< Tensión nominal de la fuente de potencia [mV]
    constexpr uint16_t BELT_TARGET_MV          = 6000;  ///< Tensión de operación nominal del motor [mV]
    constexpr uint8_t  BELT_PWM                = static_cast<uint8_t>((255UL * BELT_TARGET_MV) / BELT_SUPPLY_MV); ///< Ciclo PWM nominal calculado
    constexpr uint8_t  BELT_ACCEL_PWM_STEP     = 5;     ///< Incremento de PWM por ciclo en aceleración
    constexpr uint8_t  BELT_DECEL_PWM_STEP     = 8;     ///< Decremento de PWM por ciclo en desaceleración
    constexpr uint16_t BELT_CURRENT_LIMIT_ADC  = 650;   ///< Umbral de sobrecorriente [cuentas ADC]
    constexpr uint16_t BELT_STALL_MS           = 500;   ///< Tiempo de persistencia de sobrecorriente para declarar atasco [ms]

    // ==========================================
    // SERVOMOTORES DE RETENCIÓN
    // ==========================================
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
    constexpr uint16_t RELEASE_GAP_MS          = 500;   ///< Intervalo entre liberación de sujeción y tope [ms]
    constexpr uint16_t RELEASE_MS              = 1500;  ///< Tiempo total de despeje de botella [ms]

    // ==========================================
    // DOSIFICACIÓN (BOMBA)
    // ==========================================
    constexpr uint16_t DRIP_MS                 = 1000;  ///< Tiempo de escurrimiento tras corte de bombeo [ms]
    constexpr uint16_t MAX_PUMP_ON_MS          = 8000;  ///< Límite de seguridad de activación continua de bomba [ms]

    // ==========================================
    // APRIETE DE TAPA
    // ==========================================
    constexpr uint16_t TIGHTEN_MS              = 2500;  ///< Duración nominal de ciclo de apriete [ms]
    constexpr uint8_t  CHUCK_PWM               = 200;   ///< Ciclo de trabajo PWM de apriete [0..255]
    constexpr uint16_t CHUCK_CURRENT_LIMIT_ADC = 600;   ///< Umbral de sobrecorriente crítica en apriete [cuentas ADC]
    constexpr uint16_t CHUCK_STALL_ADC         = 450;   ///< Umbral de corriente para detección de tope de rosca [cuentas ADC]
    constexpr uint16_t CHUCK_MIN_TIGHTEN_MS    = 350;   ///< Tiempo mínimo de inicio antes de evaluar bloqueo de rosca [ms]

    // ==========================================
    // SEÑALIZACIÓN (TORRETA Y ALARMA SONORA)
    // ==========================================
    constexpr bool     TOWER_RELAY_ACTIVE_LOW  = true;  ///< Nivel activo del módulo de relevadores (true = LOW)
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

    /**
     * @struct BottleProfileParams
     * @brief Parámetros dependientes del tipo de envase seleccionado.
     */
    struct BottleProfileParams
    {
        int32_t  fillZSteps; ///< Desplazamiento vertical de boquilla [step]
        int32_t  capZSteps;  ///< Desplazamiento vertical de cabezal [step]
        uint16_t fillMs;     ///< Tiempo de dosificación [ms]
    };

    /**
     * @brief Tabla de perfiles indexada por BottleProfile.
     */
    constexpr BottleProfileParams BOTTLE_PROFILES[] =
    {
        { 0, 0, 0 }, // Small
        { 0, 0, 0 }  // Large
    };
}
