/**
 * @file conveyor_controller.hpp
 * @brief Controlador HAL/lazo PI de velocidad y detector de atasco para la banda transportadora de 24 V.
 */

#pragma once
#include <Arduino.h>
#include "dc_motor_bts7960.hpp"
#include "interrupt_encoder.hpp"

/**
 * @brief Parametros de hardware, lazo PI y proteccion de atasco para ConveyorController.
 */
struct ConveyorConfig
{
    uint8_t  rpwmPin;      // Pin PWM por hardware del BTS7960 de la banda (HardwarePins)
    uint8_t  enPin;        // Pin de habilitacion EN del BTS7960 de la banda (HardwarePins)
    uint8_t  encPinA;      // Pin de interrupcion externa canal A del encoder (HardwarePins)
    uint8_t  encPinB;      // Pin digital canal B del encoder (HardwarePins)
    uint16_t encoderPpr;   // Pulsos por revolucion del encoder de la banda [PPR]
    uint16_t targetRpm;    // Consigna de velocidad angular en modo Running [RPM]
    uint8_t  kpTenths;     // Ganancia proporcional en decimas (Kp = kpTenths / 10)
    uint8_t  kiTenths;     // Ganancia integral en decimas por ciclo (Ki = kiTenths / 10)
    uint16_t stallTimeMs;  // Ventana sin pulsos con PWM activo para declarar atasco ERR-303 [ms]
    uint8_t  stallMinPwm;  // Umbral minimo de PWM [0..255] a partir del cual se evalua atasco
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
    static constexpr int32_t PI_SCALE        = 10;
    static constexpr int32_t MAX_PWM         = 255;
    static constexpr int32_t MIN_ACTIVE_PWM  = 1;

    /**
     * @brief Construye el controlador de banda integrando su puente H BTS7960 y su encoder de cuadratura.
     *
     * @param config Estructura con pines de hardware, ganancias PI y umbrales de atasco.
     */
    explicit ConveyorController(const ConveyorConfig& config);

    /**
     * @brief Inicializa el driver BTS7960 y el encoder por interrupcion, dejando la banda detenida.
     */
    void init();

    /**
     * @brief Inicia el giro continuo de la banda bajo regulacion PI hacia targetRpm.
     *
     * Si la banda ya se encontraba en marcha, no reinicia el integrador ni los temporizadores.
     */
    void start();

    /**
     * @brief Detiene la banda en parada libre (EN = LOW, PWM = 0) y desactiva el lazo PI.
     */
    void stop();

    /**
     * @brief Aplica frenado dinamico inmediato al motor de la banda y desactiva el lazo PI.
     */
    void brake();

    /**
     * @brief Ejecuta una iteracion del lazo PI de velocidad y verifica la condicion de atasco (ERR-303).
     *
     * Debe invocarse periodicamente desde la tarea de perfiles de movimiento (cada 20 ms).
     */
    void update();

    /**
     * @brief Indica si se detecto un atasco mecanico de la banda (PWM aplicado sin pulsos de encoder).
     * @return true si ocurrio la condicion de fallo ERR-303; false en operacion normal.
     */
    bool isStalled() const;

    /**
     * @brief Limpia la bandera de atasco tras un rearme validado por el operador.
     */
    void clearStall();

    /**
     * @brief Consulta la ultima velocidad angular medida en la banda.
     * @return Velocidad actual en revoluciones por minuto [RPM].
     */
    uint16_t getSpeedRpm() const;

    /**
     * @brief Consulta si el lazo de control de la banda se encuentra activo.
     * @return true si la banda esta en modo de giro continuo; false si esta detenida o en fallo.
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
