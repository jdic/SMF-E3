#include <Arduino.h>
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "fill_pump.hpp"
#include "bottle_gate.hpp"
#include "dc_motor_bts7960.hpp"

void setup()
{
    // Inicializacion base de la Estacion 3 (Llenado y Tapado)
    pinMode(HardwarePins::PIN_STATUS_LED, OUTPUT);
    digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);
}

void loop()
{
    // Planificador cooperativo pendiente de integracion
}
