/**
 * @file conveyor_controller.hpp
 * @brief Controlador HAL/lazo PI de velocidad y detector de atasco para la banda transportadora de 24 V.
 */

#pragma once
#include <Arduino.h>
#include "dc_motor_bts7960.hpp"
#include "interrupt_encoder.hpp"

/**
 * @struct ConveyorConfig
 * @brief Parámetros de hardware, ganancias del lazo PI y umbrales de protección de atasco para la banda.
 */
struct ConveyorConfig
{
    uint8_t  rpwmPin;     ///< Pin PWM por hardware del BTS7960 de la banda (HardwarePins)
    uint8_t  enPin;       ///< Pin de habilitación EN del BTS7960 de la banda (HardwarePins)
    uint8_t  encPinA;     ///< Pin de interrupción externa canal A del encoder (HardwarePins)
    uint8_t  encPinB;     ///< Pin digital canal B del encoder (HardwarePins)
    uint16_t encoderPpr;  ///< Pulsos nominales por revolución del encoder de la banda [PPR]
    uint16_t targetRpm;   ///< Consigna de velocidad angular en modo Running [RPM]
    uint8_t  kpTenths;    ///< Ganancia proporcional en décimas (Kp = kpTenths / 10)
    uint8_t  kiTenths;    ///< Ganancia integral en décimas por ciclo de 20 ms (Ki = kiTenths / 10)
    uint16_t stallTimeMs; ///< Ventana sin pulsos con PWM activo para declarar atasco ERR-303 [ms]
    uint8_t  stallMinPwm; ///< Umbral mínimo de PWM [0..255] a partir del cual se evalúa atasco
};

/**
 * @class ConveyorController
 * @brief Regula la velocidad continua de la banda de 24 V mediante un lazo PI en punto fijo y vigila atascos.
 *
 * Compensa el rozamiento cuando las botellas retenidas por los brazos de tope resbalan sobre la banda.
 * Si el PWM supera stallMinPwm y el encoder no registra pulsos durante stallTimeMs, corta la etapa de
 * potencia y activa la bandera de fallo isStalled() (ERR-303).
 */
class ConveyorController
{
public:
    static constexpr int32_t PI_SCALE       = 10;  ///< Factor de escala divisor para ganancias PI en décimas
    static constexpr int32_t MAX_PWM        = 255; ///< Límite superior de saturación del modulador PWM
    static constexpr int32_t MIN_ACTIVE_PWM = 1;   ///< Límite inferior de PWM mientras la banda está en marcha

    /**
     * @brief Construye el controlador de banda integrando su puente H BTS7960 y su encoder de cuadratura.
     *
     * @param config Estructura con pines de hardware, ganancias PI y umbrales de atasco.
     */
    explicit ConveyorController(const ConveyorConfig& config);

    /**
     * @brief Inicializa el driver BTS7960 y el encoder por interrupción, dejando la banda detenida.
     */
    void init();

    /**
     * @brief Habilita el giro continuo de la banda bajo regulación PI hacia targetRpm.
     *
     * Función no bloqueante. Si la banda ya se encontraba en marcha, conserva el estado del integrador.
     */
    void start();

    /**
     * @brief Deshabilita el lazo PI y detiene el motor en parada libre (EN = LOW, PWM = 0).
     *
     * Función no bloqueante.
     */
    void stop();

    /**
     * @brief Deshabilita el lazo PI y aplica frenado dinámico inmediato al motor de la banda.
     *
     * Función no bloqueante.
     */
    void brake();

    /**
     * @brief Ejecuta una iteración del lazo PI de velocidad y verifica la condición de atasco (ERR-303).
     *
     * Función no bloqueante. Debe invocarse periódicamente desde la tarea de perfiles de movimiento (cada 20 ms).
     */
    void update();

    /**
     * @brief Evalúa si se detectó un atasco mecánico de la banda (PWM >= stallMinPwm sin pulsos durante stallTimeMs).
     * @return true si se disparó la protección ERR-303; false en operación normal.
     */
    bool isStalled() const;

    /**
     * @brief Restablece la bandera de atasco tras un rearme validado por el operador.
     */
    void clearStall();

    /**
     * @brief Consulta la última velocidad angular medida en el eje de la banda.
     * @return Velocidad angular actual en revoluciones por minuto [RPM].
     */
    uint16_t getSpeedRpm() const;

    /**
     * @brief Consulta si el lazo PI de giro continuo se encuentra activo.
     * @return true si la banda está regulando velocidad en modo continuo; false si está detenida o en fallo.
     */
    bool isRunning() const;

private:
    ConveyorConfig   config_;
    DcMotorBts7960   motor_;
    InterruptEncoder encoder_;
    int32_t          integralScaled_;
    int32_t          lastStallPulses_;
    uint32_t         lastPulseTimeMs_;
    bool             running_;
    bool             stalled_;
};
