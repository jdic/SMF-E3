/**
 * @file dc_motor_bts7960.hpp
 * @brief Controlador HAL para puentes H de alta corriente BTS7960 (unidireccional o bidireccional) con sensado IS.
 */

#pragma once
#include <Arduino.h>

/**
 * @class DcMotorBts7960
 * @brief Abstrae el control PWM, frenado dinámico, parada libre y lectura de corriente de un driver BTS7960.
 *
 * Garantiza por software que los canales RPWM y LPWM jamás se modulen de forma simultánea.
 * Soporta configuración unidireccional (LPWM cableado a GND en la banda) o bidireccional (cabezal de tapado).
 */
class DcMotorBts7960
{
public:
    static constexpr uint8_t UNUSED_PIN             = 0xFF; ///< Centinela para indicar pin opcional no conectado
    static constexpr uint8_t CURRENT_FILTER_SAMPLES = 8;    ///< Tamaño de ventana del filtro de media móvil sobre IS

    /**
     * @brief Construye una instancia del driver BTS7960.
     *
     * @param rpwmPin Pin PWM por hardware para giro directo (RPWM, definido en HardwarePins).
     * @param enPin Pin digital conectado a R_EN y L_EN unificados (definido en HardwarePins).
     * @param lpwmPin Pin PWM por hardware para giro inverso (LPWM), o UNUSED_PIN si LPWM va conectado a GND.
     * @param currentSensePin Pin analógico conectado a la salida IS del BTS7960, o UNUSED_PIN si no se utiliza.
     */
    DcMotorBts7960(
        uint8_t rpwmPin,
        uint8_t enPin,
        uint8_t lpwmPin = UNUSED_PIN,
        uint8_t currentSensePin = UNUSED_PIN);

    /**
     * @brief Configura los pines de control como salidas y deja el puente H deshabilitado en parada libre.
     */
    void init();

    /**
     * @brief Acciona el motor en sentido directo (RPWM) forzando previamente LPWM a cero.
     *
     * Función no bloqueante. Si pwm == 0, aplica frenado dinámico (brake).
     *
     * @param pwm Ciclo de trabajo de modulación en el rango [0..255].
     */
    void driveForward(uint8_t pwm);

    /**
     * @brief Acciona el motor en sentido inverso (LPWM) forzando previamente RPWM a cero.
     *
     * Función no bloqueante. No tiene efecto si lpwmPin == UNUSED_PIN.
     *
     * @param pwm Ciclo de trabajo de modulación en el rango [0..255].
     */
    void driveReverse(uint8_t pwm);

    /**
     * @brief Aplica frenado dinámico manteniendo EN en HIGH con RPWM = 0 y LPWM = 0 (cortocircuito de rama inferior).
     *
     * Función no bloqueante.
     */
    void brake();

    /**
     * @brief Deshabilita la etapa de potencia (EN = LOW, RPWM = 0, LPWM = 0) dejando el motor en giro libre.
     *
     * Función no bloqueante. Debe invocarse al entrar en ErrorStop o al desenergizar el actuador.
     */
    void stop();

    /**
     * @brief Ejecuta una lectura ADC sobre el pin IS (~112 us en AVR) y actualiza la media móvil de 8 muestras.
     *
     * @return Corriente filtrada expresada en cuentas ADC de 10 bits [0..1023], o 0 si currentSensePin == UNUSED_PIN.
     */
    uint16_t sampleCurrentAdc();

    /**
     * @brief Consulta el último promedio de corriente calculado sin iniciar una nueva conversión analógica.
     * @return Corriente filtrada en cuentas ADC de 10 bits [0..1023].
     */
    uint16_t getFilteredCurrentAdc() const;

    /**
     * @brief Consulta el ciclo de trabajo PWM actualmente comandado al puente H.
     * @return Ciclo de trabajo activo en el rango [0..255].
     */
    uint8_t getCurrentPwm() const;

    /**
     * @brief Evalúa si el puente H se encuentra habilitado y modulando un ciclo de trabajo mayor que cero.
     * @return true si el motor está energizado en movimiento; false si está frenado o en parada libre.
     */
    bool isRunning() const;

private:
    uint8_t  rpwmPin_;
    uint8_t  enPin_;
    uint8_t  lpwmPin_;
    uint8_t  currentSensePin_;
    uint8_t  currentPwm_;
    bool     enabled_;
    uint16_t currentSamples_[CURRENT_FILTER_SAMPLES];
    uint8_t  sampleIndex_;
    uint16_t filteredAdc_;
};
