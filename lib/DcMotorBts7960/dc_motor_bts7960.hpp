/**
 * @file dc_motor_bts7960.hpp
 * @brief Controlador HAL para puentes H de alta corriente BTS7960 (unidireccional o bidireccional) con sensado IS.
 */

#pragma once
#include <Arduino.h>

/**
 * @class DcMotorBts7960
 * @brief Abstrae el control PWM, frenado dinamico, parada libre y lectura de corriente de un driver BTS7960.
 *
 * Garantiza por software que los canales RPWM y LPWM jamas se modulen de forma simultanea.
 * Soporta configuracion unidireccional (LPWM cableado a GND en la banda) o bidireccional (cabezal de tapado).
 */
class DcMotorBts7960
{
public:
    static constexpr uint8_t UNUSED_PIN = 0xFF;
    static constexpr uint8_t CURRENT_FILTER_SAMPLES = 8;

    /**
     * @brief Construye una instancia del driver BTS7960.
     *
     * @param rpwmPin Pin PWM por hardware para giro directo (RPWM, definido en HardwarePins).
     * @param enPin Pin digital conectado a R_EN y L_EN unificados (definido en HardwarePins).
     * @param lpwmPin Pin PWM por hardware para giro inverso (LPWM), o UNUSED_PIN si LPWM va a GND.
     * @param currentSensePin Pin analogico conectado a la salida IS del BTS7960, o UNUSED_PIN si no se utiliza.
     */
    DcMotorBts7960(
        uint8_t rpwmPin,
        uint8_t enPin,
        uint8_t lpwmPin = UNUSED_PIN,
        uint8_t currentSensePin = UNUSED_PIN);

    /**
     * @brief Configura los pines de control como salidas y deja el puente H deshabilitado (parada libre).
     */
    void init();

    /**
     * @brief Acciona el motor en sentido directo (RPWM) con el ciclo de trabajo especificado.
     *
     * Si pwm == 0, aplica frenado dinamico (brake).
     *
     * @param pwm Ciclo de trabajo de modulacion en el rango [0..255].
     */
    void driveForward(uint8_t pwm);

    /**
     * @brief Acciona el motor en sentido inverso (LPWM) si el pin LPWM fue configurado.
     *
     * No tiene efecto si lpwmPin == UNUSED_PIN.
     *
     * @param pwm Ciclo de trabajo de modulacion en el rango [0..255].
     */
    void driveReverse(uint8_t pwm);

    /**
     * @brief Aplica frenado dinamico manteniendo EN en HIGH con RPWM = 0 y LPWM = 0.
     */
    void brake();

    /**
     * @brief Deshabilita la etapa de potencia (EN = LOW, RPWM = 0, LPWM = 0) dejando el motor en giro libre.
     *
     * Debe invocarse al entrar en ErrorStop o al desenergizar el actuador.
     */
    void stop();

    /**
     * @brief Toma una muestra del pin analogico IS y actualiza el filtro de media movil de 8 muestras.
     *
     * @return Valor filtrado actual del ADC en cuentas [0..1023], o 0 si currentSensePin == UNUSED_PIN.
     */
    uint16_t sampleCurrentAdc();

    /**
     * @brief Devuelve el ultimo valor filtrado de corriente sin disparar una nueva conversion ADC.
     * @return Valor promedio en cuentas ADC [0..1023].
     */
    uint16_t getFilteredCurrentAdc() const;

    /**
     * @brief Consulta el ciclo de trabajo PWM actualmente aplicado al motor.
     * @return Valor PWM actual en el rango [0..255].
     */
    uint8_t getCurrentPwm() const;

    /**
     * @brief Indica si el puente H se encuentra habilitado y aplicando PWM mayor que cero.
     * @return true si el motor esta energizado en movimiento; false si esta frenado o detenido.
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
