/**
 * @file station_hardware_factory.hpp
 * @brief Constructores de configuración HAL y utilidades de torreta para StationController.
 */

#pragma once
#include <Arduino.h>
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"
#include "bottle_gate.hpp"
#include "stepper_axis.hpp"
#include "conveyor_controller.hpp"

namespace StationHardwareFactory
{
    inline BottleGateConfig makeFillGateConfig()
    {
        BottleGateConfig cfg{};
        cfg.stopPin      = HardwarePins::PIN_SERVO_FILL_STOP;
        cfg.clampPin     = HardwarePins::PIN_SERVO_FILL_CLAMP;
        cfg.stopUpDeg    = ProcessParams::FILL_STOP_UP_DEG;
        cfg.stopDownDeg  = ProcessParams::FILL_STOP_DOWN_DEG;
        cfg.clampUpDeg   = ProcessParams::FILL_CLAMP_UP_DEG;
        cfg.clampDownDeg = ProcessParams::FILL_CLAMP_DOWN_DEG;
        cfg.stopReverse  = (ProcessParams::FILL_STOP_DIR  == ProcessParams::ServoDir::CCW);
        cfg.clampReverse = (ProcessParams::FILL_CLAMP_DIR == ProcessParams::ServoDir::CCW);
        cfg.armMoveMs    = ProcessParams::ARM_MOVE_MS;
        cfg.releaseGapMs = ProcessParams::RELEASE_GAP_MS;
        return cfg;
    }

    inline BottleGateConfig makeCapGateConfig()
    {
        BottleGateConfig cfg{};
        cfg.stopPin      = HardwarePins::PIN_SERVO_CAP_STOP;
        cfg.clampPin     = HardwarePins::PIN_SERVO_CAP_CLAMP;
        cfg.stopUpDeg    = ProcessParams::CAP_STOP_UP_DEG;
        cfg.stopDownDeg  = ProcessParams::CAP_STOP_DOWN_DEG;
        cfg.clampUpDeg   = ProcessParams::CAP_CLAMP_UP_DEG;
        cfg.clampDownDeg = ProcessParams::CAP_CLAMP_DOWN_DEG;
        cfg.stopReverse  = (ProcessParams::CAP_STOP_DIR  == ProcessParams::ServoDir::CCW);
        cfg.clampReverse = (ProcessParams::CAP_CLAMP_DIR == ProcessParams::ServoDir::CCW);
        cfg.armMoveMs    = ProcessParams::ARM_MOVE_MS;
        cfg.releaseGapMs = ProcessParams::RELEASE_GAP_MS;
        return cfg;
    }

    inline StepperAxisConfig makeFillZConfig()
    {
        StepperAxisConfig cfg{};
        cfg.slot                 = StepperSlot::FillZ;
        cfg.stepPin              = HardwarePins::PIN_STEPPER_FILL_STEP;
        cfg.dirPin               = HardwarePins::PIN_STEPPER_FILL_DIR;
        cfg.enPin                = HardwarePins::PIN_STEPPER_FILL_EN;
        cfg.limitSwitchPin       = HardwarePins::PIN_LIMIT_SWITCH_FILL;
        cfg.maxTravelSteps       = ProcessParams::FILL_Z_MAX_TRAVEL_STEPS;
        cfg.accelStepsPerSec2    = ProcessParams::Z_ACCEL;
        cfg.homeBackoffSteps     = ProcessParams::Z_HOME_BACKOFF_STEPS;
        cfg.dirDownHigh          = (ProcessParams::FILL_Z_DOWN_DIR == ProcessParams::StepperDir::CW);
        cfg.enableActiveLow      = true;
        cfg.limitSwitchActiveLow = true;
        return cfg;
    }

    inline StepperAxisConfig makeCapZConfig()
    {
        StepperAxisConfig cfg{};
        cfg.slot                 = StepperSlot::CapZ;
        cfg.stepPin              = HardwarePins::PIN_STEPPER_CAP_STEP;
        cfg.dirPin               = HardwarePins::PIN_STEPPER_CAP_DIR;
        cfg.enPin                = HardwarePins::PIN_STEPPER_CAP_EN;
        cfg.limitSwitchPin       = HardwarePins::PIN_LIMIT_SWITCH_CAP;
        cfg.maxTravelSteps       = ProcessParams::CAP_Z_MAX_TRAVEL_STEPS;
        cfg.accelStepsPerSec2    = ProcessParams::Z_ACCEL;
        cfg.homeBackoffSteps     = ProcessParams::Z_HOME_BACKOFF_STEPS;
        cfg.dirDownHigh          = (ProcessParams::CAP_Z_DOWN_DIR == ProcessParams::StepperDir::CW);
        cfg.enableActiveLow      = true;
        cfg.limitSwitchActiveLow = true;
        return cfg;
    }

    inline ConveyorConfig makeConveyorConfig()
    {
        ConveyorConfig cfg{};
        cfg.rpwmPin            = HardwarePins::PIN_BTS_CONV_RPWM;
        cfg.enPin              = HardwarePins::PIN_BTS_CONV_EN;
        cfg.currentSensePin    = HardwarePins::PIN_ADC_CONV_CURRENT;
        cfg.targetPwm          = ProcessParams::BELT_PWM;
        cfg.maxBoostPwm        = ProcessParams::BELT_MAX_BOOST_PWM;
        cfg.enableIrComp       = ProcessParams::ENABLE_BELT_IR_COMP;
        cfg.idleCurrentAdc     = ProcessParams::BELT_IDLE_CURRENT_ADC;
        cfg.fullLoadCurrentAdc = ProcessParams::BELT_FULL_LOAD_ADC;
        cfg.accelStepPwm       = ProcessParams::BELT_ACCEL_PWM_STEP;
        cfg.decelStepPwm       = ProcessParams::BELT_DECEL_PWM_STEP;
        cfg.enableStallProt    = ProcessParams::ENABLE_BELT_STALL_PROT;
        cfg.currentLimitAdc    = ProcessParams::BELT_CURRENT_LIMIT_ADC;
        cfg.stallTimeMs        = ProcessParams::BELT_STALL_MS;
        return cfg;
    }

    inline void writeTowerRelayPin(uint8_t pin, bool turnOn)
    {
        const bool levelHigh = ProcessParams::TOWER_RELAY_ACTIVE_LOW ? !turnOn : turnOn;
        digitalWrite(pin, levelHigh ? HIGH : LOW);
    }

    inline void setTowerLights(bool greenOn, bool redOn)
    {
        writeTowerRelayPin(HardwarePins::PIN_TOWER_GREEN, greenOn);
        writeTowerRelayPin(HardwarePins::PIN_TOWER_RED, redOn);
    }
}
