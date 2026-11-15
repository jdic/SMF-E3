/**
 * @file capping_automaton.hpp
 * @brief Máquina de estados para el proceso de colocación y apriete de tapas.
 */

#pragma once
#include <Arduino.h>
#include "station_automaton_base.hpp"
#include "dc_motor_bts7960.hpp"

/**
 * @class CappingAutomaton
 * @brief Coordina la retención de botella con doble sensor IR, descenso del cabezal, apriete de tapa y liberación final.
 */
class CappingAutomaton : public StationAutomatonBase<CappingState>
{
public:
    CappingAutomaton(
        DigitalSensor& presenceSensor,
        DigitalSensor& positionSensor,
        DigitalSensor& limitSwitch,
        BottleGate& gate,
        StepperAxis& zAxis,
        DcMotorBts7960& chuckMotor);

    void reset();
    void stopImmediate();
    void update();

    bool consumeBottleCompletedEvent();

private:
    void onEntry(CappingState state);
    void onDuring(CappingState state);
    void onExit(CappingState state);
    void triggerFault(ErrorCode error);

    DcMotorBts7960& chuckMotor_;    ///< Puente H BTS7960 del motor DC de apriete
    bool            engageStarted_; ///< Indica si ya se activó el motor de roscado y la bajada lenta en LoweringHead
};
