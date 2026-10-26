/**
 * @file capping_automaton.hpp
 * @brief Máquina de estados finitos (FSM) de tres fases para el Autómata 2 (Tapado).
 *
 * Implementa el patrón Enum FSM con ciclo de vida explícito (Entry, During, Exit),
 * supervisión de sobrecorriente (ERR-301) y par de bloqueo prematuro por corriente (ERR-302)
 * en el puente H BTS7960 durante el apriete, y notificación de botella completada.
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "bottle_gate.hpp"
#include "stepper_axis.hpp"
#include "dc_motor_bts7960.hpp"

/**
 * @class CappingAutomaton
 * @brief Coordina la retención de botella, descenso del cabezal, apriete de tapa y liberación final.
 */
class CappingAutomaton
{
public:
    /**
     * @brief Construye la máquina de estados del autómata de tapado inyectando sus periféricos HAL.
     *
     * @param[in] presenceSensor Sensor óptico de presencia de botella en la zona de tapado.
     * @param[in] limitSwitch Fin de carrera superior del eje Z de tapado.
     * @param[in] gate Compuerta de doble servomotor (brazo de tope y brazo de sujeción) de tapado.
     * @param[in] zAxis Eje vertical paso a paso del cabezal de tapado.
     * @param[in] chuckMotor Puente H BTS7960 del motor DC del cabezal de apriete con sensado IS.
     */
    CappingAutomaton(
        DigitalSensor& presenceSensor,
        DigitalSensor& limitSwitch,
        BottleGate& gate,
        StepperAxis& zAxis,
        DcMotorBts7960& chuckMotor);

    /**
     * @brief Reinicia la FSM al estado Idle, detiene el cabezal y limpia banderas de fallo y eventos.
     *
     * Función no bloqueante. Invocada por StationController al iniciar Boot o tras confirmar despeje.
     */
    void reset();

    /**
     * @brief Detiene inmediatamente el motor DC de apriete y el eje Z ante una parada de emergencia o fallo global.
     *
     * Conserva la posición actual de los servomotores para no soltar bruscamente un envase a medio proceso.
     */
    void stopImmediate();

    /**
     * @brief Ejecuta un ciclo de la máquina de estados de tres fases (Exit -> Entry -> During).
     *
     * Función no bloqueante. Debe invocarse periódicamente desde StationController en modo Running.
     */
    void update();

    /**
     * @brief Selecciona el perfil de botella activo cuya cota vertical (capZSteps) aplicará el ciclo.
     *
     * @param[in] profile Formato de botella activo (Small o Large).
     */
    void setProfile(BottleProfile profile);

    /**
     * @brief Consume el evento de botella tapada y en proceso de liberación.
     *
     * @return true una única vez cada vez que el autómata entra en ReleasingBottle; false en caso contrario.
     */
    bool consumeBottleCompletedEvent();

    /**
     * @brief Consulta el estado operativo actual del autómata de tapado.
     * @return Fase activa dentro de la enumeración CappingState.
     */
    CappingState getState() const;

    /**
     * @brief Verifica si el autómata se encuentra en reposo esperando una nueva botella.
     * @return true si el estado actual es CappingState::Idle y no tiene error activo.
     */
    bool isIdle() const;

    /**
     * @brief Obtiene el código de error disparado por el autómata (si ocurrió alguno).
     * @return ErrorCode::None en operación nominal, o el código de fallo serie 100/200/300 detectado.
     */
    ErrorCode getError() const;

private:
    /**
     * @brief Acciones ejecutadas una sola vez al entrar a un nuevo estado.
     * @param[in] state Estado al que acaba de ingresar el autómata.
     */
    void onEntry(CappingState state);

    /**
     * @brief Acciones y evaluación de transiciones/timeouts ejecutadas en cada ciclo del estado activo.
     * @param[in] state Estado actualmente activo.
     */
    void onDuring(CappingState state);

    /**
     * @brief Acciones de limpieza ejecutadas una sola vez inmediatamente antes de abandonar un estado.
     * @param[in] state Estado que se está abandonando.
     */
    void onExit(CappingState state);

    /**
     * @brief Registra un fallo crítico en el autómata y detiene de forma segura el cabezal DC y el eje Z.
     * @param[in] error Código de error detectado.
     */
    void triggerFault(ErrorCode error);

    DigitalSensor&  presenceSensor_;       ///< Sensor óptico de presencia en la estación de tapado
    DigitalSensor&  limitSwitch_;          ///< Fin de carrera superior del eje Z de tapado
    BottleGate&     gate_;                 ///< Par de servomotores (tope y sujeción) de tapado
    StepperAxis&    zAxis_;                ///< Actuador lineal paso a paso del cabezal
    DcMotorBts7960& chuckMotor_;           ///< Puente H BTS7960 del motor DC de apriete

    CappingState    currentState_;         ///< Estado activo en el ciclo actual
    CappingState    nextState_;            ///< Próximo estado solicitado por una transición
    CappingState    previousState_;        ///< Estado anterior para trazabilidad de transiciones
    BottleProfile   profile_;              ///< Perfil de botella activo (Small / Large)
    ErrorCode       activeError_;          ///< Código de fallo retenido (ErrorCode::None si opera normal)
    uint32_t        stageTimerMs_;         ///< Marca de tiempo millis() de inicio de la fase actual [ms]
    bool            isFirstRun_;           ///< Bandera para disparar onEntry() en el primer ciclo del estado
    bool            needsClear_;           ///< Impide re-disparo en Idle hasta que la botella saliente despeje el sensor
    bool            bottleCompletedEvent_; ///< Pulso de evento para incrementar bottleCount y descontar cola intermedia
};
