/**
 * @file hardware_pins.hpp
 * @brief Asignacion centralizada y exclusiva de pines del Arduino Mega 2560 (ATmega2560).
 *
 * Regla arquitectonica: este archivo contiene UNICAMENTE numeros de pin (cableado fisico).
 * Las cantidades de pasos, velocidades, tiempos y umbrales residen en process_params.hpp.
 */

#pragma once
#include <Arduino.h>

namespace HardwarePins
{
    // ==========================================
    // RESERVADOS: USB y Modulo Ethernet W5500 (SPI)
    // ==========================================
    // 0, 1     : Serial USB
    // 50 - 52  : SPI (MISO, MOSI, SCK) del modulo W5500
    constexpr uint8_t PIN_ETH_CS            = 53; // SS de hardware -> SCS del W5500
    constexpr uint8_t PIN_ETH_RST           = 49; // RST del W5500 (activo en bajo)

    // ==========================================
    // PANTALLA TFT 2.4" ILI9341 (Bus Paralelo 8 bits + Touch Resistivo)
    // ==========================================
    constexpr uint8_t PIN_TFT_RD            = A0;
    constexpr uint8_t PIN_TFT_WR            = A1;
    constexpr uint8_t PIN_TFT_RS            = A2; // Compartido con Touch XM
    constexpr uint8_t PIN_TFT_CS            = A3; // Compartido con Touch YP
    constexpr uint8_t PIN_TFT_RST           = A4;
    // Bus de datos paralelo de 8 bits (cableado 1 a 1 con el shield):
    // LCD_D0=8, LCD_D1=9, LCD_D2=2, LCD_D3=3, LCD_D4=4, LCD_D5=5, LCD_D6=6, LCD_D7=7
    // Pines del panel tactil (ajustar segun calibracion de MCUFRIEND_kbv):
    constexpr uint8_t PIN_TOUCH_XP          = 8;
    constexpr uint8_t PIN_TOUCH_XM          = A2;
    constexpr uint8_t PIN_TOUCH_YP          = A3;
    constexpr uint8_t PIN_TOUCH_YM          = 9;

    // ==========================================
    // ENTRADAS: Encoders de Cuadratura (Interrupcion Externa en PORTD)
    // ==========================================
    constexpr uint8_t PIN_ENC_CHUCK_A       = 21; // INT0 (Encoder cabezal apriete canal A)
    constexpr uint8_t PIN_ENC_CHUCK_B       = 20; // INT1 (Encoder cabezal apriete canal B)
    constexpr uint8_t PIN_ENC_CONV_A        = 19; // INT2 (Encoder banda canal A; inhabilita Serial1)
    constexpr uint8_t PIN_ENC_CONV_B        = 18; // INT3 (Encoder banda canal B; inhabilita Serial1)

    // ==========================================
    // ENTRADAS: Sensores Digitales (INPUT_PULLUP, PORTA)
    // ==========================================
    constexpr uint8_t PIN_PRESENCE_FILL     = 22; // PA0: Presencia de botella en automata de llenado
    constexpr uint8_t PIN_PRESENCE_CAP      = 23; // PA1: Presencia de botella en automata de tapado
    constexpr uint8_t PIN_LIMIT_SWITCH_FILL = 24; // PA2: Fin de carrera superior eje Z de llenado (ISR)
    constexpr uint8_t PIN_LIMIT_SWITCH_CAP  = 25; // PA3: Fin de carrera superior eje Z de tapado (ISR)
    constexpr uint8_t PIN_ESTOP_AUX         = 27; // Contacto auxiliar NC de la seta E-STOP
    constexpr uint8_t PIN_BTN_RESET         = 28; // Pulsador fisico de rearme (en paralelo al touch)

    // ==========================================
    // ENTRADA ANALOGICA: Sensado de Corriente (A0-A4 ocupados por TFT)
    // ==========================================
    constexpr uint8_t PIN_ADC_CHUCK_CURRENT = A8; // IS_R / IS_L del BTS7960 de apriete

    // ==========================================
    // SALIDAS: Servomotores de Brazos (libreria Servo, Timer5)
    // ==========================================
    constexpr uint8_t PIN_SERVO_FILL_STOP   = 44; // Brazo de tope - llenado
    constexpr uint8_t PIN_SERVO_FILL_CLAMP  = 45; // Brazo de sujecion - llenado
    constexpr uint8_t PIN_SERVO_CAP_STOP    = 46; // Brazo de tope - tapado
    constexpr uint8_t PIN_SERVO_CAP_CLAMP   = 47; // Brazo de sujecion - tapado

    // ==========================================
    // SALIDA: Bomba de Llenado ON/OFF (via rele o MOSFET; pull-down externo)
    // ==========================================
    constexpr uint8_t PIN_PUMP_FILL         = 43;

    // ==========================================
    // SALIDAS: Motor a Pasos - Eje Z de Llenado - PORTC
    // ==========================================
    constexpr uint8_t PIN_STEPPER_FILL_STEP = 37; // PC0 (escritura directa en ISR de Timer3)
    constexpr uint8_t PIN_STEPPER_FILL_DIR  = 36; // PC1
    constexpr uint8_t PIN_STEPPER_FILL_EN   = 35; // PC2

    // ==========================================
    // SALIDAS: Motor a Pasos - Eje Z de Tapado - PORTC
    // ==========================================
    constexpr uint8_t PIN_STEPPER_CAP_STEP  = 34; // PC3 (escritura directa en ISR de Timer3)
    constexpr uint8_t PIN_STEPPER_CAP_DIR   = 33; // PC4
    constexpr uint8_t PIN_STEPPER_CAP_EN    = 32; // PC5

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Apriete de Tapa) - Timer1 PWM
    // ==========================================
    constexpr uint8_t PIN_BTS_CHUCK_RPWM    = 11; // OC1A (490 Hz)
    constexpr uint8_t PIN_BTS_CHUCK_LPWM    = 12; // OC1B (490 Hz)
    constexpr uint8_t PIN_BTS_CHUCK_EN      = 40; // R_EN y L_EN unificados

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Banda 24 V, un sentido) - Timer2 PWM
    // ==========================================
    constexpr uint8_t PIN_BTS_CONV_RPWM     = 10; // OC2A (490 Hz); LPWM del modulo a GND
    constexpr uint8_t PIN_BTS_CONV_EN       = 41; // R_EN y L_EN unificados

    // ==========================================
    // SALIDA: Indicador de Estado en Placa / Torreta
    // ==========================================
    constexpr uint8_t PIN_STATUS_LED        = 42;
}
