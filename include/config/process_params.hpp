/**
 * @file process_params.hpp
 * @brief Parámetros cinemáticos, temporales, de control y calibración de la Estación 3.
 *
 * Regla arquitectónica (§1.5): este archivo contiene ÚNICAMENTE cantidades (pasos, velocidades,
 * tiempos, ángulos, ganancias y umbrales). Los pines físicos residen en hardware_pins.hpp.
 */

#pragma once
#include <Arduino.h>

/**
 * @namespace ProcessParams
 * @brief Constantes de ingeniería de proceso y tablas de perfiles de botella.
 */
namespace ProcessParams
{
    // ==========================================
    // SENSORES DIGITALES (antirrebote)
    // ==========================================
    constexpr uint16_t SENSOR_DEBOUNCE_MS      = 25;    ///< Ventana de estabilidad antirrebote para entradas digitales [ms]

    // ==========================================
    // COLA INTERMEDIA ENTRE AUTÓMATAS (Buffer WIP Llenado -> Tapado)
    // ==========================================
    constexpr uint8_t  MAX_INTERSTAGE_QUEUE    = 2;     ///< Máximo de botellas en tránsito/espera entre Llenado y Tapado [unidades] (AJUSTAR)

    // ==========================================
    // EJES Z (motores a pasos de llenado y tapado)
    // ==========================================
    constexpr uint16_t Z_STEPS_PER_MM          = 400;   ///< Relación cinemática: 200 pasos/rev x 16 micropasos / 8 mm [step/mm] (AJUSTAR)
    constexpr uint16_t Z_CRUISE_SPEED          = 4000;  ///< Velocidad de crucero en desplazamientos verticales [step/s] (máx. 8000)
    constexpr uint16_t Z_HOMING_SPEED          = 1000;  ///< Velocidad de búsqueda del fin de carrera superior [step/s]
    constexpr uint16_t Z_ACCEL                 = 8000;  ///< Aceleración y desaceleración de la rampa trapezoidal [step/s^2]
    constexpr uint16_t Z_HOME_BACKOFF_STEPS    = 200;   ///< Retroceso descendente tras accionar el fin de carrera [step]
    constexpr uint16_t Z_JOG_STEPS             = 400;   ///< Desplazamiento ascendente por cada pulso táctil en ManualRecovery [step]
    constexpr int32_t  FILL_Z_MAX_TRAVEL_STEPS = 24000; ///< Cota máxima descendente por software del eje Z de llenado [step] (AJUSTAR)
    constexpr int32_t  CAP_Z_MAX_TRAVEL_STEPS  = 24000; ///< Cota máxima descendente por software del eje Z de tapado [step] (AJUSTAR)
    constexpr uint16_t Z_MOVE_TIMEOUT_MS       = 5000;  ///< Tiempo máximo de guardia para bajada o subida de un eje Z [ms]
    constexpr uint16_t Z_HOMING_TIMEOUT_MS     = 10000; ///< Tiempo máximo de guardia para completar el homing en Boot [ms]

    // ==========================================
    // BANDA TRANSPORTADORA (motor 24 V + encoder)
    // ==========================================
    constexpr uint16_t BELT_TARGET_RPM         = 60;    ///< Consigna de velocidad angular del lazo PI de la banda [RPM] (AJUSTAR)
    constexpr uint16_t BELT_ENCODER_PPR        = 600;   ///< Pulsos nominales por revolución del encoder de la banda [PPR] (AJUSTAR)
    constexpr uint8_t  BELT_PI_KP              = 20;    ///< Ganancia proporcional del lazo PI en punto fijo (Kp = valor / 10)
    constexpr uint8_t  BELT_PI_KI              = 5;     ///< Ganancia integral del lazo PI en punto fijo (Ki = valor / 10)
    constexpr uint16_t BELT_STALL_MS           = 500;   ///< Tiempo máximo con PWM activo sin pulsos antes de declarar ERR-303 [ms]
    constexpr uint8_t  BELT_STALL_MIN_PWM      = 40;    ///< Umbral mínimo de PWM [0..255] para evaluar atasco de la banda

    // ==========================================
    // BRAZOS CON SERVOMOTOR (ángulos en grados, tiempos en ms)
    // ==========================================
    constexpr uint8_t  FILL_STOP_UP_DEG        = 10;    ///< Ángulo de reposo (arriba) del brazo de tope en llenado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  FILL_STOP_DOWN_DEG      = 100;   ///< Ángulo de bloqueo (abajo) del brazo de tope en llenado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  FILL_CLAMP_UP_DEG       = 10;    ///< Ángulo de reposo (arriba) del brazo de sujeción en llenado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  FILL_CLAMP_DOWN_DEG     = 95;    ///< Ángulo de retención (abajo) del brazo de sujeción en llenado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  CAP_STOP_UP_DEG         = 10;    ///< Ángulo de reposo (arriba) del brazo de tope en tapado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  CAP_STOP_DOWN_DEG       = 100;   ///< Ángulo de bloqueo (abajo) del brazo de tope en tapado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  CAP_CLAMP_UP_DEG        = 10;    ///< Ángulo de reposo (arriba) del brazo de sujeción en tapado [grados, 0..180] (AJUSTAR)
    constexpr uint8_t  CAP_CLAMP_DOWN_DEG      = 95;    ///< Ángulo de retención (abajo) del brazo de sujeción en tapado [grados, 0..180] (AJUSTAR)
    constexpr uint16_t CLAMP_DELAY_MS          = 1000;  ///< Espera entre bajar el brazo de tope y bajar el de sujeción [ms]
    constexpr uint16_t ARM_MOVE_MS             = 600;   ///< Tiempo estimado de recorrido físico de un servomotor [ms]
    constexpr uint16_t RELEASE_GAP_MS          = 500;   ///< Pausa escalonada entre subir el brazo de sujeción y subir el de tope [ms]
    constexpr uint16_t RELEASE_MS              = 1500;  ///< Tiempo total de liberación hasta que la botella despeja el sensor [ms]

    // ==========================================
    // LLENADO (bomba)
    // ==========================================
    constexpr uint16_t DRIP_MS                 = 1000;  ///< Espera de escurrimiento de boquilla con la bomba apagada [ms]
    constexpr uint16_t MAX_PUMP_ON_MS          = 8000;  ///< Límite absoluto de seguridad para bomba encendida continua [ms]

    // ==========================================
    // TAPADO (motor DC del cabezal)
    // ==========================================
    constexpr uint16_t TIGHTEN_MS              = 2500;  ///< Duración nominal de la fase de apriete de tapa [ms]
    constexpr uint8_t  CHUCK_PWM               = 200;   ///< Ciclo de trabajo PWM durante el apriete de tapa [0..255] (AJUSTAR)
    constexpr uint16_t CHUCK_CURRENT_LIMIT_ADC = 600;   ///< Umbral de sobrecorriente del cabezal en cuentas ADC [0..1023] (AJUSTAR)
    constexpr uint16_t CHUCK_STALL_MS          = 150;   ///< Ventana sin pulsos de encoder con PWM activo para declarar ERR-302 [ms]

    // ==========================================
    // SEÑALIZACIÓN ACÚSTICA (Buzzer activo 5 V no bloqueante)
    // ==========================================
    constexpr uint16_t BUZZER_BOOT_ON_MS       = 80;    ///< Duración de cada pitido corto de confirmación de arranque [ms]
    constexpr uint16_t BUZZER_BOOT_OFF_MS      = 80;    ///< Pausa entre pitidos cortos de confirmación de arranque [ms]
    constexpr uint8_t  BUZZER_BOOT_PULSES      = 2;     ///< Número de pitidos cortos al entrar en modo Running [pulsos]
    constexpr uint16_t BUZZER_WARN_ON_MS       = 100;   ///< Duración del pulso acústico de advertencia no bloqueante (ERR-902) [ms]
    constexpr uint16_t BUZZER_WARN_OFF_MS      = 4900;  ///< Pausa entre avisos acústicos de advertencia no bloqueante [ms]
    constexpr uint16_t BUZZER_FAULT_ON_MS      = 300;   ///< Tiempo encendido en patrón de fallo de proceso (ERR-1xx..3xx) [ms]
    constexpr uint16_t BUZZER_FAULT_OFF_MS     = 300;   ///< Tiempo apagado en patrón de fallo de proceso (ERR-1xx..3xx) [ms]
    constexpr uint16_t BUZZER_ESTOP_ON_MS      = 120;   ///< Tiempo encendido en patrón urgente de parada de emergencia (ERR-901) [ms]
    constexpr uint16_t BUZZER_ESTOP_OFF_MS     = 120;   ///< Tiempo apagado en patrón urgente de parada de emergencia (ERR-901) [ms]

    // ==========================================
    // PANTALLA TÁCTIL (calibrada con setRotation(1) en el shield 2.4")
    // En rotación horizontal: el eje X de pantalla (0..319) proviene de rawY (invertido)
    // y el eje Y de pantalla (0..239) proviene de rawX (invertido).
    // ==========================================
    constexpr int16_t  TS_MINX                 = 945;   ///< Lectura rawY en borde izquierdo (px = 0) [0..1023]
    constexpr int16_t  TS_MAXX                 = 101;   ///< Lectura rawY en borde derecho (px = 319) [0..1023]
    constexpr int16_t  TS_MINY                 = 925;   ///< Lectura rawX en borde superior (py = 0) [0..1023]
    constexpr int16_t  TS_MAXY                 = 105;   ///< Lectura rawX en borde inferior (py = 239) [0..1023]
    constexpr int16_t  TS_MIN_PRESSURE         = 80;    ///< Presión mínima válida Z para aceptar un toque estable [cuentas]
    constexpr int16_t  TS_MAX_PRESSURE         = 1000;  ///< Presión máxima válida Z para aceptar un toque [cuentas]
    constexpr bool     TOUCH_DEBUG_COORDS      = false; ///< Flag: muestra en pantalla las coordenadas RAW y mapeadas
    constexpr bool     TOUCH_DEBUG_MARKER      = false; ///< Flag: dibuja únicamente el marcador del último touch

    /**
     * @struct BottleProfileParams
     * @brief Parámetros de proceso dependientes del formato de botella seleccionado.
     */
    struct BottleProfileParams
    {
        int32_t  fillZSteps; ///< Cota descendente de llenado medida desde home [step]
        int32_t  capZSteps;  ///< Cota descendente de tapado medida desde home [step]
        uint16_t fillMs;     ///< Tiempo de activación de la bomba de llenado [ms]
    };

    /**
     * @brief Tabla de perfiles indexada por static_cast<uint8_t>(BottleProfile).
     */
    constexpr BottleProfileParams BOTTLE_PROFILES[] =
    {
        { 0, 0, 0 }, // Small: calibrar antes de operar con botellas
        { 0, 0, 0 }  // Large: calibrar antes de operar con botellas
    };
}
