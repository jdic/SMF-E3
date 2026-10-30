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
    // ENTRADAS: Sensores Digitales (PORTA y PORTL)
    // ==========================================
    constexpr uint8_t PIN_PRESENCE_FILL     = 22; ///< PA0: 1er sensor IR en llenado (aproximación -> baja brazo de tope, 5 V = HIGH)
    constexpr uint8_t PIN_POS_FILL          = 26; ///< PA4: 2do sensor IR en llenado (posición exacta entre brazos -> baja sujeción, 5 V = HIGH)
    constexpr uint8_t PIN_PRESENCE_CAP      = 23; ///< PA1: 1er sensor IR en tapado (aproximación -> baja brazo de tope, 5 V = HIGH)
    constexpr uint8_t PIN_POS_CAP           = 48; ///< PL1: 2do sensor IR en tapado (posición exacta entre brazos -> baja sujeción, 5 V = HIGH)
    constexpr uint8_t PIN_LIMIT_SWITCH_FILL = 24; ///< PA2: Fin de carrera superior eje Z llenado (COM a GND, INPUT_PULLUP)
    constexpr uint8_t PIN_LIMIT_SWITCH_CAP  = 25; ///< PA3: Fin de carrera superior eje Z tapado (COM a GND, INPUT_PULLUP)
    constexpr uint8_t PIN_ESTOP_AUX         = 27; ///< PA5: Contacto auxiliar NC de la seta E-STOP
    constexpr uint8_t PIN_BTN_RESET         = 28; ///< PA6: Pulsador físico de rearme (en paralelo al comando táctil)

    // ==========================================
    // ENTRADAS ANALÓGICAS: Sensado de Corriente BTS7960 (A0-A4 ocupados por TFT)
    // ==========================================
    constexpr uint8_t PIN_ADC_CHUCK_CURRENT = A8; ///< Salida L_IS del módulo IBT-2 del cabezal de apriete (ERR-301)
    constexpr uint8_t PIN_ADC_CONV_CURRENT  = A9; ///< Salida R_IS del módulo IBT-2 de la banda transportadora (ERR-303)

    // ==========================================
    // SALIDAS: Servomotores de Brazos (librería Servo, Timer5)
    // Pines en cabecera 22..53 (libres mientras el shield TFT está montado directo sobre el Mega)
    // ==========================================
    constexpr uint8_t PIN_SERVO_FILL_STOP   = 29; ///< PA7: Brazo de tope del autómata de llenado
    constexpr uint8_t PIN_SERVO_FILL_CLAMP  = 30; ///< PC7: Brazo de sujeción del autómata de llenado
    constexpr uint8_t PIN_SERVO_CAP_STOP    = 31; ///< PC6: Brazo de tope del autómata de tapado
    constexpr uint8_t PIN_SERVO_CAP_CLAMP   = 47; ///< PL2: Brazo de sujeción del autómata de tapado

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
    // SALIDAS: Módulo Único IBT-2 / BTS7960 (2 motores DC unidireccionales independientes)
    // - Semipuente L (borne M- respecto a B-/GND): Cabezal de Apriete de Tapa (LPWM, L_EN, L_IS)
    // - Semipuente R (borne M+ respecto a B-/GND): Banda Transportadora 24 V (RPWM, R_EN, R_IS)
    // ==========================================
    constexpr uint8_t PIN_BTS_CHUCK_RPWM    = 45; ///< PL4: Entrada LPWM del módulo IBT-2 (cabezal en M- a GND)
    constexpr uint8_t PIN_BTS_CHUCK_EN      = 40; ///< PG1: Entrada L_EN del módulo IBT-2 (habilitación cabezal)

    constexpr uint8_t PIN_BTS_CONV_RPWM     = 44; ///< PL5: Entrada RPWM del módulo IBT-2 (banda 24 V en M+ a GND)
    constexpr uint8_t PIN_BTS_CONV_EN       = 41; ///< PG0: Entrada R_EN del módulo IBT-2 (habilitación banda)

    // ==========================================
    // SALIDA: Indicador de Estado en Placa / Torreta y Alarma Acústica
    // ==========================================
    constexpr uint8_t PIN_STATUS_LED        = 42; ///< Indicador visual de estado general
    constexpr uint8_t PIN_BUZZER            = 38; ///< Buzzer activo de 5 V para señalización acústica de eventos y alarmas
}
