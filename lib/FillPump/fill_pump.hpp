/**
 * @file fill_pump.hpp
 * @brief Controlador HAL no bloqueante para la bomba de llenado ON/OFF con corte autónomo por tiempo máximo.
 */

#pragma once
#include <Arduino.h>

/**
 * @class FillPump
 * @brief Gestiona la etapa de potencia de la bomba de llenado con protección contra encendido prolongado.
 *
 * Impone un límite temporal absoluto (maxOnTimeMs) evaluado en update() para prevenir
 * desbordamientos de líquido incluso ante bloqueos lógicos de la máquina de estados superior.
 */
class FillPump
{
public:
    /**
     * @brief Construye el controlador de la bomba de llenado.
     *
     * @param pin Número de pin GPIO conectado al relé o MOSFET de la bomba (definido en HardwarePins).
     * @param maxOnTimeMs Tiempo máximo continuo permitido con la bomba encendida en milisegundos [ms].
     * @param activeHigh true si la etapa de potencia se activa con nivel HIGH (pull-down externo);
     *                   false si es activa en nivel bajo (LOW).
     */
    FillPump(uint8_t pin, uint16_t maxOnTimeMs, bool activeHigh = true);

    /**
     * @brief Configura el pin como salida digital garantizando estado apagado previo para evitar pulsos de arranque.
     */
    void init();

    /**
     * @brief Energiza la bomba e inicia el temporizador interno de seguridad con millis().
     *
     * Función no bloqueante. Si ya estaba encendida, conserva la marca temporal original.
     */
    void on();

    /**
     * @brief Desenergiza inmediatamente la etapa de potencia de la bomba.
     *
     * Función no bloqueante.
     */
    void off();

    /**
     * @brief Supervisa el tiempo de bombeo acumulado y corta la salida si alcanza maxOnTimeMs.
     *
     * Función no bloqueante. Debe invocarse desde la tarea de control del super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Consulta si la etapa de potencia de la bomba se encuentra actualmente energizada.
     * @return true si la salida está activa; false si está apagada.
     */
    bool isOn() const;

private:
    void writeOutput(bool enable);

    uint8_t  pin_;
    uint16_t maxOnTimeMs_;
    bool     activeHigh_;
    bool     isOn_;
    uint32_t onStartTimeMs_;
};
