/**
 * @file station_automaton_base.hpp
 * @brief Plantilla base (CRTP / Template Method sin sobrecarga de vtable) para los autómatas de estación.
 *
 * Encapsula el motor FSM de tres fases (Exit -> Entry -> During), los sensores ópticos de
 * aproximación y posición, la compuerta BottleGate, el eje StepperAxis y las transiciones
 * comunes (Idle, StopArmDown, ClampArmDown, RaisingZ y ReleasingBottle).
 */

#pragma once
#include <Arduino.h>
#include "config/process_params.hpp"
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "bottle_gate.hpp"
#include "stepper_axis.hpp"

/**
 * @class StationAutomatonBase
 * @brief Base parametrizada por la enumeración de estados (FillState o CappingState).
 * @tparam StateEnum Enumeración de estados del autómata (debe definir Idle, StopArmDown, ClampArmDown, ReleasingBottle).
 */
template <typename StateEnum>
class StationAutomatonBase
{
public:
    StationAutomatonBase(
        DigitalSensor& presenceSensor,
        DigitalSensor& positionSensor,
        DigitalSensor& limitSwitch,
        BottleGate& gate,
        StepperAxis& zAxis)
        : presenceSensor_(presenceSensor),
          positionSensor_(positionSensor),
          limitSwitch_(limitSwitch),
          gate_(gate),
          zAxis_(zAxis),
          currentState_(StateEnum::Idle),
          nextState_(StateEnum::Idle),
          previousState_(StateEnum::Idle),
          profile_(BottleProfile::Small),
          activeError_(ErrorCode::None),
          stageTimerMs_(0UL),
          lastUpdateMs_(0UL),
          isFirstRun_(true),
          needsClear_(false),
          bottleEvent_(false),
          beltPaused_(false)
    {
    }

    /**
     * @brief Selecciona el perfil de botella activo (Small o Large).
     */
    void setProfile(BottleProfile profile)
    {
        profile_ = profile;
    }

    /**
     * @brief Consulta el estado operativo actual del autómata.
     */
    StateEnum getState() const
    {
        return currentState_;
    }

    /**
     * @brief Verifica si el autómata se encuentra en reposo esperando una nueva botella y sin error.
     */
    bool isIdle() const
    {
        return (currentState_ == StateEnum::Idle) && (activeError_ == ErrorCode::None);
    }

    /**
     * @brief Indica si el autómata tiene una botella posicionada y retenida (requiere pausar la banda).
     *
     * La banda sólo puede avanzar en Idle, en StopArmDown (llevando la botella hacia el 2do sensor)
     * o en ReleasingBottle una vez que el brazo de tope terminó de subir.
     */
    bool isHoldingBottle() const
    {
        return ((currentState_ != StateEnum::Idle) &&
                (currentState_ != StateEnum::StopArmDown) &&
                (currentState_ != StateEnum::ReleasingBottle)) ||
               ((currentState_ == StateEnum::ReleasingBottle) && gate_.isWaitingToClearStop());
    }

    /**
     * @brief Obtiene el código de error disparado por el autómata (ErrorCode::None si opera normal).
     */
    ErrorCode getError() const
    {
        return activeError_;
    }

    /**
     * @brief Informa al autómata si la banda transportadora está pausada externamente.
     */
    void setBeltPaused(bool paused)
    {
        beltPaused_ = paused;
    }

protected:
    /**
     * @brief Restablece los campos comunes de la máquina de estados al estado Idle.
     */
    void resetBaseState()
    {
        zAxis_.stop();
        zAxis_.clearSoftLimitFault();
        gate_.raiseArmsImmediate();

        currentState_  = StateEnum::Idle;
        nextState_     = StateEnum::Idle;
        previousState_ = StateEnum::Idle;
        activeError_   = ErrorCode::None;
        stageTimerMs_  = millis();
        lastUpdateMs_  = stageTimerMs_;
        isFirstRun_    = true;
        needsClear_    = false;
        bottleEvent_   = false;
        beltPaused_    = false;
    }

    /**
     * @brief Consume y limpia el pulso de evento de botella liberada/completada.
     */
    bool consumeBottleEvent()
    {
        const bool occurred = bottleEvent_;
        bottleEvent_ = false;
        return occurred;
    }

    /**
     * @brief Libera el candado de re-disparo cuando el sensor de aproximación queda despejado.
     */
    void updateClearLock()
    {
        if (needsClear_ && !presenceSensor_.isTriggered())
        {
            needsClear_ = false;
        }
    }

    /**
     * @brief Congela el temporizador de fases dependientes de movimiento cuando la banda está pausada.
     */
    void updateBeltDependentTimer(bool extraBeltDependency = false)
    {
        const uint32_t nowMs = millis();
        const uint32_t dtMs  = nowMs - lastUpdateMs_;
        lastUpdateMs_ = nowMs;

        const bool dependsOnBelt =
            extraBeltDependency ||
            (currentState_ == StateEnum::StopArmDown) ||
            ((currentState_ == StateEnum::ReleasingBottle) && !gate_.isWaitingToClearStop());

        if (beltPaused_ && dependsOnBelt && !isFirstRun_)
        {
            stageTimerMs_ += dtMs;
        }
    }

    /**
     * @brief Evalúa la transición común desde Idle al detectar una nueva botella.
     */
    void evalIdleTransition()
    {
        if (presenceSensor_.isTriggered() && !needsClear_)
        {
            needsClear_ = true;
            nextState_  = StateEnum::StopArmDown;
        }
    }

    /**
     * @brief Evalúa la fase común StopArmDown: espera el 2do sensor de posición o dispara timeout.
     */
    bool evalStopArmDown(uint32_t elapsedMs)
    {
        if (positionSensor_.isTriggered())
        {
            nextState_ = StateEnum::ClampArmDown;
            return true;
        }
        if (elapsedMs >= ProcessParams::CLAMP_SENSOR_TIMEOUT_MS)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief Evalúa la fase común ClampArmDown con ventana de tolerancia BOTTLE_LOST_GRACE_MS.
     * @return false si expiró la ventana de gracia sin detectar botella (debe dispararse fallo).
     */
    bool evalClampArmDown(uint32_t elapsedMs, StateEnum nextLoweringState, bool lostErrEnabled)
    {
        if (elapsedMs >= ProcessParams::ARM_MOVE_MS)
        {
            if (positionSensor_.isTriggered() || !lostErrEnabled)
            {
                nextState_ = nextLoweringState;
                return true;
            }
            if (elapsedMs >= (ProcessParams::ARM_MOVE_MS + ProcessParams::BOTTLE_LOST_GRACE_MS))
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Evalúa la fase común ReleasingBottle tras cumplirse RELEASE_MS y despejar el sensor de posición.
     */
    void evalReleasingBottle(uint32_t elapsedMs)
    {
        if ((elapsedMs >= ProcessParams::RELEASE_MS) && !positionSensor_.isTriggered())
        {
            nextState_ = StateEnum::Idle;
        }
    }

    DigitalSensor& presenceSensor_; ///< 1er sensor IR (aproximación antes de los brazos)
    DigitalSensor& positionSensor_; ///< 2do sensor IR (posición exacta en medio de ambos brazos)
    DigitalSensor& limitSwitch_;    ///< Fin de carrera superior del eje Z
    BottleGate&    gate_;           ///< Par de servomotores (tope y sujeción)
    StepperAxis&   zAxis_;          ///< Actuador lineal paso a paso del eje Z

    StateEnum      currentState_;   ///< Estado activo en el ciclo actual
    StateEnum      nextState_;      ///< Próximo estado solicitado por una transición
    StateEnum      previousState_;  ///< Estado anterior para trazabilidad de transiciones
    BottleProfile  profile_;        ///< Perfil de botella activo (Small / Large)
    ErrorCode      activeError_;    ///< Código de fallo retenido (ErrorCode::None si opera normal)
    uint32_t       stageTimerMs_;   ///< Marca de tiempo millis() de inicio de la fase actual [ms]
    uint32_t       lastUpdateMs_;   ///< Marca de tiempo del último ciclo update() [ms]
    bool           isFirstRun_;     ///< Bandera para disparar onEntry() en el primer ciclo del estado
    bool           needsClear_;     ///< Impide re-disparo en Idle hasta que la botella saliente despeje los sensores
    bool           bottleEvent_;    ///< Pulso de evento al iniciar ReleasingBottle
    bool           beltPaused_;     ///< Indica si la banda transportadora se encuentra pausada externamente
};
