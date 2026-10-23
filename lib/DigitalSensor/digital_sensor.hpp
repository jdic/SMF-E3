/**
 * @file digital_sensor.hpp
 * @brief Abstracción HAL no bloqueante para entradas digitales con filtro antirrebote y detección de flancos.
 */

#pragma once
#include <Arduino.h>

/**
 * @class DigitalSensor
 * @brief Encapsula un sensor digital discreto (IR de presencia, fin de carrera, pulsador o contacto E-STOP).
 *
 * Opera por muestreo cooperativo en el super-loop mediante millis() sin bloquear la CPU.
 */
class DigitalSensor
{
public:
    /**
     * @brief Construye una instancia de sensor digital asociada a un pin físico.
     *
     * @param pin Número de pin GPIO del ATmega2560 (definido en HardwarePins).
     * @param debounceMs Ventana de estabilidad antirrebote en milisegundos [ms] (> 0).
     * @param activeHigh true si el estado activo corresponde a nivel lógico HIGH; false si es activo en LOW.
     * @param usePullup true para habilitar la resistencia interna INPUT_PULLUP del AVR; false para INPUT.
     */
    DigitalSensor(
        uint8_t pin,
        uint16_t debounceMs,
        bool activeHigh = false,
        bool usePullup = true);

    /**
     * @brief Configura la dirección del pin y captura el nivel inicial sin generar flancos espurios.
     *
     * No bloqueante. Debe invocarse una vez en setup().
     */
    void init();

    /**
     * @brief Ejecuta el filtro antirrebote con millis() y actualiza las banderas de flanco del ciclo actual.
     *
     * No bloqueante. Debe invocarse periódicamente desde el super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Evalúa si la entrada filtrada permanece en su estado lógico activo.
     * @return true mientras el sensor esté activado tras superar debounceMs; false en reposo.
     */
    bool isTriggered() const;

    /**
     * @brief Verifica si el estado estable conmutó durante la última llamada a update().
     * @return true únicamente durante el ciclo de actualización donde ocurrió el cambio.
     */
    bool hasChanged() const;

    /**
     * @brief Detecta la transición de reposo a estado activo (flanco de activación).
     * @return true únicamente durante el ciclo de actualización donde el sensor pasó a activo.
     */
    bool onRisingEdge() const;

    /**
     * @brief Detecta la transición de estado activo a reposo (flanco de liberación).
     * @return true únicamente durante el ciclo de actualización donde el sensor volvió a reposo.
     */
    bool onFallingEdge() const;

private:
    bool readRawTriggered() const;

    uint8_t  pin_;
    uint16_t debounceMs_;
    bool     activeHigh_;
    bool     usePullup_;
    bool     stableState_;
    bool     lastRawState_;
    bool     changed_;
    bool     risingEdge_;
    bool     fallingEdge_;
    uint32_t lastDebounceTimeMs_;
};
