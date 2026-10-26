/**
 * @file alarm_buzzer.hpp
 * @brief Controlador HAL no bloqueante para señalización acústica mediante buzzer activo de 5 V.
 *
 * Opera exclusivamente con temporización diferencial basada en millis() y digitalWrite(),
 * evitando cualquier conflicto con el Timer2 (usado por el canal PWM de la banda transportadora).
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"

/**
 * @class AlarmBuzzer
 * @brief Abstrae un buzzer activo con patrones acústicos periódicos y de disparo único.
 */
class AlarmBuzzer
{
public:
    /**
     * @brief Construye el controlador del buzzer asignando su pin digital de salida.
     * @param[in] pin Pin GPIO del Arduino Mega conectado a la etapa del buzzer activo.
     * @param[in] activeHigh true si el buzzer suena con nivel lógico HIGH (por defecto true).
     */
    explicit AlarmBuzzer(uint8_t pin, bool activeHigh = true);

    /**
     * @brief Configura el pin como salida digital y asegura el estado inicial en silencio.
     */
    void init();

    /**
     * @brief Establece el patrón acústico activo. Si el patrón ya estaba activo, no reinicia su fase.
     * @param[in] pattern Patrón deseado según el estado o severidad de alarma de la estación.
     * @param[in] nowMs Marca de tiempo actual provista por millis() [ms].
     */
    void setPattern(BuzzerPattern pattern, uint32_t nowMs);

    /**
     * @brief Dispara por única vez la secuencia de confirmación de arranque (BootReady).
     * @param[in] nowMs Marca de tiempo actual provista por millis() [ms].
     */
    void triggerBootReady(uint32_t nowMs);

    /**
     * @brief Silencia inmediatamente la salida física del buzzer y pasa a BuzzerPattern::Silent.
     */
    void silence();

    /**
     * @brief Actualiza la máquina de tiempos no bloqueante del patrón acústico activo.
     * @param[in] nowMs Marca de tiempo actual provista por millis() [ms].
     */
    void update(uint32_t nowMs);

    /**
     * @brief Consulta el patrón acústico actualmente en ejecución.
     * @return Enumeración BuzzerPattern activa.
     */
    BuzzerPattern getPattern() const;

    /**
     * @brief Indica si la salida física del buzzer está emitiendo sonido en este instante.
     * @return true si el buzzer se encuentra energizado.
     */
    bool isSounding() const;

private:
    /**
     * @brief Conmuta el nivel físico del pin según la polaridad configurada.
     * @param[in] on true para energizar el buzzer, false para apagarlo.
     */
    void writeOutput(bool on);

    const uint8_t pin_;              ///< Pin GPIO asignado al buzzer activo
    const bool    activeHigh_;       ///< Polaridad de activación (true = activo en HIGH)
    BuzzerPattern pattern_;          ///< Patrón acústico actualmente seleccionado
    uint32_t      phaseStartMs_;     ///< Instante de inicio del semi-ciclo ON u OFF actual [ms]
    uint8_t       remainingPulses_;  ///< Pulsos restantes para secuencias finitas como BootReady [pulsos]
    bool          outputOn_;         ///< Estado actual de excitación física del buzzer
};
