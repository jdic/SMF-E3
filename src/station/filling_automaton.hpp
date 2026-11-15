/**
 * @file filling_automaton.hpp
 * @brief Máquina de estados para el proceso de dosificación y llenado de envases.
 */

#pragma once
#include <Arduino.h>
#include "station_automaton_base.hpp"
#include "fill_pump.hpp"

/**
 * @class FillingAutomaton
 * @brief Coordina la retención de botella con doble sensor IR, descenso de boquilla, dosificación y liberación.
 */
class FillingAutomaton : public StationAutomatonBase<FillState>
{
public:
    FillingAutomaton(
        DigitalSensor& presenceSensor,
        DigitalSensor& positionSensor,
        DigitalSensor& limitSwitch,
        BottleGate& gate,
        StepperAxis& zAxis,
        FillPump& pump);

    void reset();
    void stopImmediate();
    void update();

    void setReleasePermitted(bool canRelease);
    bool consumeBottleReleasedEvent();

private:
    void onEntry(FillState state);
    void onDuring(FillState state);
    void onExit(FillState state);
    void triggerFault(ErrorCode error);

    FillPump& pump_;             ///< Bomba peristáltica / de diafragma de llenado
    bool      canReleaseBottle_; ///< Habilitación desde StationController cuando hay cupo en la cola intermedia
};
