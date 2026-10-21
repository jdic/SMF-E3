/**
 * @file fill_pump.hpp
 * @brief Controlador HAL no bloqueante para la bomba de llenado ON/OFF con corte autonomo por tiempo maximo.
 */

#pragma once
#include <Arduino.h>

/**
 * @class FillPump
 * @brief Gestiona la etapa de potencia de la bomba de llenado con proteccion contra encendido prolongado.
 *
 * Impone un tiempo maximo continuo de bombeo (maxOnTimeMs) evaluado en update() para prevenir
 * desbordamientos incluso ante bloqueos logicos del automata superior.
 */
class FillPump
{
public:
    /**
     * @brief Construye el controlador de la bomba de llenado.
     *
     * @param pin Numero de pin GPIO conectado al rele o MOSFET de la bomba (definido en HardwarePins).
     * @param maxOnTimeMs Tiempo maximo continuo permitido con la bomba encendida en milisegundos [ms]
     *                    (MAX_PUMP_ON_MS en ProcessParams).
     * @param activeHigh true si la etapa de potencia activa la bomba con nivel HIGH (pull-down externo);
     *                   false si es activa en nivel bajo (LOW).
     */
    FillPump(uint8_t pin, uint16_t maxOnTimeMs, bool activeHigh = true);

    /**
     * @brief Configura el pin como salida digital y asegura que la bomba inicie apagada.
     */
    void init();

    /**
     * @brief Enciende la bomba e inicia el temporizador interno de seguridad.
     */
    void on();

    /**
     * @brief Apaga inmediatamente la bomba de llenado.
     */
    void off();

    /**
     * @brief Supervisa el tiempo de encendido continuo y corta la bomba si rebasa maxOnTimeMs.
     *
     * Debe invocarse periodicamente desde la tarea de control del super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Consulta si la bomba se encuentra actualmente energizada.
     * @return true si la salida esta activa; false si esta apagada.
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
