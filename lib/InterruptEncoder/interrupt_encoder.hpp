/**
 * @file interrupt_encoder.hpp
 * @brief Decodificador HAL de cuadratura x2 por interrupción externa (PORTD) con lectura atómica y cálculo de RPM.
 */

#pragma once
#include <Arduino.h>

/**
 * @enum EncoderSlot
 * @brief Ranura de interrupción estática asignada a cada instancia de InterruptEncoder.
 */
enum class EncoderSlot : uint8_t
{
    Chuck    = 0, ///< Cabezal de apriete (INT0 / INT1 en PD0 / PD1, pines 21 y 20)
    Conveyor = 1  ///< Banda transportadora (INT2 / INT3 en PD2 / PD3, pines 19 y 18)
};

/**
 * @class InterruptEncoder
 * @brief Abstrae un encoder incremental de cuadratura conectado a pines del puerto D (INT0..INT3).
 *
 * La rutina de servicio de interrupción (ISR) lee directamente el registro PIND en un ciclo de reloj
 * con decodificación x2 sobre el canal A. Todas las lecturas del contador de 32 bits desde el
 * super-loop se protegen mediante bloques atómicos (ATOMIC_BLOCK).
 */
class InterruptEncoder
{
public:
    static constexpr uint8_t  MAX_SLOTS       = 2;       ///< Cantidad máxima de encoders soportados en PORTD
    static constexpr uint8_t  EDGES_PER_PULSE = 2;       ///< Factor de multiplicación x2 (CHANGE en canal A)
    static constexpr uint32_t MS_PER_MINUTE   = 60000UL; ///< Factor de conversión de milisegundos a minutos [ms/min]

    /**
     * @brief Construye una instancia de encoder asociada a una ranura de interrupción y dos pines de PORTD.
     *
     * @param slot Ranura estática (EncoderSlot::Chuck o EncoderSlot::Conveyor).
     * @param pinA Pin de interrupción externa del canal A (pines 18..21 del Mega 2560, PORTD).
     * @param pinB Pin digital del canal B en el mismo puerto D (pines 18..21 del Mega 2560, PORTD).
     * @param pulsesPerRev Pulsos nominales por revolución [PPR] del encoder antes de multiplicación x2 (máx. recomendado 600).
     * @param usePullup true para activar resistencias INPUT_PULLUP internas; false si existe pull-up externo.
     */
    InterruptEncoder(
        EncoderSlot slot,
        uint8_t pinA,
        uint8_t pinB,
        uint16_t pulsesPerRev,
        bool usePullup = true);

    /**
     * @brief Configura los pines de entrada, calcula las máscaras de bit en PIND y registra la ISR externa.
     */
    void init();

    /**
     * @brief Copia de forma atómica el conteo acumulado de flancos (con signo) desde el último reset().
     *
     * Función no bloqueante. Segura para invocarse desde el super-loop mientras la ISR está activa.
     *
     * @return Conteo acumulado de flancos de cuadratura [flancos] (2 flancos = 1 pulso PPR).
     */
    int32_t getPulses() const;

    /**
     * @brief Reinicia de forma atómica el contador de pulsos y el historial de velocidad a cero.
     *
     * Función no bloqueante. Segura para invocarse desde el super-loop.
     */
    void reset();

    /**
     * @brief Calcula la velocidad angular en RPM a partir del incremento de flancos y el tiempo transcurrido en millis().
     *
     * Función no bloqueante. Debe invocarse a intervalo periódico desde el super-loop (cada 20 ms).
     *
     * @return Velocidad angular estimada en revoluciones por minuto [RPM].
     */
    uint16_t updateSpeed();

    /**
     * @brief Consulta la última velocidad angular calculada por updateSpeed() sin alterar las muestras.
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
