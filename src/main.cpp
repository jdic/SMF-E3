/**
 * @file main.cpp
 * @brief Punto de entrada y planificador cooperativo no bloqueante.
 *
 * Ejecuta tres tareas periódicas mediante marcas de tiempo millis() y gestión de interrupciones:
 * - Tarea 1 (1 ms) : StationController::update() (muestreo de entradas, seguridad, autómatas y alarmas).
 * - Tarea 2 (20 ms): StationController::updateMotionProfiles() (perfiles de velocidad de ejes y rampa de banda).
 * - Tarea 3 (50 ms): DisplayManager::pollTouchCommand() y refresco de interfaz gráfica en pantalla.
 */

#include <Arduino.h>
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"
#include "types/station_types.hpp"
#include "display_manager.hpp"
#include "station/station_controller.hpp"

namespace
{
    constexpr uint32_t CONTROL_PERIOD_MS = 1UL;  ///< Período de muestreo de sensores y autómatas FSM [ms]
    constexpr uint32_t MOTION_PERIOD_MS  = 20UL; ///< Período de actualización de rampas Z y corriente de banda [ms]
    constexpr uint32_t DISPLAY_PERIOD_MS = 50UL; ///< Período de lectura táctil y refresco incremental TFT [ms]

    StationController g_station;
    DisplayManager    g_display;

    uint32_t g_lastControlMs = 0UL;
    uint32_t g_lastMotionMs  = 0UL;
    uint32_t g_lastDisplayMs = 0UL;
}

void setup()
{
    Serial.begin(115200);

    // 1. Inicializar pantalla TFT 2.4" ILI9341 y dibujar estructura base Home
    g_display.init();
    g_display.showScreen(ScreenId::Home);

    // 2. Inicializar controlador central (sensores, actuadores, temporizadores e inicio de Boot)
    g_station.init();

    // 3. Renderizar estado inicial inmediatamente en pantalla
    DisplaySnapshot initialSnapshot{};
    if (g_station.consumeDisplaySnapshot(initialSnapshot))
    {
        g_display.render(initialSnapshot);
    }

    const uint32_t nowMs = millis();
    g_lastControlMs = nowMs;
    g_lastMotionMs  = nowMs;
    g_lastDisplayMs = nowMs;
}

void loop()
{
    const uint32_t nowMs = millis();

    // Tarea 1 (1 ms): Muestreo de entradas digitales, seguridad E-STOP y máquinas de estados
    if ((nowMs - g_lastControlMs) >= CONTROL_PERIOD_MS)
    {
        g_lastControlMs = nowMs;
        g_station.update();
    }

    // Tarea 2 (20 ms): Rampas de velocidad de ejes Z y vigilancia de sobrecorriente en banda 24 V
    if ((nowMs - g_lastMotionMs) >= MOTION_PERIOD_MS)
    {
        g_lastMotionMs = nowMs;
        g_station.updateMotionProfiles();
    }

    // Tarea 3 (50 ms): Lectura del panel táctil resistivo y actualización incremental de la TFT
    if ((nowMs - g_lastDisplayMs) >= DISPLAY_PERIOD_MS)
    {
        g_lastDisplayMs = nowMs;

        const TouchCommand cmd = g_display.pollTouchCommand();
        if (cmd != TouchCommand::None)
        {
            g_station.handleTouchCommand(cmd);
        }

        DisplaySnapshot snapshot{};
        if (g_station.consumeDisplaySnapshot(snapshot))
        {
            g_display.render(snapshot);
        }
    }
}
