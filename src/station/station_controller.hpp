/**
 * @file station_controller.hpp
 * @brief Coordinador central (Patrón Mediador) de los procesos de llenado y tapado.
 *
 * Concentra las instancias de sensores y actuadores, gobierna el modo operativo global
 * (Boot, Running, ErrorStop, ManualRecovery), administra la transferencia de botellas
 * entre autómatas, coordina el avance de banda y aplica interbloqueos de seguridad.
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "bottle_gate.hpp"
#include "stepper_axis.hpp"
#include "fill_pump.hpp"
#include "dc_motor_bts7960.hpp"
#include "conveyor_controller.hpp"
#include "alarm_buzzer.hpp"
#include "filling_automaton.hpp"
#include "capping_automaton.hpp"

/**
 * @class StationController
 * @brief Mediador central que coordina los autómatas de proceso, la banda y la seguridad.
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
     * @brief Actualiza las rampas trapezoidales de los dos ejes Z y la vigilancia de corriente de la banda.
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
     * @return Estructura TelemetrySnapshot con el estado actual de la celda, sensores y PWM de la banda.
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
     * @brief Transiciona la estación al modo de prueba manual (TestMode), apagando todos los actuadores.
     */
    void enterTestMode();

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
    DigitalSensor      presenceFill_;         ///< Sensor de aproximación en llenado
    DigitalSensor      positionFill_;         ///< Sensor de posición de botella en llenado
    DigitalSensor      presenceCap_;          ///< Sensor de aproximación en tapado
    DigitalSensor      positionCap_;          ///< Sensor de posición de botella en tapado
    DigitalSensor      limitSwitchFill_;      ///< Fin de carrera superior del eje de llenado
    DigitalSensor      limitSwitchCap_;       ///< Fin de carrera superior del eje de tapado
    DigitalSensor      btnStart_;             ///< Pulsador de marcha (START)
    DigitalSensor      btnStop_;              ///< Pulsador de parada de ciclo (STOP)
    DigitalSensor      estopAux_;             ///< Contacto auxiliar de parada de emergencia
    DigitalSensor      btnReset_;             ///< Pulsador de rearme de alarmas (RESET)

    // Actuadores y controladores de lazo local
    BottleGate         fillGate_;             ///< Compuerta de retención de llenado
    BottleGate         capGate_;              ///< Compuerta de retención de tapado
    StepperAxis        fillZ_;                ///< Eje vertical de boquilla de llenado
    StepperAxis        capZ_;                 ///< Eje vertical de cabezal de tapado
    FillPump           fillPump_;             ///< Bomba de dosificación con temporizador de seguridad
    DcMotorBts7960     chuckMotor_;           ///< Motor DC del cabezal de apriete
    ConveyorController conveyor_;             ///< Control de rampa y supervisión de corriente de la banda
    AlarmBuzzer        buzzer_;               ///< Señalizador acústico de alarmas

    // Autómatas de proceso
    FillingAutomaton   fillingAutomaton_;     ///< Máquina de estados de la etapa de llenado
    CappingAutomaton   cappingAutomaton_;     ///< Máquina de estados de la etapa de tapado

    // Estado global del mediador
    StationMode        mode_;                 ///< Modo operativo activo
    ErrorCode          activeError_;          ///< Código de fallo crítico retenido
    BottleProfile      profile_;              ///< Perfil de envase activo (Small / Large)
    uint8_t            interstageQueueCount_; ///< Botellas en tránsito entre etapas [unidades]
    uint32_t           bottleCount_;          ///< Conteo total de botellas terminadas [unidades]
    uint32_t           bootStartTimeMs_;      ///< Marca de tiempo de inicio de secuencia de origen [ms]
    bool               linePaused_;           ///< Estado de pausa temporal solicitado por STOP
    bool               networkWarningActive_; ///< Estado de pérdida de enlace de telemetría
    uint16_t           testFlags_;            ///< Máscara de actuadores encendidos manualmente en TestMode
    DisplaySnapshot    lastSnapshot_;         ///< Última instantánea entregada a DisplayManager
    bool               snapshotDirty_;        ///< Indica que hay cambios pendientes de proyectar en la pantalla TFT
};
