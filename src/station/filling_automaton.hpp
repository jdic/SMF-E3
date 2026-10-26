/**
 * @file filling_automaton.hpp
 * @brief Máquina de estados finitos (FSM) de tres fases para el Autómata 1 (Llenado).
 *
 * Implementa el patrón Enum FSM con ciclo de vida explícito (Entry, During, Exit),
 * vigilancia de tiempos de guardia (timeouts), control de derrames en fase Exit
 * y retención de botella por cola intermedia llena (WaitingQueueSlot).
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "bottle_gate.hpp"
#include "stepper_axis.hpp"
#include "fill_pump.hpp"

/**
 * @class FillingAutomaton
 * @brief Coordina la retención de botella, descenso de boquilla, dosificación y liberación en la etapa de llenado.
 */
class FillingAutomaton
{
public:
    /**
     * @brief Construye la máquina de estados del autómata de llenado inyectando sus periféricos HAL.
     *
     * @param[in] presenceSensor Sensor óptico de presencia de botella en la zona de llenado.
     * @param[in] limitSwitch Fin de carrera superior del eje Z de llenado.
     * @param[in] gate Compuerta de doble servomotor (brazo de tope y brazo de sujeción) de llenado.
     * @param[in] zAxis Eje vertical paso a paso de la boquilla de llenado.
     * @param[in] pump Controlador de la bomba de llenado con corte autónomo de seguridad.
     */
    FillingAutomaton(
        DigitalSensor& presenceSensor,
        DigitalSensor& limitSwitch,
        BottleGate& gate,
        StepperAxis& zAxis,
        FillPump& pump);

    /**
     * @brief Reinicia la FSM al estado Idle, apaga la bomba y limpia banderas de error y eventos.
     *
     * Función no bloqueante. Invocada por StationController al iniciar Boot o tras confirmar despeje.
     */
    void reset();

    /**
     * @brief Detiene inmediatamente la bomba y el eje Z ante una parada de emergencia o fallo global.
     *
     * Conserva la posición actual de los servomotores para no liberar bruscamente un envase a medio proceso.
     */
    void stopImmediate();

    /**
     * @brief Ejecuta un ciclo de la máquina de estados de tres fases (Exit -> Entry -> During).
     *
     * Función no bloqueante. Debe invocarse periódicamente desde StationController en modo Running.
     */
    void update();

    /**
     * @brief Selecciona el perfil de botella activo cuyos parámetros (fillZSteps, fillMs) aplicará el ciclo.
     *
     * @param[in] profile Formato de botella activo (Small o Large).
     */
    void setProfile(BottleProfile profile);

    /**
     * @brief Autoriza o retiene el paso del estado WaitingQueueSlot a ReleasingBottle según la cola intermedia.
     *
     * @param[in] canRelease true si interstageQueueCount_ < MAX_INTERSTAGE_QUEUE; false si la cola está llena.
     */
    void setReleasePermitted(bool canRelease);

    /**
     * @brief Consume el evento de liberación de botella hacia el tramo intermedio de la banda.
     *
     * @return true una única vez cada vez que el autómata entra en ReleasingBottle; false en caso contrario.
     */
    bool consumeBottleReleasedEvent();

    /**
     * @brief Consulta el estado operativo actual del autómata de llenado.
     * @return Fase activa dentro de la enumeración FillState.
     */
    FillState getState() const;

    /**
     * @brief Verifica si el autómata se encuentra en reposo esperando una nueva botella.
     * @return true si el estado actual es FillState::Idle y no tiene error activo.
     */
    bool isIdle() const;

    /**
     * @brief Obtiene el código de error disparado por el autómata (si ocurrió alguno).
     * @return ErrorCode::None en operación nominal, o el código de fallo serie 100/200 detectado.
     */
    ErrorCode getError() const;

private:
    /**
     * @brief Acciones ejecutadas una sola vez al entrar a un nuevo estado.
     * @param[in] state Estado al que acaba de ingresar el autómata.
     */
    void onEntry(FillState state);

    /**
     * @brief Acciones y evaluación de transiciones/timeouts ejecutadas en cada ciclo del estado activo.
     * @param[in] state Estado actualmente activo.
     */
    void onDuring(FillState state);

    /**
     * @brief Acciones de limpieza ejecutadas una sola vez inmediatamente antes de abandonar un estado.
     * @param[in] state Estado que se está abandonando.
     */
    void onExit(FillState state);

    /**
     * @brief Registra un fallo crítico en el autómata y detiene de forma segura la bomba y el eje Z.
     * @param[in] error Código de error detectado.
     */
    void triggerFault(ErrorCode error);

    DigitalSensor& presenceSensor_;      ///< Sensor óptico de presencia en la estación de llenado
    DigitalSensor& limitSwitch_;         ///< Fin de carrera superior del eje Z de llenado
    BottleGate&    gate_;                ///< Par de servomotores (tope y sujeción) de llenado
    StepperAxis&   zAxis_;               ///< Actuador lineal paso a paso de la boquilla
    FillPump&      pump_;                ///< Bomba peristáltica / de diafragma de llenado

    FillState      currentState_;        ///< Estado activo en el ciclo actual
    FillState      nextState_;           ///< Próximo estado solicitado por una transición
    FillState      previousState_;       ///< Estado anterior para trazabilidad de transiciones
    BottleProfile  profile_;             ///< Perfil de botella activo (Small / Large)
    ErrorCode      activeError_;         ///< Código de fallo retenido (ErrorCode::None si opera normal)
    uint32_t       stageTimerMs_;        ///< Marca de tiempo millis() de inicio de la fase actual [ms]
    bool           isFirstRun_;          ///< Bandera para disparar onEntry() en el primer ciclo del estado
    bool           needsClear_;          ///< Impide re-disparo en Idle hasta que la botella saliente despeje el sensor
    bool           canReleaseBottle_;    ///< Habilitación desde StationController cuando hay cupo en la cola intermedia
    bool           bottleReleasedEvent_; ///< Pulso de evento para incrementar interstageQueueCount_ en el mediador
};
