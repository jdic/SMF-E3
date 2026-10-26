/**
 * @file hardware_pins.hpp
 * @brief Asignación centralizada y exclusiva de pines del Arduino Mega 2560 (ATmega2560).
 *
 * Regla arquitectónica (§1.5): este archivo contiene ÚNICAMENTE números de pin (cableado físico).
 * Las cantidades de pasos, velocidades, tiempos y umbrales residen en process_params.hpp.
 */

#pragma once
#include <Arduino.h>

/**
 * @namespace HardwarePins
 * @brief Mapa físico de pines GPIO, PWM, ADC, SPI e interrupciones externas de la Estación 3.
 */
namespace HardwarePins
{
    // ==========================================
    // RESERVADOS: USB y Módulo Ethernet W5500 (SPI)
    // ==========================================
    // 0, 1     : Serial USB
    // 50 - 52  : SPI (MISO, MOSI, SCK) del módulo W5500
    constexpr uint8_t PIN_ETH_CS            = 53; ///< SS de hardware -> SCS del W5500
    constexpr uint8_t PIN_ETH_RST           = 49; ///< RST del W5500 (activo en nivel bajo)

    // ==========================================
    // PANTALLA TFT 2.4" ILI9341 (Bus Paralelo 8 bits + Touch Resistivo)
    // ==========================================
    constexpr uint8_t PIN_TFT_RD            = A0; ///< Línea Read Strobe del bus paralelo TFT
    constexpr uint8_t PIN_TFT_WR            = A1; ///< Línea Write Strobe del bus paralelo TFT
    constexpr uint8_t PIN_TFT_RS            = A2; ///< Selección Comando/Dato TFT (compartido con Touch XM)
    constexpr uint8_t PIN_TFT_CS            = A3; ///< Chip Select TFT (compartido con Touch YP)
    constexpr uint8_t PIN_TFT_RST           = A4; ///< Reset por hardware del controlador ILI9341
    // Bus de datos paralelo de 8 bits (cableado 1 a 1 con el shield):
    // LCD_D0=8, LCD_D1=9, LCD_D2=2, LCD_D3=3, LCD_D4=4, LCD_D5=5, LCD_D6=6, LCD_D7=7
    // Pines del panel táctil resistivo (calibrados para el shield 2.4" ILI9341):
    constexpr uint8_t PIN_TOUCH_XP          = 6;  ///< Terminal X+ del panel táctil resistivo (compartido con LCD_D6)
    constexpr uint8_t PIN_TOUCH_XM          = A2; ///< Terminal X- analógico del panel táctil (compartido con LCD_RS)
    constexpr uint8_t PIN_TOUCH_YP          = A1; ///< Terminal Y+ analógico del panel táctil (compartido con LCD_WR)
    constexpr uint8_t PIN_TOUCH_YM          = 7;  ///< Terminal Y- del panel táctil resistivo (compartido con LCD_D7)

    // ==========================================
    // ENTRADAS: Sensores Digitales (INPUT_PULLUP, PORTA)
    // ==========================================
    constexpr uint8_t PIN_PRESENCE_FILL     = 22; ///< PA0: Presencia de botella en autómata de llenado
    constexpr uint8_t PIN_PRESENCE_CAP      = 23; ///< PA1: Presencia de botella en autómata de tapado
    constexpr uint8_t PIN_LIMIT_SWITCH_FILL = 24; ///< PA2: Fin de carrera superior eje Z de llenado (leído en ISR via PINA)
    constexpr uint8_t PIN_LIMIT_SWITCH_CAP  = 25; ///< PA3: Fin de carrera superior eje Z de tapado (leído en ISR via PINA)
    constexpr uint8_t PIN_ESTOP_AUX         = 27; ///< PA5: Contacto auxiliar NC de la seta E-STOP
    constexpr uint8_t PIN_BTN_RESET         = 28; ///< PA6: Pulsador físico de rearme (en paralelo al comando táctil)

    // ==========================================
    // ENTRADAS ANALÓGICAS: Sensado de Corriente BTS7960 (A0-A4 ocupados por TFT)
    // ==========================================
    constexpr uint8_t PIN_ADC_CHUCK_CURRENT = A8; ///< Salida IS del BTS7960 del cabezal de apriete (ERR-301)
    constexpr uint8_t PIN_ADC_CONV_CURRENT  = A9; ///< Salida IS del BTS7960 de la banda transportadora (ERR-303)

    // ==========================================
    // SALIDAS: Servomotores de Brazos (librería Servo, Timer5)
    // ==========================================
    constexpr uint8_t PIN_SERVO_FILL_STOP   = 44; ///< Brazo de tope del autómata de llenado
    constexpr uint8_t PIN_SERVO_FILL_CLAMP  = 45; ///< Brazo de sujeción del autómata de llenado
    constexpr uint8_t PIN_SERVO_CAP_STOP    = 46; ///< Brazo de tope del autómata de tapado
    constexpr uint8_t PIN_SERVO_CAP_CLAMP   = 47; ///< Brazo de sujeción del autómata de tapado

    // ==========================================
    // SALIDA: Bomba de Llenado ON/OFF (vía relé o MOSFET; pull-down externo)
    // ==========================================
    constexpr uint8_t PIN_PUMP_FILL         = 43; ///< Etapa de potencia de la bomba de llenado

    // ==========================================
    // SALIDAS: Motor a Pasos - Eje Z de Llenado - PORTC
    // ==========================================
    constexpr uint8_t PIN_STEPPER_FILL_STEP = 37; ///< PC0: Pulso STEP eje Z llenado (escritura directa en ISR Timer3)
    constexpr uint8_t PIN_STEPPER_FILL_DIR  = 36; ///< PC1: Dirección DIR eje Z llenado
    constexpr uint8_t PIN_STEPPER_FILL_EN   = 35; ///< PC2: Habilitación EN eje Z llenado

    // ==========================================
    // SALIDAS: Motor a Pasos - Eje Z de Tapado - PORTC
    // ==========================================
    constexpr uint8_t PIN_STEPPER_CAP_STEP  = 34; ///< PC3: Pulso STEP eje Z tapado (escritura directa en ISR Timer3)
    constexpr uint8_t PIN_STEPPER_CAP_DIR   = 33; ///< PC4: Dirección DIR eje Z tapado
    constexpr uint8_t PIN_STEPPER_CAP_EN    = 32; ///< PC5: Habilitación EN eje Z tapado

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Apriete de Tapa) - Timer1 PWM
    // ==========================================
    constexpr uint8_t PIN_BTS_CHUCK_RPWM    = 11; ///< OC1A (490 Hz): Giro directo del cabezal de apriete
    constexpr uint8_t PIN_BTS_CHUCK_LPWM    = 12; ///< OC1B (490 Hz): Giro inverso del cabezal de apriete
    constexpr uint8_t PIN_BTS_CHUCK_EN      = 40; ///< R_EN y L_EN unificados del BTS7960 de apriete

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Banda 24 V, un sentido) - Timer2 PWM
    // ==========================================
    constexpr uint8_t PIN_BTS_CONV_RPWM     = 10; ///< OC2A (490 Hz): Giro directo de banda (LPWM conectado a GND)
    constexpr uint8_t PIN_BTS_CONV_EN       = 41; ///< R_EN y L_EN unificados del BTS7960 de banda

    // ==========================================
    // SALIDA: Indicador de Estado en Placa / Torreta y Alarma Acústica
    // ==========================================
    constexpr uint8_t PIN_STATUS_LED        = 42; ///< Indicador visual de estado general
    constexpr uint8_t PIN_BUZZER            = 38; ///< Buzzer activo de 5 V para señalización acústica de eventos y alarmas
}
