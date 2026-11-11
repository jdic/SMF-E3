/**
 * @file conveyor_controller.hpp
 * @brief Controlador HAL con perfil trapezoidal de velocidad PWM y detección de atasco (IS) para la banda.
 */

#pragma once
#include <Arduino.h>
#include "dc_motor_bts7960.hpp"

/**
 * @struct ConveyorConfig
 * @brief Parámetros de hardware, consigna PWM, pendientes trapezoidales y protección de corriente para la banda.
 */
struct ConveyorConfig
{
    uint8_t  rpwmPin;            ///< Pin de control PWM de avance
    uint8_t  enPin;              ///< Pin de habilitación de potencia
    uint8_t  currentSensePin;    ///< Pin analógico de lectura de corriente (IS)
    uint8_t  targetPwm;          ///< Ciclo de trabajo PWM base en vacío [0..255]
    uint8_t  maxBoostPwm;        ///< Ciclo de trabajo PWM máximo bajo carga con compensación I*R [0..255]
    bool     enableIrComp;       ///< true para aumentar el PWM proporcionalmente a la corriente IS
    uint16_t idleCurrentAdc;     ///< Corriente base en vacío [cuentas ADC] a partir de la cual inicia el boost
    uint16_t fullLoadCurrentAdc; ///< Corriente de plena carga [cuentas ADC] donde se alcanza maxBoostPwm
    uint8_t  accelStepPwm;       ///< Incremento de PWM por ciclo de 20 ms en rampa de aceleración [1..255]
    uint8_t  decelStepPwm;       ///< Decremento de PWM por ciclo de 20 ms en rampa de desaceleración [1..255]
    bool     enableStallProt;    ///< true para habilitar corte automático por sobrecorriente ERR-303
    uint16_t currentLimitAdc;    ///< Umbral de sobrecorriente en cuentas ADC [0..1023] para evaluar atasco
    uint16_t stallTimeMs;        ///< Tiempo continuo superando currentLimitAdc para declarar atasco ERR-303 [ms]
};

/**
 * @class ConveyorController
 * @brief Gobierna el giro de la banda mediante rampa trapezoidal (aceleración, crucero y desaceleración) y vigila IS.
 *
 * Evita tirones bruscos en las botellas sobre la superficie adherente de la banda aplicando una rampa
 * progresiva cada 20 ms hacia la consigna (targetPwm o 0). Ante un paro de seguridad (brake/stop) corta
 * de manera inmediata.
 */
class ConveyorController
{
public:
    /**
     * @brief Construye el controlador de banda integrando su puente H BTS7960 con sensado de corriente.
     *
     * @param[in] config Estructura con pines de hardware, ciclo PWM nominal, pendientes de rampa y umbrales.
     */
    explicit ConveyorController(const ConveyorConfig& config);

    /**
     * @brief Inicializa el driver BTS7960 dejando la banda detenida y sin banderas de fallo.
     */
    void init();

    /**
     * @brief Ordena el avance de la banda iniciando la rampa de aceleración trapezoidal hasta targetPwm.
     *
     * Función no bloqueante.
     */
    void start();

    /**
     * @brief Ordena una pausa suave iniciando la rampa de desaceleración trapezoidal hasta llegar a PWM = 0 y frenar.
     *
     * Función no bloqueante. Ideal para detener la banda sin sacudir las botellas al cerrar los brazos.
     */
    void pauseSmooth();

    /**
     * @brief Deshabilita inmediatamente la etapa de potencia en parada libre (EN = LOW, PWM = 0, sin rampa).
     *
     * Función no bloqueante.
     */
    void stop();

    /**
     * @brief Aplica frenado dinámico inmediato al motor de la banda (EN = HIGH, PWM = 0, sin rampa).
     *
     * Función no bloqueante. Utilizado en paradas de emergencia o fallos críticos (ErrorStop).
     */
    void brake();

    /**
     * @brief Actualiza la rampa trapezoidal de PWM y verifica la condición de atasco por sobrecorriente (ERR-303).
     *
     * Función no bloqueante. Debe invocarse periódicamente desde la tarea de perfiles de movimiento (cada 20 ms).
     */
    void update();

    /**
     * @brief Evalúa si se detectó un atasco mecánico de la banda por sobrecorriente sostenida (ERR-303).
     * @return true si se disparó la protección ERR-303; false en operación normal.
     */
    bool isStalled() const;

    /**
     * @brief Restablece la bandera de atasco tras un rearme validado por el operador.
     */
    void clearStall();

    /**
     * @brief Consulta el ciclo de trabajo PWM instantáneo aplicado en este momento al motor de la banda.
     * @return Valor PWM actual en el rango [0..255].
     */
    uint8_t getCurrentPwm() const;

    /**
     * @brief Consulta el último valor filtrado de corriente del puente H de la banda.
     * @return Corriente filtrada en cuentas ADC de 10 bits [0..1023].
     */
    uint16_t getFilteredCurrentAdc() const;

    /**
     * @brief Consulta si la banda tiene activa la orden de avance hacia su velocidad de crucero.
     * @return true si la consigna actual es avanzar; false si está en pausa, frenada o en estado de fallo.
     */
    bool isRunning() const;

private:
    ConveyorConfig config_;             ///< Parámetros de configuración de pines, rampa PWM y corriente
    DcMotorBts7960 motor_;              ///< Instancia del driver BTS7960 unidireccional
    uint8_t        currentPwm_;         ///< Ciclo de trabajo PWM instantáneo en la rampa trapezoidal [0..255]
    uint8_t        desiredPwm_;         ///< Consigna objetivo actual de la rampa (targetPwm o 0) [0..255]
    uint32_t       overcurrentStartMs_; ///< Marca temporal de inicio de condición de sobrecorriente [ms]
    bool           inOvercurrent_;      ///< Indica si la lectura actual supera el umbral currentLimitAdc
    bool           running_;            ///< Indica si la banda tiene orden de avance activa
    bool           stalled_;            ///< Bandera retenida (latch) de fallo por atasco ERR-303
};
