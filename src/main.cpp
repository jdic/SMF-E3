#include <Arduino.h>
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "fill_pump.hpp"
#include "bottle_gate.hpp"
#include "dc_motor_bts7960.hpp"
#include "interrupt_encoder.hpp"
#include "conveyor_controller.hpp"
#include "stepper_axis.hpp"
#include "alarm_buzzer.hpp"
#include "display_manager.hpp"
#include "station/filling_automaton.hpp"
#include "station/capping_automaton.hpp"

DisplayManager display;

void setup()
{
    // Inicialización base de la Estación 3 (Llenado y Tapado)
    pinMode(HardwarePins::PIN_STATUS_LED, OUTPUT);
    digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);

    // Inicializar pantalla TFT 2.4" y mostrar la vista Home con el Layout inferior
    display.init();
    display.showScreen(ScreenId::Home);
}

void loop()
{
    // Permite cambiar entre Home y Settings tocando los iconos de la barra inferior
    display.updateTouchNavigation();
}
