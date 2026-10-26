/**
 * @file station_controller.hpp
 * @brief Coordinador central (Patrón Mediador) de la Estación 3 de Llenado y Tapado.
 *
 * Concentra las instancias de todos los sensores y actuadores HAL, gobierna el modo global
 * de la estación (Boot, Running, ErrorStop, ManualRecovery), administra la cola intermedia
 * de botellas entre ambos autómatas, aplica el interbloqueo de la seta E-STOP y publica
 * instantáneas para la pantalla TFT y el cliente SCADA.
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "bottle_gate.hpp"
#include "stepper_axis.hpp"
#include "fill_pump.hpp"
#include "dc_motor_bts7960.hpp"
#include "interrupt_encoder.hpp"
#include "conveyor_controller.hpp"
#include "alarm_buzzer.hpp"
#include "filling_automaton.hpp"
#include "capping_automaton.hpp"

/**
 * @class StationController
 * @brief Mediador de la celda de manufactura que coordina ambos autómatas, la banda continua y la seguridad.
 */
class StationController
{
public:
    /**
     * @brief Construye el controlador de la estación inicializando la topología de pines y parámetros.
     */
    StationController();

    /**
     * @brief Inicializa todos los periféricos HAL, configura el Timer3 e inicia la secuencia Boot (Homing).
     *
     * Debe invocarse una vez desde setup().
     */
    void init();

    /**
     * @brief Ejecuta la lógica supervisora de 1 ms: muestreo de sensores, arbitraje de fallos, autómatas y cola.
     *
     * Función no bloqueante. Debe invocarse desde la tarea de control del super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Actualiza las rampas trapezoidales de los dos ejes Z y el lazo PI de velocidad de la banda.
     *
     * Función no bloqueante. Debe invocarse desde la tarea de perfiles de movimiento (cada 20 ms).
     */
    void updateMotionProfiles();

    /**
     * @brief Procesa un comando táctil o de rearme aplicando el interbloqueo de la seta E-STOP.
     *
     * Si el contacto auxiliar PIN_ESTOP_AUX está activo, el comando se descarta por completo.
     *
     * @param[in] cmd Comando originado por DisplayManager o por el pulsador físico de rearme.
     */
    void handleTouchCommand(TouchCommand cmd);

    /**
     * @brief Informa al mediador si el enlace con el broker MQTT/SCADA lleva perdido más de SCADA_OFFLINE_WARN_MS.
     *
     * Activa o limpia la advertencia no bloqueante ERR-902 sin detener el proceso productivo.
     *
     * @param[in] lost true si el enlace de red está caído de forma prolongada; false al restablecerse.
     */
    void setNetworkLinkLost(bool lost);

    /**
     * @brief Extrae la instantánea de estado para la pantalla TFT únicamente si hubo cambios desde la última lectura.
     *
     * @param[out] out Estructura donde se copiará la instantánea actual si había cambios pendientes.
     * @return true si el estado cambió y se actualizó out; false si no requiere redibujado.
     */
    bool consumeDisplaySnapshot(DisplaySnapshot& out);

    /**
     * @brief Construye y devuelve la instantánea completa de telemetría para su publicación por MQTT/JSON.
     * @return Estructura TelemetrySnapshot con el estado actual de la celda, sensores y RPM de la banda.
     */
    TelemetrySnapshot getTelemetry() const;

    /**
     * @brief Consulta el modo operativo global actual de la estación.
     * @return Modo activo dentro de la enumeración StationMode.
     */
    StationMode getMode() const;

    /**
     * @brief Consulta el código de error o advertencia actualmente visible en la estación.
     * @return ErrorCode activo (prioriza fallos críticos sobre la advertencia no bloqueante ERR-902).
     */
    ErrorCode getActiveError() const;

    /**
     * @brief Consulta la cantidad actual de botellas en la cola intermedia entre Llenado y Tapado.
     * @return Número de botellas en tránsito [0..MAX_INTERSTAGE_QUEUE].
     */
    uint8_t getQueueCount() const;

private:
    /**
     * @brief Transiciona la estación al modo Boot, reinicia la cola intermedia y ordena el homing de ambos ejes Z.
     */
    void enterBootMode();

    /**
     * @brief Transiciona la estación al modo Running tras concluir exitosamente el homing de ambos ejes Z.
     */
    void enterRunningMode();

    /**
     * @brief Ejecuta la parada segura coordinada (ErrorStop): apaga bomba, banda, motores DC y deshabilita steppers.
     * @param[in] error Código de fallo crítico que originó la detención.
     */
    void enterErrorStop(ErrorCode error);

    /**
     * @brief Compara el estado actual contra la última instantánea enviada a la TFT y activa snapshotDirty_ si cambió.
     */
    void refreshDisplaySnapshot();

    /**
     * @brief Calcula el código de error efectivo combinando fallos críticos y la advertencia de red ERR-902.
     * @return ErrorCode a proyectar en pantalla y telemetría.
     */
    ErrorCode computeEffectiveError() const;

    // Sensores digitales con antirrebote
    DigitalSensor      presenceFill_;         ///< Sensor óptico de presencia en llenado (PIN_PRESENCE_FILL)
    DigitalSensor      presenceCap_;          ///< Sensor óptico de presencia en tapado (PIN_PRESENCE_CAP)
    DigitalSensor      limitSwitchFill_;      ///< Fin de carrera superior del eje Z de llenado (PIN_LIMIT_SWITCH_FILL)
    DigitalSensor      limitSwitchCap_;       ///< Fin de carrera superior del eje Z de tapado (PIN_LIMIT_SWITCH_CAP)
    DigitalSensor      estopAux_;             ///< Contacto auxiliar NC de la seta de emergencia (PIN_ESTOP_AUX)
    DigitalSensor      btnReset_;             ///< Pulsador físico de rearme en paralelo a la TFT (PIN_BTN_RESET)

    // Actuadores y controladores de lazo local
    BottleGate         fillGate_;             ///< Brazos de tope y sujeción del autómata de llenado
    BottleGate         capGate_;              ///< Brazos de tope y sujeción del autómata de tapado
    StepperAxis        fillZ_;                ///< Eje Z paso a paso de la boquilla de llenado
    StepperAxis        capZ_;                 ///< Eje Z paso a paso del cabezal de tapado
    FillPump           fillPump_;             ///< Bomba de llenado con corte autónomo por tiempo máximo
    DcMotorBts7960     chuckMotor_;           ///< Puente H BTS7960 del motor DC de apriete de tapa
    InterruptEncoder   chuckEncoder_;         ///< Encoder incremental del cabezal de apriete (INT0/INT1)
    ConveyorController conveyor_;             ///< Controlador PI y detector de atasco de la banda de 24 V
    AlarmBuzzer        buzzer_;               ///< Señalizador acústico no bloqueante por severidad de alarma

    // Autómatas de proceso
    FillingAutomaton   fillingAutomaton_;     ///< Máquina de estados de tres fases de la etapa de llenado
    CappingAutomaton   cappingAutomaton_;     ///< Máquina de estados de tres fases de la etapa de tapado

    // Estado global del mediador
    StationMode        mode_;                 ///< Modo global activo de la estación
    ErrorCode          activeError_;          ///< Fallo crítico retenido (latch) en ErrorStop / ManualRecovery
    BottleProfile      profile_;              ///< Formato de botella seleccionado (Small / Large)
    uint8_t            interstageQueueCount_; ///< Botellas actualmente en tránsito entre Llenado y Tapado [unidades]
    uint32_t           bottleCount_;          ///< Conteo acumulado de botellas finalizadas desde el arranque [unidades]
    uint32_t           bootStartTimeMs_;      ///< Marca temporal de inicio de la secuencia de homing en Boot [ms]
    bool               networkWarningActive_; ///< Bandera de advertencia no bloqueante ERR-902 (SCADA sin enlace)
    DisplaySnapshot    lastSnapshot_;         ///< Última instantánea entregada a DisplayManager
    bool               snapshotDirty_;        ///< Indica que hay cambios pendientes de proyectar en la pantalla TFT
};
