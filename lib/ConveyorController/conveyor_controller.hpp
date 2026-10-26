/**
 * @file conveyor_controller.hpp
 * @brief Controlador HAL en lazo abierto PWM con detección de atasco por corriente (IS) para la banda de 24 V.
 */

#pragma once
#include <Arduino.h>
#include "dc_motor_bts7960.hpp"

/**
 * @struct ConveyorConfig
 * @brief Parámetros de hardware, consigna PWM y umbrales de protección de sobrecorriente para la banda.
 */
struct ConveyorConfig
{
    uint8_t  rpwmPin;         ///< Pin PWM por hardware del BTS7960 de la banda (HardwarePins)
    uint8_t  enPin;           ///< Pin de habilitación EN del BTS7960 de la banda (HardwarePins)
    uint8_t  currentSensePin; ///< Pin analógico conectado a la salida IS del BTS7960 de la banda (HardwarePins)
    uint8_t  targetPwm;       ///< Ciclo de trabajo PWM nominal en modo Running [0..255]
    uint16_t currentLimitAdc; ///< Umbral de sobrecorriente en cuentas ADC [0..1023] para evaluar atasco
    uint16_t stallTimeMs;     ///< Tiempo continuo superando currentLimitAdc para declarar atasco ERR-303 [ms]
};

/**
 * @class ConveyorController
 * @brief Gobierna el giro continuo de la banda de 24 V mediante PWM y supervisa su consumo de corriente.
 *
 * Si la corriente filtrada del puente H BTS7960 (pin IS) supera currentLimitAdc de forma ininterrumpida
 * durante stallTimeMs, corta la etapa de potencia y activa la bandera de fallo isStalled() (ERR-303).
 */
class ConveyorController
{
public:
    /**
     * @brief Construye el controlador de banda integrando su puente H BTS7960 con sensado de corriente.
     *
     * @param[in] config Estructura con pines de hardware, ciclo PWM nominal y umbrales de sobrecorriente.
     */
    explicit ConveyorController(const ConveyorConfig& config);

    /**
     * @brief Inicializa el driver BTS7960 dejando la banda detenida y sin banderas de fallo.
     */
    void init();

    /**
     * @brief Habilita el giro continuo de la banda aplicando el ciclo de trabajo targetPwm.
     *
     * Función no bloqueante.
     */
    void start();

    /**
     * @brief Deshabilita la etapa de potencia y detiene el motor en parada libre (EN = LOW, PWM = 0).
     *
     * Función no bloqueante.
     */
    void stop();

    /**
     * @brief Deshabilita el avance y aplica frenado dinámico inmediato al motor de la banda.
     *
     * Función no bloqueante.
     */
    void brake();

    /**
     * @brief Muestrea la corriente del puente H (IS) y verifica la condición de atasco por sobrecorriente (ERR-303).
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
     * @brief Consulta el ciclo de trabajo PWM actualmente aplicado al motor de la banda.
     * @return Valor PWM activo en el rango [0..255].
     */
    uint8_t getCurrentPwm() const;

    /**
     * @brief Consulta el último valor filtrado de corriente del puente H de la banda.
     * @return Corriente filtrada en cuentas ADC de 10 bits [0..1023].
     */
    uint16_t getFilteredCurrentAdc() const;

    /**
     * @brief Consulta si la banda se encuentra actualmente energizada en modo continuo.
     * @return true si la banda está en marcha; false si está detenida o en estado de fallo.
     */
    bool isRunning() const;

private:
    ConveyorConfig config_;             ///< Parámetros de configuración de pines, PWM y corriente
    DcMotorBts7960 motor_;              ///< Instancia del driver BTS7960 unidireccional (LPWM a GND)
    uint32_t       overcurrentStartMs_; ///< Marca temporal de inicio de condición de sobrecorriente [ms]
    bool           inOvercurrent_;      ///< Indica si la lectura actual supera el umbral currentLimitAdc
    bool           running_;            ///< Indica si la banda tiene orden de giro continuo activa
    bool           stalled_;            ///< Bandera retenida (latch) de fallo por atasco ERR-303
};
