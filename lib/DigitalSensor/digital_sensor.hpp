/**
 * @file digital_sensor.hpp
 * @brief Abstraccion HAL no bloqueante para entradas digitales con filtro antirrebote y deteccion de flancos.
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
     * @brief Construye una instancia de sensor digital asociada a un pin fisico.
     *
     * @param pin Numero de pin GPIO del ATmega2560 (definido en HardwarePins).
     * @param debounceMs Ventana de estabilidad antirrebote en milisegundos [ms] (definido en ProcessParams).
     * @param activeHigh true si el estado activo del sensor corresponde a nivel logico HIGH;
     *                   false si es activo en nivel bajo (LOW).
     * @param usePullup true para habilitar la resistencia interna INPUT_PULLUP del AVR;
     *                  false para entrada de alta impedancia INPUT.
     */
    DigitalSensor(
        uint8_t pin,
        uint16_t debounceMs,
        bool activeHigh = false,
        bool usePullup = true);

    /**
     * @brief Configura el registro de direccion del pin y captura el estado inicial sin generar flancos espurios.
     */
    void init();

    /**
     * @brief Ejecuta el filtro antirrebote no bloqueante y actualiza las banderas de flanco del ciclo actual.
     *
     * Debe invocarse periodicamente desde la tarea de control del super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Consulta el estado logico estable (filtrado) del sensor.
     * @return true si el sensor se encuentra en su estado activo; false en reposo.
     */
    bool isTriggered() const;

    /**
     * @brief Indica si el estado estable cambio durante la ultima llamada a update().
     * @return true unicamente en el ciclo de actualizacion donde ocurrio la transicion.
     */
    bool hasChanged() const;

    /**
     * @brief Detecta la transicion de reposo a estado activo (flanco de activacion).
     * @return true unicamente en el ciclo de actualizacion donde el sensor paso a activo.
     */
    bool onRisingEdge() const;

    /**
     * @brief Detecta la transicion de estado activo a reposo (flanco de liberacion).
     * @return true unicamente en el ciclo de actualizacion donde el sensor volvio a reposo.
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
