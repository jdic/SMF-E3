/**
 * @file interrupt_encoder.hpp
 * @brief Decodificador HAL de cuadratura x2 por interrupcion externa (PORTD) con lectura atomica y calculo de RPM.
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Ranura de interrupcion estatica asignada a cada instancia de InterruptEncoder.
 */
enum class EncoderSlot : uint8_t
{
    Chuck    = 0, // Cabezal de apriete (INT0 / INT1 en PD0 / PD1)
    Conveyor = 1  // Banda transportadora (INT2 / INT3 en PD2 / PD3)
};

/**
 * @class InterruptEncoder
 * @brief Abstrae un encoder incremental de cuadratura conectado a pines del puerto D (INT0..INT3).
 *
 * La rutina de servicio de interrupcion (ISR) lee directamente el registro PIND en un ciclo de reloj
 * con decodificacion x2 sobre el canal A. Todas las lecturas del contador de 32 bits desde el
 * super-loop se protegen mediante bloques atomicos (ATOMIC_BLOCK).
 */
class InterruptEncoder
{
public:
    static constexpr uint8_t  MAX_SLOTS        = 2;
    static constexpr uint8_t  EDGES_PER_PULSE  = 2;     // Decodificacion x2 (CHANGE en canal A)
    static constexpr uint32_t MS_PER_MINUTE    = 60000UL;

    /**
     * @brief Construye una instancia de encoder asociada a una ranura de interrupcion y dos pines de PORTD.
     *
     * @param slot Ranura estatica (EncoderSlot::Chuck o EncoderSlot::Conveyor).
     * @param pinA Pin de interrupcion externa del canal A (pines 18..21 del Mega 2560, PORTD).
     * @param pinB Pin digital del canal B en el mismo puerto D (pines 18..21 del Mega 2560, PORTD).
     * @param pulsesPerRev Pulsos nominales por revolucion [PPR] del encoder antes de multiplicacion x2.
     * @param usePullup true para activar resistencias INPUT_PULLUP internas; false si hay pull-up externo.
     */
    InterruptEncoder(
        EncoderSlot slot,
        uint8_t pinA,
        uint8_t pinB,
        uint16_t pulsesPerRev,
        bool usePullup = true);

    /**
     * @brief Configura los pines de entrada, calcula las mascaras de bit en PIND y registra la ISR.
     */
    void init();

    /**
     * @brief Obtiene de forma atomica el conteo acumulado de flancos (con signo) desde el ultimo reset().
     *
     * Seguro para invocarse desde el super-loop mientras la ISR esta activa.
     *
     * @return Conteo acumulado de flancos (2 flancos = 1 pulso PPR).
     */
    int32_t getPulses() const;

    /**
     * @brief Reinicia de forma atomica el contador de pulsos y el historial de velocidad a cero.
     */
    void reset();

    /**
     * @brief Actualiza el calculo de velocidad angular en RPM a partir del incremento de pulsos y el tiempo transcurrido.
     *
     * Debe invocarse a intervalo fijo desde la tarea de perfiles de movimiento (ej. cada 20 ms).
     *
     * @return Velocidad angular actual estimada en revoluciones por minuto [RPM].
     */
    uint16_t updateSpeed();

    /**
     * @brief Consulta la ultima velocidad angular calculada por updateSpeed().
     * @return Velocidad angular en revoluciones por minuto [RPM].
     */
    uint16_t getSpeedRpm() const;

private:
    void handleIsr();
    static void isrSlot0();
    static void isrSlot1();

    static InterruptEncoder* instances_[MAX_SLOTS];

    EncoderSlot      slot_;
    uint8_t          pinA_;
    uint8_t          pinB_;
    uint8_t          maskA_;
    uint8_t          maskB_;
    uint16_t         pulsesPerRev_;
    bool             usePullup_;
    volatile int32_t pulseCount_;
    int32_t          lastPulseSample_;
    uint32_t         lastSpeedTimeMs_;
    uint16_t         currentRpm_;
};
