/**
 * @file hardware_pins.hpp
 * @brief Asignación centralizada de pines de entrada y salida del sistema.
 *
 * Define exclusivamente las constantes de mapeo físico para sensores,
 * actuadores, controladores de motor, botonera de control y señalización.
 */

#pragma once
#include <Arduino.h>

/**
 * @namespace HardwarePins
 * @brief Mapa físico de conexiones GPIO, PWM y ADC.
 */
namespace HardwarePins
{
    // =========================================================================
    // COMUNICACIÓN Y RESERVADOS
    // =========================================================================
    constexpr uint8_t PIN_ETH_RST           = 21; ///< Línea de reinicio del módulo de red
    constexpr uint8_t PIN_ETH_CS            = 21; ///< Selección de chip del bus de red

    // =========================================================================
    // PANTALLA Y PANEL TÁCTIL
    // =========================================================================
    constexpr uint8_t PIN_TFT_RD            = A0; ///< Línea de lectura (RD) de la pantalla
    constexpr uint8_t PIN_TFT_WR            = A1; ///< Línea de escritura (WR) de la pantalla / Terminal Y+ táctil
    constexpr uint8_t PIN_TFT_RS            = A2; ///< Selección de comando/datos (RS) de la pantalla / Terminal X- táctil
    constexpr uint8_t PIN_TFT_CS            = A3; ///< Selección de chip (CS) de la pantalla
    constexpr uint8_t PIN_TFT_RST           = A4; ///< Reinicio por hardware de la pantalla

    constexpr uint8_t PIN_TOUCH_XP          = 6;  ///< Terminal X+ del panel táctil
    constexpr uint8_t PIN_TOUCH_XM          = A2; ///< Terminal X- del panel táctil
    constexpr uint8_t PIN_TOUCH_YP          = A1; ///< Terminal Y+ del panel táctil
    constexpr uint8_t PIN_TOUCH_YM          = 7;  ///< Terminal Y- del panel táctil

    // =========================================================================
    // MOTORES DC (CONTROLADOR IBT-2)
    // =========================================================================
    // Motor de banda transportadora:
    constexpr uint8_t PIN_BTS_CONV_RPWM     = 10; ///< PWM de modulación de avance de banda
    constexpr uint8_t PIN_BTS_CONV_EN       = 12; ///< Habilitación del puente para banda
    constexpr uint8_t PIN_ADC_CONV_CURRENT  = A8; ///< Lectura analógica de corriente de banda (IS)

    // Motor de cabezal de apriete:
    constexpr uint8_t PIN_BTS_CHUCK_RPWM    = 11; ///< PWM de modulación de giro de apriete
    constexpr uint8_t PIN_BTS_CHUCK_EN      = 14; ///< Habilitación del puente para apriete
    constexpr uint8_t PIN_ADC_CHUCK_CURRENT = A9; ///< Lectura analógica de corriente de apriete (IS)

    // =========================================================================
    // SENSORES DE PRESENCIA Y POSICIÓN
    // =========================================================================
    constexpr uint8_t PIN_PRESENCE_FILL     = 15; ///< Sensor óptico de aproximación en módulo de llenado
    constexpr uint8_t PIN_POS_FILL          = 16; ///< Sensor óptico de posición exacta en módulo de llenado
    constexpr uint8_t PIN_PRESENCE_CAP      = 17; ///< Sensor óptico de aproximación en módulo de tapado
    constexpr uint8_t PIN_POS_CAP           = 18; ///< Sensor óptico de posición exacta en módulo de tapado

    // =========================================================================
    // BOTONERA DE CONTROL Y SEGURIDAD
    // =========================================================================
    constexpr uint8_t PIN_BTN_START         = 46; ///< Pulsador de marcha (START)
    constexpr uint8_t PIN_BTN_STOP          = 48; ///< Pulsador de parada de ciclo (STOP)
    constexpr uint8_t PIN_ESTOP_AUX         = 50; ///< Contacto auxiliar de la parada de emergencia
    constexpr uint8_t PIN_BTN_RESET         = 52; ///< Pulsador de rearme de alarmas (RESET)

    // =========================================================================
    // SENSORES DE LÍMITE (FINES DE CARRERA EJES Z)
    // =========================================================================
    constexpr uint8_t PIN_LIMIT_SWITCH_FILL = 23; ///< Fin de carrera superior del eje vertical de llenado
    constexpr uint8_t PIN_LIMIT_SWITCH_CAP  = 25; ///< Fin de carrera superior del eje vertical de tapado

    // =========================================================================
    // CONTROLADORES DE MOTOR A PASOS (EJES Z)
    // =========================================================================
    // Eje vertical de tapado:
    constexpr uint8_t PIN_STEPPER_CAP_EN    = 27; ///< Habilitación del controlador de tapado
    constexpr uint8_t PIN_STEPPER_CAP_DIR   = 29; ///< Dirección de giro del eje de tapado
    constexpr uint8_t PIN_STEPPER_CAP_STEP  = 31; ///< Pulso de paso del eje de tapado

    // Eje vertical de llenado:
    constexpr uint8_t PIN_STEPPER_FILL_EN   = 33; ///< Habilitación del controlador de llenado
    constexpr uint8_t PIN_STEPPER_FILL_DIR  = 35; ///< Dirección de giro del eje de llenado
    constexpr uint8_t PIN_STEPPER_FILL_STEP = 37; ///< Pulso de paso del eje de llenado

    // =========================================================================
    // SEÑALIZACIÓN Y ACTUADORES DE SALIDA
    // =========================================================================
    constexpr uint8_t PIN_BUZZER            = 39; ///< Alarma sonora
    constexpr uint8_t PIN_TOWER_GREEN       = 41; ///< Indicador luminoso de operación normal
    constexpr uint8_t PIN_STATUS_LED        = PIN_TOWER_GREEN; ///< Alias de compatibilidad para indicador de estado
    constexpr uint8_t PIN_TOWER_RED         = 43; ///< Indicador luminoso de parada o alarma
    constexpr uint8_t PIN_PUMP_FILL         = 45; ///< Activación de bomba de llenado

    // =========================================================================
    // SERVOMOTORES DE RETENCIÓN DE BOTELLA
    // =========================================================================
    constexpr uint8_t PIN_SERVO_FILL_STOP   = 47; ///< Brazo de tope en módulo de llenado
    constexpr uint8_t PIN_SERVO_FILL_CLAMP  = 49; ///< Brazo de sujeción en módulo de llenado
    constexpr uint8_t PIN_SERVO_CAP_STOP    = 51; ///< Brazo de tope en módulo de tapado
    constexpr uint8_t PIN_SERVO_CAP_CLAMP   = 53; ///< Brazo de sujeción en módulo de tapado
}
