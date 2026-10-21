/**
 * @file process_params.hpp
 * @brief Parametros cinematicos, temporales, de control y calibracion de la Estacion 3.
 *
 * Regla arquitectonica: este archivo contiene UNICAMENTE cantidades (pasos, velocidades,
 * tiempos, angulos, ganancias y umbrales). Los pines fisicos residen en hardware_pins.hpp.
 */

#pragma once
#include <Arduino.h>

namespace ProcessParams
{
    // ==========================================
    // SENSORES DIGITALES (antirrebote)
    // ==========================================
    constexpr uint16_t SENSOR_DEBOUNCE_MS      = 25;    // Ventana de estabilidad para entradas digitales [ms]

    // ==========================================
    // EJES Z (motores a pasos de llenado y tapado)
    // ==========================================
    constexpr uint16_t Z_STEPS_PER_MM          = 400;   // 200 pasos/rev x 16 micropasos / 8 mm de avance (AJUSTAR)
    constexpr uint16_t Z_CRUISE_SPEED          = 4000;  // Velocidad de crucero [step/s] (max. 8000)
    constexpr uint16_t Z_HOMING_SPEED          = 1000;  // Busqueda del fin de carrera [step/s]
    constexpr uint16_t Z_ACCEL                 = 8000;  // Aceleracion [step/s^2]
    constexpr uint16_t Z_HOME_BACKOFF_STEPS    = 200;   // Retroceso tras tocar el fin de carrera [step]
    constexpr uint16_t Z_JOG_STEPS             = 400;   // Paso de jog en ManualRecovery [step]
    constexpr int32_t  FILL_Z_MAX_TRAVEL_STEPS = 24000; // Soft limit eje de llenado [step] (AJUSTAR)
    constexpr int32_t  CAP_Z_MAX_TRAVEL_STEPS  = 24000; // Soft limit eje de tapado [step] (AJUSTAR)
    constexpr uint16_t Z_MOVE_TIMEOUT_MS       = 5000;  // Guardia de bajada / subida [ms]
    constexpr uint16_t Z_HOMING_TIMEOUT_MS     = 10000; // Guardia de Boot [ms]

    // ==========================================
    // BANDA TRANSPORTADORA (motor 24 V + encoder)
    // ==========================================
    constexpr uint16_t BELT_TARGET_RPM         = 60;    // Consigna del lazo PI [RPM] (AJUSTAR)
    constexpr uint16_t BELT_ENCODER_PPR        = 600;   // Pulsos por vuelta del encoder (AJUSTAR)
    constexpr uint8_t  BELT_PI_KP              = 20;    // Ganancia proporcional (punto fijo, /10)
    constexpr uint8_t  BELT_PI_KI              = 5;     // Ganancia integral (punto fijo, /10)
    constexpr uint16_t BELT_STALL_MS           = 500;   // PWM aplicado sin pulsos (ERR-303) [ms]
    constexpr uint8_t  BELT_STALL_MIN_PWM      = 40;    // PWM minimo (0-255) para evaluar atasco

    // ==========================================
    // BRAZOS CON SERVOMOTOR (angulos en grados, tiempos en ms)
    // ==========================================
    constexpr uint8_t  FILL_STOP_UP_DEG        = 10;    // Brazo de tope - llenado (AJUSTAR)
    constexpr uint8_t  FILL_STOP_DOWN_DEG      = 100;
    constexpr uint8_t  FILL_CLAMP_UP_DEG       = 10;    // Brazo de sujecion - llenado (AJUSTAR)
    constexpr uint8_t  FILL_CLAMP_DOWN_DEG     = 95;
    constexpr uint8_t  CAP_STOP_UP_DEG         = 10;    // Brazo de tope - tapado (AJUSTAR)
    constexpr uint8_t  CAP_STOP_DOWN_DEG       = 100;
    constexpr uint8_t  CAP_CLAMP_UP_DEG        = 10;    // Brazo de sujecion - tapado (AJUSTAR)
    constexpr uint8_t  CAP_CLAMP_DOWN_DEG      = 95;
    constexpr uint16_t CLAMP_DELAY_MS          = 1000;  // Espera entre bajar el tope y bajar la sujecion [ms]
    constexpr uint16_t ARM_MOVE_MS             = 600;   // Recorrido de un servo sin retroalimentacion [ms]
    constexpr uint16_t RELEASE_GAP_MS          = 500;   // Entre subir sujecion y subir tope [ms]
    constexpr uint16_t RELEASE_MS              = 1500;  // Hasta que la botella libera el sensor [ms]

    // ==========================================
    // LLENADO (bomba)
    // ==========================================
    constexpr uint16_t DRIP_MS                 = 1000;  // Goteo con bomba apagada [ms]
    constexpr uint16_t MAX_PUMP_ON_MS          = 8000;  // Limite absoluto de bomba encendida (FillPump) [ms]

    // ==========================================
    // TAPADO (motor DC del cabezal)
    // ==========================================
    constexpr uint16_t TIGHTEN_MS              = 2500;  // Tiempo de apriete [ms]
    constexpr uint8_t  CHUCK_PWM               = 200;   // PWM de apriete (0-255) (AJUSTAR)
    constexpr uint16_t CHUCK_CURRENT_LIMIT_ADC = 600;   // Umbral ADC (0-1023) de sobrecorriente (AJUSTAR)
    constexpr uint16_t CHUCK_STALL_MS          = 150;   // PWM activo sin pulsos del encoder (ERR-302) [ms]

    // ==========================================
    // PANTALLA TACTIL (calibrar con setRotation(1) ya aplicada)
    // ==========================================
    constexpr int16_t  TS_MINX                 = 120;   // AJUSTAR con TouchScreen_Calibr_native
    constexpr int16_t  TS_MAXX                 = 900;
    constexpr int16_t  TS_MINY                 = 70;
    constexpr int16_t  TS_MAXY                 = 920;
    constexpr int16_t  TS_MIN_PRESSURE         = 200;
    constexpr int16_t  TS_MAX_PRESSURE         = 1000;

    /**
     * @brief Parametros dependientes del tamano de botella.
     */
    struct BottleProfileParams
    {
        int32_t  fillZSteps; // Cota de llenado desde home [step]
        int32_t  capZSteps;  // Cota de tapado desde home [step]
        uint16_t fillMs;     // Tiempo de bombeo [ms]
    };

    // Indice = static_cast<uint8_t>(BottleProfile)
    constexpr BottleProfileParams BOTTLE_PROFILES[] =
    {
        { 0, 0, 0 }, // Small: calibrar antes de operar con botellas
        { 0, 0, 0 }  // Large: calibrar antes de operar con botellas
    };
}
